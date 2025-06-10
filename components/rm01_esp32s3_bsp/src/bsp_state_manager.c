/**
 * @file bsp_state_manager.c
 * @brief BSP系统状态管理器实现 - 纯状态检测与管理
 * 
 * 专注于系统状态的检测、分析和管理，不涉及显示逻辑
 * 提供状态变化通知机制，供显示控制器使用
 */

#include "bsp_state_manager.h"
#include "network_monitor.h"
#include "bsp_power.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <inttypes.h>
#include <string.h>

static const char *TAG = "BSP_STATE_MGR";

// 状态变化回调管理
#define MAX_CALLBACKS 5
typedef struct {
    state_change_callback_t callback;
    void* user_data;
    bool active;
} callback_entry_t;

// 系统状态管理器结构
typedef struct {
    system_state_t current_state;
    system_state_t previous_state;
    uint32_t state_change_count;
    uint32_t state_start_time;
    bool monitoring_active;
    TaskHandle_t monitor_task_handle;
    SemaphoreHandle_t state_mutex;
    callback_entry_t callbacks[MAX_CALLBACKS];
} bsp_state_manager_t;

// 全局状态管理器实例
static bsp_state_manager_t s_manager = {0};

// 状态名称映射
static const char* STATE_NAMES[] = {
    "待机状态",
    "启动状态0",
    "启动状态1", 
    "启动状态2",
    "启动状态3",
    "高温状态1",
    "高温状态2", 
    "用户主机未连接",
    "高负荷计算状态",
    "GPU高使用率状态",
    "内存高使用率状态"
};

// 静态函数声明
static void state_monitor_task(void *pvParameters);
static system_state_t determine_system_state(void);
static esp_err_t set_system_state(system_state_t new_state);
static bool is_high_compute_load(void);
static uint32_t get_time_seconds(void);
static void notify_state_change(system_state_t old_state, system_state_t new_state);

// ========== 核心接口实现 ==========

esp_err_t bsp_state_manager_init(void) {
    ESP_LOGI(TAG, "初始化BSP系统状态管理器");
    
    // 创建互斥锁
    s_manager.state_mutex = xSemaphoreCreateMutex();
    if (s_manager.state_mutex == NULL) {
        ESP_LOGE(TAG, "创建状态互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    // 初始化状态
    s_manager.current_state = SYSTEM_STATE_STANDBY;
    s_manager.previous_state = SYSTEM_STATE_STANDBY;
    s_manager.state_change_count = 0;
    s_manager.state_start_time = get_time_seconds();
    s_manager.monitoring_active = false;
    s_manager.monitor_task_handle = NULL;
    
    // 初始化回调数组
    memset(s_manager.callbacks, 0, sizeof(s_manager.callbacks));
    
    ESP_LOGI(TAG, "BSP系统状态管理器初始化完成");
    return ESP_OK;
}

void bsp_state_manager_start_monitoring(void) {
    if (s_manager.monitoring_active) {
        ESP_LOGW(TAG, "BSP系统状态监控已在运行");
        return;
    }
    
    ESP_LOGI(TAG, "启动BSP系统状态监控");
    
    // 先设置标志位，确保任务不会立即退出
    s_manager.monitoring_active = true;
    
    // 创建监控任务
    BaseType_t ret = xTaskCreate(
        state_monitor_task,
        "bsp_state_monitor",
        4096,
        NULL,
        4,  // 中等优先级
        &s_manager.monitor_task_handle
    );
    
    if (ret == pdPASS) {
        ESP_LOGI(TAG, "BSP系统状态监控任务已启动");
    } else {
        s_manager.monitoring_active = false;  // 创建失败时重置标志位
        ESP_LOGE(TAG, "创建BSP系统状态监控任务失败");
    }
}

void bsp_state_manager_stop_monitoring(void) {
    if (!s_manager.monitoring_active) {
        ESP_LOGW(TAG, "BSP系统状态监控未运行");
        return;
    }
    
    ESP_LOGI(TAG, "停止BSP系统状态监控");
    
    s_manager.monitoring_active = false;
    
    if (s_manager.monitor_task_handle != NULL) {
        vTaskDelete(s_manager.monitor_task_handle);
        s_manager.monitor_task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "BSP系统状态监控已停止");
}

system_state_t bsp_state_manager_get_current_state(void) {
    system_state_t state = SYSTEM_STATE_STANDBY;
    
    if (xSemaphoreTake(s_manager.state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = s_manager.current_state;
        xSemaphoreGive(s_manager.state_mutex);
    }
    
    return state;
}

const char* bsp_state_manager_get_state_name(system_state_t state) {
    if (state >= 0 && state < SYSTEM_STATE_COUNT) {
        return STATE_NAMES[state];
    }
    return "未知状态";
}

esp_err_t bsp_state_manager_get_info(system_state_info_t* info) {
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 获取网络状态
    bool computing_connected = (nm_get_status(NM_COMPUTING_MODULE_IP) == NM_STATUS_UP);
    bool application_connected = (nm_get_status(NM_APPLICATION_MODULE_IP) == NM_STATUS_UP);
    bool user_host_connected = (nm_get_status(NM_USER_HOST_IP) == NM_STATUS_UP);
    
    // 获取温度（XSP16芯片不提供温度数据，设置为0）
    float temperature = 0.0f;
    // 注意：XSP16电源芯片不提供温度数据，未来可能需要从其他温度传感器获取
    
    // 填充信息结构
    if (xSemaphoreTake(s_manager.state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        info->current_state = s_manager.current_state;
        info->previous_state = s_manager.previous_state;
        info->state_change_count = s_manager.state_change_count;
        info->time_in_current_state = get_time_seconds() - s_manager.state_start_time;
        xSemaphoreGive(s_manager.state_mutex);
    }
    
    info->current_temperature = temperature;
    info->computing_module_connected = computing_connected;
    info->application_module_connected = application_connected;
    info->user_host_connected = user_host_connected;
    info->high_compute_load = is_high_compute_load();
    
    return ESP_OK;
}

esp_err_t bsp_state_manager_force_set_state(system_state_t state) {
    if (state >= SYSTEM_STATE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "强制设置BSP系统状态为: %s", bsp_state_manager_get_state_name(state));
    return set_system_state(state);
}

void bsp_state_manager_update_now(void) {
    ESP_LOGI(TAG, "手动更新BSP系统状态");
    
    // 确定新的系统状态
    system_state_t new_state = determine_system_state();
    
    // 如果状态发生变化，更新状态
    if (new_state != s_manager.current_state) {
        esp_err_t ret = set_system_state(new_state);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "BSP状态已更新");
        } else {
            ESP_LOGE(TAG, "BSP状态更新失败: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGI(TAG, "BSP状态无变化，保持当前状态: %s", bsp_state_manager_get_state_name(new_state));
    }
}

void bsp_state_manager_print_status(void) {
    system_state_info_t info;
    esp_err_t ret = bsp_state_manager_get_info(&info);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取BSP系统状态信息失败");
        return;
    }
    
    ESP_LOGI(TAG, "========== BSP系统状态管理器报告 ==========");
    ESP_LOGI(TAG, "当前状态: %s", bsp_state_manager_get_state_name(info.current_state));
    ESP_LOGI(TAG, "前一状态: %s", bsp_state_manager_get_state_name(info.previous_state));
    ESP_LOGI(TAG, "状态变化次数: %" PRIu32, info.state_change_count);
    ESP_LOGI(TAG, "在当前状态时间: %" PRIu32 " 秒", info.time_in_current_state);
    ESP_LOGI(TAG, "当前温度: %.1f°C", info.current_temperature);
    ESP_LOGI(TAG, "算力模组连接: %s", info.computing_module_connected ? "是" : "否");
    ESP_LOGI(TAG, "应用模组连接: %s", info.application_module_connected ? "是" : "否");
    ESP_LOGI(TAG, "用户主机连接: %s", info.user_host_connected ? "是" : "否");
    ESP_LOGI(TAG, "高负荷计算: %s", info.high_compute_load ? "是" : "否");
    ESP_LOGI(TAG, "监控状态: %s", s_manager.monitoring_active ? "运行中" : "已停止");
    ESP_LOGI(TAG, "=======================================");
}

// ========== 回调管理接口实现 ==========

esp_err_t bsp_state_manager_register_callback(state_change_callback_t callback, void* user_data) {
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 查找空闲槽位
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!s_manager.callbacks[i].active) {
            s_manager.callbacks[i].callback = callback;
            s_manager.callbacks[i].user_data = user_data;
            s_manager.callbacks[i].active = true;
            ESP_LOGI(TAG, "注册状态变化回调，槽位: %d", i);
            return ESP_OK;
        }
    }
    
    ESP_LOGE(TAG, "回调槽位已满，无法注册新回调");
    return ESP_ERR_NO_MEM;
}

esp_err_t bsp_state_manager_unregister_callback(state_change_callback_t callback) {
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 查找并移除回调
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_manager.callbacks[i].active && s_manager.callbacks[i].callback == callback) {
            s_manager.callbacks[i].active = false;
            s_manager.callbacks[i].callback = NULL;
            s_manager.callbacks[i].user_data = NULL;
            ESP_LOGI(TAG, "注销状态变化回调，槽位: %d", i);
            return ESP_OK;
        }
    }
    
    ESP_LOGW(TAG, "未找到要注销的回调函数");
    return ESP_ERR_NOT_FOUND;
}

// ========== 静态函数实现 ==========

static void state_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "BSP系统状态监控任务开始运行");
    
    while (s_manager.monitoring_active) {
        // 检测新的系统状态
        system_state_t new_state = determine_system_state();
        
        // 如果状态发生变化，更新状态
        if (new_state != s_manager.current_state) {
            set_system_state(new_state);
        }
        
        // 每2秒检查一次状态
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    ESP_LOGI(TAG, "BSP系统状态监控任务结束");
    
    // 清理任务句柄
    s_manager.monitor_task_handle = NULL;
    
    // 删除自身任务（FreeRTOS要求）
    vTaskDelete(NULL);
}

static system_state_t determine_system_state(void) {
    // 获取网络连接状态
    bool computing_connected = (nm_get_status(NM_COMPUTING_MODULE_IP) == NM_STATUS_UP);
    bool application_connected = (nm_get_status(NM_APPLICATION_MODULE_IP) == NM_STATUS_UP);
    bool user_host_connected = (nm_get_status(NM_USER_HOST_IP) == NM_STATUS_UP);
    
    // 获取本地电源芯片温度 (XSP16芯片不提供温度数据)
    float local_temperature = 0.0f;
    // 注意：XSP16电源芯片不提供温度数据，未来可能需要从其他温度传感器获取
    
    // 当前使用本地温度作为系统温度
    float system_temperature = local_temperature;
    
    ESP_LOGD(TAG, "BSP系统温度评估: 本地=%.1f°C, 系统=%.1f°C", 
             local_temperature, system_temperature);
    
    // 优先级1: 高温状态（最高优先级）
    if (system_temperature > TEMP_THRESHOLD_HIGH_2) {
        ESP_LOGW(TAG, "BSP检测到极高温度状态: %.1f°C > %.1f°C", 
                 system_temperature, TEMP_THRESHOLD_HIGH_2);
        return SYSTEM_STATE_HIGH_TEMP_2;
    } else if (system_temperature > TEMP_THRESHOLD_HIGH_1) {
        ESP_LOGW(TAG, "BSP检测到高温状态: %.1f°C > %.1f°C", 
                 system_temperature, TEMP_THRESHOLD_HIGH_1);
        return SYSTEM_STATE_HIGH_TEMP_1;
    }
    
    // 优先级2: 高负荷计算状态
    if (is_high_compute_load()) {
        return SYSTEM_STATE_HIGH_COMPUTE_LOAD;
    }
    
    // 优先级3: 用户主机未连接状态
    if (!user_host_connected) {
        return SYSTEM_STATE_USER_HOST_DISCONNECTED;
    }
    
    // 优先级4: 模组启动状态（基于算力模组和应用模组连接状态组合）
    if (!computing_connected && !application_connected) {
        return SYSTEM_STATE_STARTUP_0;  // 所有模组断开
    } else if (computing_connected && !application_connected) {
        return SYSTEM_STATE_STARTUP_1;  // 仅算力模组连接
    } else if (!computing_connected && application_connected) {
        return SYSTEM_STATE_STARTUP_2;  // 仅应用模组连接
    } else if (computing_connected && application_connected) {
        return SYSTEM_STATE_STARTUP_3;  // 算力+应用模组连接
    }
    
    // 默认: 待机状态
    return SYSTEM_STATE_STANDBY;
}

static esp_err_t set_system_state(system_state_t new_state) {
    if (new_state >= SYSTEM_STATE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_state_t old_state = s_manager.current_state;
    
    if (xSemaphoreTake(s_manager.state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // 更新状态
        s_manager.previous_state = s_manager.current_state;
        s_manager.current_state = new_state;
        s_manager.state_change_count++;
        s_manager.state_start_time = get_time_seconds();
        
        xSemaphoreGive(s_manager.state_mutex);
        
        // 记录状态变化
        ESP_LOGI(TAG, "BSP系统状态变化: [%s] -> [%s]", 
                 bsp_state_manager_get_state_name(old_state),
                 bsp_state_manager_get_state_name(new_state));
        
        // 通知所有注册的回调函数
        notify_state_change(old_state, new_state);
        
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

static bool is_high_compute_load(void) {
    // 当前临时实现：仅基于功耗判断（通过电源协商数据）
    const bsp_power_chip_data_t* power_data = bsp_get_latest_power_chip_data();
    if (power_data != NULL && power_data->valid) {
        // 如果功耗超过一定阈值（例如50W），认为是高负荷状态
        if (power_data->power > COMPUTE_POWER_THRESHOLD) {
            ESP_LOGD(TAG, "BSP检测到高功耗状态: %.2fW > %.1fW", 
                     power_data->power, COMPUTE_POWER_THRESHOLD);
            return true;
        }
    }
    
    // 未来扩展：通过HTTP客户端从网络模组获取详细负载状态
    // - 算力模组状态查询
    // - 应用模组状态查询  
    // - 综合负载评估算法
    
    return false;
}

static uint32_t get_time_seconds(void) {
    return xTaskGetTickCount() / configTICK_RATE_HZ;
}

static void notify_state_change(system_state_t old_state, system_state_t new_state) {
    // 通知所有注册的回调函数
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_manager.callbacks[i].active && s_manager.callbacks[i].callback != NULL) {
            ESP_LOGD(TAG, "通知状态变化回调，槽位: %d", i);
            s_manager.callbacks[i].callback(old_state, new_state, s_manager.callbacks[i].user_data);
        }
    }
}
