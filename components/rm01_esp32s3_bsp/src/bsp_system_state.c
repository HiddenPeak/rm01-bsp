/**
 * @file bsp_system_state.c
 * @brief 系统状态控制器实现 - BSP组件
 * 
 * 实现基于多种条件的复杂状态联动系统
 */

#include "bsp_system_state.h"
#include "network_monitor.h"
#include "bsp_power.h"
#include "bsp_ws2812.h"     // WS2812 LED控制
#include "led_animation.h"  // LED动画控制
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <string.h>

static const char *TAG = "BSP_SYS_STATE";

// 动画索引常量定义 (基于led_animation_export.c中的顺序)
#define ANIM_DEMO           0  // 示例动画
#define ANIM_STARTUP        1  // 启动中
#define ANIM_LINK_ERROR     2  // 链接错误
#define ANIM_HIGH_TEMP      3  // 高温警告
#define ANIM_COMPUTING      4  // 计算中
#define ANIM_EXAMPLE        ANIM_DEMO  // 向后兼容的别名

// 系统状态控制器结构
typedef struct {
    system_state_t current_state;
    system_state_t previous_state;
    uint32_t state_change_count;
    uint32_t state_start_time;
    bool monitoring_active;
    TaskHandle_t monitor_task_handle;
    SemaphoreHandle_t state_mutex;
} bsp_system_state_controller_t;

// 全局控制器实例
static bsp_system_state_controller_t s_controller = {0};

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
    "高负荷计算状态"
};

// 状态到动画的映射
static const int STATE_TO_ANIMATION[] = {
    ANIM_EXAMPLE,       // SYSTEM_STATE_STANDBY
    ANIM_STARTUP,       // SYSTEM_STATE_STARTUP_0
    ANIM_STARTUP,       // SYSTEM_STATE_STARTUP_1
    ANIM_STARTUP,       // SYSTEM_STATE_STARTUP_2
    ANIM_STARTUP,       // SYSTEM_STATE_STARTUP_3
    ANIM_HIGH_TEMP,     // SYSTEM_STATE_HIGH_TEMP_1
    ANIM_HIGH_TEMP,     // SYSTEM_STATE_HIGH_TEMP_2
    ANIM_LINK_ERROR,    // SYSTEM_STATE_USER_HOST_DISCONNECTED
    ANIM_COMPUTING      // SYSTEM_STATE_HIGH_COMPUTE_LOAD
};

// 静态函数声明
static void bsp_system_state_monitor_task(void *pvParameters);
static system_state_t determine_system_state(void);
static esp_err_t set_system_state(system_state_t new_state);
static bool is_high_compute_load(void);
static uint32_t get_time_seconds(void);

esp_err_t bsp_system_state_init(void) {
    ESP_LOGI(TAG, "初始化BSP系统状态控制器");
    
    // 创建互斥锁
    s_controller.state_mutex = xSemaphoreCreateMutex();
    if (s_controller.state_mutex == NULL) {
        ESP_LOGE(TAG, "创建状态互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    // 初始化状态
    s_controller.current_state = SYSTEM_STATE_STANDBY;
    s_controller.previous_state = SYSTEM_STATE_STANDBY;
    s_controller.state_change_count = 0;
    s_controller.state_start_time = get_time_seconds();
    s_controller.monitoring_active = false;
    s_controller.monitor_task_handle = NULL;
    
    ESP_LOGI(TAG, "BSP系统状态控制器初始化完成");
    return ESP_OK;
}

void bsp_system_state_start_monitoring(void) {
    if (s_controller.monitoring_active) {
        ESP_LOGW(TAG, "BSP系统状态监控已在运行");
        return;
    }
    
    ESP_LOGI(TAG, "启动BSP系统状态监控");
    
    // 先设置标志位，确保任务不会立即退出
    s_controller.monitoring_active = true;
    
    // 创建监控任务
    BaseType_t ret = xTaskCreate(
        bsp_system_state_monitor_task,
        "bsp_sys_state_monitor",
        4096,
        NULL,
        4,  // 中等优先级
        &s_controller.monitor_task_handle
    );
    
    if (ret == pdPASS) {
        ESP_LOGI(TAG, "BSP系统状态监控任务已启动");
    } else {
        s_controller.monitoring_active = false;  // 创建失败时重置标志位
        ESP_LOGE(TAG, "创建BSP系统状态监控任务失败");
    }
}

void bsp_system_state_stop(void) {
    if (!s_controller.monitoring_active) {
        ESP_LOGW(TAG, "BSP系统状态监控未运行");
        return;
    }
    
    ESP_LOGI(TAG, "停止BSP系统状态监控");
    
    s_controller.monitoring_active = false;
    
    if (s_controller.monitor_task_handle != NULL) {
        vTaskDelete(s_controller.monitor_task_handle);
        s_controller.monitor_task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "BSP系统状态监控已停止");
}

system_state_t bsp_system_state_get_current(void) {
    system_state_t state = SYSTEM_STATE_STANDBY;
    
    if (xSemaphoreTake(s_controller.state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = s_controller.current_state;
        xSemaphoreGive(s_controller.state_mutex);
    }
    
    return state;
}

const char* bsp_system_state_get_name(system_state_t state) {
    if (state >= 0 && state < SYSTEM_STATE_COUNT) {
        return STATE_NAMES[state];
    }
    return "未知状态";
}

esp_err_t bsp_system_state_force_set(system_state_t state) {
    if (state >= SYSTEM_STATE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "强制设置BSP系统状态为: %s", bsp_system_state_get_name(state));
    return set_system_state(state);
}

void bsp_system_state_print_status(void) {
    system_state_info_t info;
    esp_err_t ret = bsp_system_state_get_info(&info);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取BSP系统状态信息失败");
        return;
    }
    
    ESP_LOGI(TAG, "========== BSP系统状态报告 ==========");
    ESP_LOGI(TAG, "当前状态: %s", bsp_system_state_get_name(info.current_state));
    ESP_LOGI(TAG, "前一状态: %s", bsp_system_state_get_name(info.previous_state));
    ESP_LOGI(TAG, "状态变化次数: %" PRIu32, info.state_change_count);
    ESP_LOGI(TAG, "在当前状态时间: %" PRIu32 " 秒", info.time_in_current_state);
    ESP_LOGI(TAG, "当前温度: %.1f°C", info.current_temperature);
    ESP_LOGI(TAG, "算力模组连接: %s", info.computing_module_connected ? "是" : "否");
    ESP_LOGI(TAG, "应用模组连接: %s", info.application_module_connected ? "是" : "否");
    ESP_LOGI(TAG, "用户主机连接: %s", info.user_host_connected ? "是" : "否");
    ESP_LOGI(TAG, "高负荷计算: %s", info.high_compute_load ? "是" : "否");
    ESP_LOGI(TAG, "监控状态: %s", s_controller.monitoring_active ? "运行中" : "已停止");
    ESP_LOGI(TAG, "====================================");
}

void bsp_system_state_update_and_report(void) {
    ESP_LOGI(TAG, "手动更新BSP系统状态并生成报告");
    
    // 确定新的系统状态
    system_state_t new_state = determine_system_state();
    
    // 如果状态发生变化，更新状态
    if (new_state != s_controller.current_state) {
        esp_err_t ret = set_system_state(new_state);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "BSP状态已更新");
        } else {
            ESP_LOGE(TAG, "BSP状态更新失败: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGI(TAG, "BSP状态无变化，保持当前状态: %s", bsp_system_state_get_name(new_state));
    }
    
    // 打印状态报告
    bsp_system_state_print_status();
}

esp_err_t bsp_system_state_get_info(system_state_info_t* info) {
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
    if (xSemaphoreTake(s_controller.state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        info->current_state = s_controller.current_state;
        info->previous_state = s_controller.previous_state;
        info->state_change_count = s_controller.state_change_count;
        info->time_in_current_state = get_time_seconds() - s_controller.state_start_time;
        xSemaphoreGive(s_controller.state_mutex);
    }
    
    info->current_temperature = temperature;
    info->computing_module_connected = computing_connected;
    info->application_module_connected = application_connected;
    info->user_host_connected = user_host_connected;
    info->high_compute_load = is_high_compute_load();
    
    return ESP_OK;
}

// ====== 静态函数实现 ======

static void bsp_system_state_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "BSP系统状态监控任务开始运行");
    
    while (s_controller.monitoring_active) {
        // 检测新的系统状态
        system_state_t new_state = determine_system_state();
        
        // 如果状态发生变化，更新状态和动画
        if (new_state != s_controller.current_state) {
            set_system_state(new_state);
        }
        
        // 每2秒检查一次状态
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    ESP_LOGI(TAG, "BSP系统状态监控任务结束");
    
    // 清理任务句柄
    s_controller.monitor_task_handle = NULL;
    
    // 删除自身任务（FreeRTOS要求）
    vTaskDelete(NULL);
}

static system_state_t determine_system_state(void) {
    // 获取网络连接状态
    bool computing_connected = (nm_get_status(NM_COMPUTING_MODULE_IP) == NM_STATUS_UP);
    bool application_connected = (nm_get_status(NM_APPLICATION_MODULE_IP) == NM_STATUS_UP);
    bool user_host_connected = (nm_get_status(NM_USER_HOST_IP) == NM_STATUS_UP);
    
    // ========== TODO: 需要等待设备API启用后实现 ==========
    // 获取温度数据 - 需要从多个来源读取
    // 
    // 需要实现的功能：
    // 1. 从本地电源芯片读取温度 (已实现)
    // 2. 从算力模组读取温度数据
    //    - HTTP API: GET 10.10.99.98/api/temperature
    //    - 响应字段: cpu_temp, board_temp, ambient_temp
    // 3. 从应用模组读取温度数据  
    //    - HTTP API: GET 10.10.99.99/api/temperature
    //    - 响应字段: cpu_temp, board_temp, ambient_temp
    // 4. 综合温度评估算法
    //    - 取所有传感器的最高温度
    //    - 考虑不同组件的温度权重
    // ================================================
    
    // 获取本地电源芯片温度 (XSP16芯片不提供温度数据)
    float local_temperature = 0.0f;
    // 注意：XSP16电源芯片不提供温度数据，未来可能需要从其他温度传感器获取
    
    // TODO: 等待网络模组API启用后，添加以下功能：
    // - 从算力模组读取温度 (computing_temp)
    // - 从应用模组读取温度 (application_temp)  
    // - 实现最高温度评估: max_temp = max(local_temp, computing_temp, application_temp)
    
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
    
    system_state_t old_state = s_controller.current_state;
    
    if (xSemaphoreTake(s_controller.state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // 更新状态
        s_controller.previous_state = s_controller.current_state;
        s_controller.current_state = new_state;
        s_controller.state_change_count++;
        s_controller.state_start_time = get_time_seconds();
        
        xSemaphoreGive(s_controller.state_mutex);
        
        // 记录状态变化
        ESP_LOGI(TAG, "BSP系统状态变化: [%s] -> [%s]", 
                 bsp_system_state_get_name(old_state),
                 bsp_system_state_get_name(new_state));
        
        // ========== TODO: 需要等待动画文件更新后完善 ==========
        // 切换对应的动画
        // 
        // 当前状态：
        // - 高温状态动画已在 example_animation.json 中定义 ("高温警告")
        // - 计算状态动画已在 example_animation.json 中定义 ("计算中") 
        // - 但动画索引映射可能需要根据实际加载结果调整
        // =====================================================
        
        int animation_index = STATE_TO_ANIMATION[new_state];
        
        // 对于高温和计算状态，使用新添加的专用动画
        if (new_state == SYSTEM_STATE_HIGH_TEMP_1 || new_state == SYSTEM_STATE_HIGH_TEMP_2) {
            // 使用高温警告动画 (应该是 ANIM_HIGH_TEMP)
            animation_index = ANIM_HIGH_TEMP;
            ESP_LOGI(TAG, "BSP使用高温警告动画");
        } else if (new_state == SYSTEM_STATE_HIGH_COMPUTE_LOAD) {
            // 使用计算中动画 (应该是 ANIM_COMPUTING)
            animation_index = ANIM_COMPUTING;
            ESP_LOGI(TAG, "BSP使用计算负载动画");
        }
        
        esp_err_t ret = led_animation_select(animation_index);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "BSP已切换到动画索引 %d (%s)", animation_index, 
                     led_animation_get_name(animation_index));
        } else {
            ESP_LOGW(TAG, "BSP切换动画失败: %s", esp_err_to_name(ret));
        }
        
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

static bool is_high_compute_load(void) {
    // ========== TODO: 需要等待设备API启用后实现 ==========
    // 判断高负荷计算状态的逻辑
    // 
    // 需要实现的功能：
    // 1. 从算力模组 (10.10.99.98) 读取计算负载状态
    //    - HTTP API: GET /api/compute/status
    //    - 响应字段: cpu_usage, memory_usage, task_count, load_average
    // 
    // 2. 从应用模组 (10.10.99.99) 读取应用状态
    //    - HTTP API: GET /api/app/status  
    //    - 响应字段: active_processes, resource_usage
    //
    // 3. 综合判断标准：
    //    - CPU使用率 > 80%
    //    - 内存使用率 > 85%
    //    - 功耗 > 50W (已实现)
    //    - 活跃任务数 > 阈值
    // ================================================
    
    // 当前临时实现：仅基于功耗判断（通过电源协商数据）
    const bsp_power_chip_data_t* power_data = bsp_get_latest_power_chip_data();
    if (power_data != NULL && power_data->valid) {
        // 如果功耗超过一定阈值（例如50W），认为是高负荷状态
        if (power_data->power > 50.0f) {
            ESP_LOGD(TAG, "BSP检测到高功耗状态: %.2fW > 50W", power_data->power);
            return true;
        }
    }
    
    // TODO: 等待网络模组API启用后，添加以下功能：
    // - 通过HTTP客户端从算力模组获取状态
    // - 通过HTTP客户端从应用模组获取状态  
    // - 实现综合负载评估算法
    
    return false;
}

static uint32_t get_time_seconds(void) {
    return xTaskGetTickCount() / configTICK_RATE_HZ;
}
