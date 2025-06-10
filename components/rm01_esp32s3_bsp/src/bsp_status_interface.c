/**
 * @file bsp_status_interface.c
 * @brief BSP统一状态接口实现 - 单一入口的状态获取和控制
 * 
 * 统一的状态查询和控制接口，聚合所有子系统状态信息
 * 提供缓存、异步查询、条件监听等高级功能
 */

#include "bsp_status_interface.h"
#include "bsp_state_manager.h"
#include "bsp_display_controller.h"
#include "network_monitor.h"
#include "bsp_power.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

static const char *TAG = "BSP_STATUS_IF";

// ========== 内部数据结构 ==========

// 状态缓存结构
typedef struct {
    unified_system_status_t cached_status;
    uint32_t cache_timestamp;
    uint32_t cache_ttl_ms;
    bool cache_valid;
    SemaphoreHandle_t cache_mutex;
} status_cache_t;

// 事件监听器结构
#define MAX_LISTENERS 8
typedef struct {
    bsp_status_change_callback_t callback;
    void* user_data;
    status_watch_config_t config;
    bool active;
    uint32_t last_trigger_time;
} event_listener_t;

// 性能统计结构
typedef struct {
    uint32_t total_queries;
    uint32_t cache_hits;
    uint32_t cache_misses;
    uint64_t total_query_time_us;
    uint32_t event_notifications_sent;
    uint32_t error_count;
} performance_stats_t;

// 统一状态接口管理器
typedef struct {
    bool initialized;
    bool monitoring_active;
    
    // 缓存管理
    status_cache_t cache;
    status_cache_config_t cache_config;
    
    // 事件监听
    event_listener_t listeners[MAX_LISTENERS];
    QueueHandle_t event_queue;
    TaskHandle_t event_task_handle;
    
    // 性能统计
    performance_stats_t stats;
    SemaphoreHandle_t stats_mutex;
    
    // 配置
    bool debug_mode;
} bsp_status_interface_t;

// 全局接口管理器实例
static bsp_status_interface_t s_interface = {0};

// ========== 静态函数声明 ==========

static esp_err_t collect_system_status(unified_system_status_t* status);
static esp_err_t collect_network_status(network_connection_status_t* network);
static esp_err_t collect_performance_status(system_performance_status_t* performance);
static esp_err_t collect_display_status(display_control_status_t* display);
static void event_processing_task(void* pvParameters);
static void publish_system_event(bsp_event_type_t type, void* data, uint32_t data_size, const char* source);
static void state_manager_callback(system_state_t old_state, system_state_t new_state, void* user_data);
static bool should_trigger_listener(const event_listener_t* listener, const bsp_system_event_t* event);
static uint32_t get_time_ms(void);
static uint64_t get_time_us(void);
static void update_performance_stats(bool cache_hit, uint64_t query_time_us);

// ========== 核心接口实现 ==========

esp_err_t bsp_status_interface_init(void) {
    ESP_LOGI(TAG, "初始化BSP统一状态接口");
    
    if (s_interface.initialized) {
        ESP_LOGW(TAG, "BSP状态接口已初始化");
        return ESP_OK;
    }
    
    // 创建互斥锁
    s_interface.cache.cache_mutex = xSemaphoreCreateMutex();
    if (s_interface.cache.cache_mutex == NULL) {
        ESP_LOGE(TAG, "创建缓存互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    s_interface.stats_mutex = xSemaphoreCreateMutex();
    if (s_interface.stats_mutex == NULL) {
        ESP_LOGE(TAG, "创建统计互斥锁失败");
        vSemaphoreDelete(s_interface.cache.cache_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // 创建事件队列
    s_interface.event_queue = xQueueCreate(16, sizeof(bsp_system_event_t));
    if (s_interface.event_queue == NULL) {
        ESP_LOGE(TAG, "创建事件队列失败");
        vSemaphoreDelete(s_interface.cache.cache_mutex);
        vSemaphoreDelete(s_interface.stats_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // 初始化缓存配置
    s_interface.cache_config.cache_ttl_ms = 1000;  // 1秒缓存
    s_interface.cache_config.enable_auto_refresh = true;
    s_interface.cache_config.auto_refresh_interval_ms = 5000;  // 5秒自动刷新
    s_interface.cache.cache_ttl_ms = s_interface.cache_config.cache_ttl_ms;
    
    // 初始化监听器数组
    memset(s_interface.listeners, 0, sizeof(s_interface.listeners));
    
    // 初始化性能统计
    memset(&s_interface.stats, 0, sizeof(s_interface.stats));
    
    s_interface.debug_mode = false;
    s_interface.initialized = true;
    
    ESP_LOGI(TAG, "BSP统一状态接口初始化完成");
    return ESP_OK;
}

esp_err_t bsp_status_interface_start(void) {
    if (!s_interface.initialized) {
        ESP_LOGE(TAG, "状态接口未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "启动BSP统一状态接口服务");
    
    // 启动事件处理任务
    BaseType_t ret = xTaskCreate(
        event_processing_task,
        "bsp_status_events",
        4096,
        NULL,
        5,  // 高优先级
        &s_interface.event_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建事件处理任务失败");
        return ESP_ERR_NO_MEM;
    }
    
    // 注册状态管理器回调
    esp_err_t err = bsp_state_manager_register_callback(state_manager_callback, NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "注册状态管理器回调失败: %s", esp_err_to_name(err));
    }
    
    s_interface.monitoring_active = true;
    
    // 初始化缓存
    unified_system_status_t initial_status;
    err = collect_system_status(&initial_status);
    if (err == ESP_OK) {
        if (xSemaphoreTake(s_interface.cache.cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_interface.cache.cached_status = initial_status;
            s_interface.cache.cache_timestamp = get_time_ms();
            s_interface.cache.cache_valid = true;
            xSemaphoreGive(s_interface.cache.cache_mutex);
        }
        ESP_LOGI(TAG, "初始状态缓存已建立");
    }
    
    ESP_LOGI(TAG, "BSP统一状态接口服务已启动");
    return ESP_OK;
}

void bsp_status_interface_stop(void) {
    if (!s_interface.monitoring_active) {
        ESP_LOGW(TAG, "状态接口服务未运行");
        return;
    }
    
    ESP_LOGI(TAG, "停止BSP统一状态接口服务");
    
    s_interface.monitoring_active = false;
    
    // 停止事件处理任务
    if (s_interface.event_task_handle != NULL) {
        vTaskDelete(s_interface.event_task_handle);
        s_interface.event_task_handle = NULL;
    }
    
    // 注销回调
    bsp_state_manager_unregister_callback(state_manager_callback);
    
    // 清除事件队列
    xQueueReset(s_interface.event_queue);
    
    ESP_LOGI(TAG, "BSP统一状态接口服务已停止");
}

esp_err_t bsp_get_system_status(unified_system_status_t* status) {
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_interface.initialized) {
        ESP_LOGE(TAG, "状态接口未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    uint64_t start_time = get_time_us();
    esp_err_t ret = collect_system_status(status);
    uint64_t query_time = get_time_us() - start_time;
    
    // 更新性能统计
    update_performance_stats(false, query_time);
    
    if (ret == ESP_OK && s_interface.debug_mode) {
        ESP_LOGI(TAG, "系统状态查询完成，耗时: %" PRIu64 " us", query_time);
    }
    
    return ret;
}

esp_err_t bsp_get_system_status_cached(unified_system_status_t* status, uint32_t max_age_ms) {
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_interface.initialized) {
        ESP_LOGE(TAG, "状态接口未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    uint64_t start_time = get_time_us();
    bool cache_hit = false;
    esp_err_t ret = ESP_OK;
    
    // 检查缓存
    if (xSemaphoreTake(s_interface.cache.cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        uint32_t current_time = get_time_ms();
        uint32_t cache_age = current_time - s_interface.cache.cache_timestamp;
        
        if (s_interface.cache.cache_valid && cache_age <= max_age_ms) {
            // 缓存命中
            *status = s_interface.cache.cached_status;
            cache_hit = true;
        } else {
            // 缓存过期或无效，需要刷新
            xSemaphoreGive(s_interface.cache.cache_mutex);
            
            // 收集新状态
            ret = collect_system_status(status);
            if (ret == ESP_OK) {
                // 更新缓存
                if (xSemaphoreTake(s_interface.cache.cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    s_interface.cache.cached_status = *status;
                    s_interface.cache.cache_timestamp = current_time;
                    s_interface.cache.cache_valid = true;
                    xSemaphoreGive(s_interface.cache.cache_mutex);
                }
            }
            goto exit_update_stats;
        }
        
        xSemaphoreGive(s_interface.cache.cache_mutex);
    } else {
        // 无法获取缓存锁，直接查询
        ret = collect_system_status(status);
    }
    
exit_update_stats:
    uint64_t query_time = get_time_us() - start_time;
    update_performance_stats(cache_hit, query_time);
    
    if (ret == ESP_OK && s_interface.debug_mode) {
        ESP_LOGI(TAG, "缓存状态查询完成，%s，耗时: %" PRIu64 " us", 
                 cache_hit ? "缓存命中" : "缓存未命中", query_time);
    }
    
    return ret;
}

esp_err_t bsp_set_display_mode(bool manual_mode) {
    if (!s_interface.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // 注意：LED Matrix现在独立管理Logo显示，不再通过display controller
    // 这里只发布事件通知其他组件显示模式的变化
    publish_system_event(BSP_EVENT_DISPLAY_CHANGED, &manual_mode, sizeof(bool), "status_interface");
    
    return ESP_OK;
}

esp_err_t bsp_set_animation(int animation_index) {
    if (!s_interface.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // 注意：LED Matrix现在独立管理Logo显示，animation设置已移至led_matrix_logo_display
    // 这里只发布事件通知动画索引的变化
    publish_system_event(BSP_EVENT_DISPLAY_CHANGED, &animation_index, sizeof(int), "status_interface");
    
    return ESP_OK;
}

esp_err_t bsp_force_status_refresh(void) {
    if (!s_interface.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "强制刷新系统状态");
    
    // 清除缓存
    if (xSemaphoreTake(s_interface.cache.cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_interface.cache.cache_valid = false;
        xSemaphoreGive(s_interface.cache.cache_mutex);
    }
    
    // 触发状态管理器更新
    bsp_state_manager_update_now();
    
    // 收集新状态并更新缓存
    unified_system_status_t new_status;
    esp_err_t ret = collect_system_status(&new_status);
    
    if (ret == ESP_OK) {
        if (xSemaphoreTake(s_interface.cache.cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_interface.cache.cached_status = new_status;
            s_interface.cache.cache_timestamp = get_time_ms();
            s_interface.cache.cache_valid = true;
            xSemaphoreGive(s_interface.cache.cache_mutex);
        }
        
        ESP_LOGI(TAG, "系统状态刷新完成");
    } else {
        ESP_LOGE(TAG, "系统状态刷新失败: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

// ========== 事件订阅接口实现 ==========

esp_err_t bsp_register_status_listener(bsp_status_change_callback_t callback, void* user_data) {
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 查找空闲监听器槽
    for (int i = 0; i < MAX_LISTENERS; i++) {
        if (!s_interface.listeners[i].active) {
            s_interface.listeners[i].callback = callback;
            s_interface.listeners[i].user_data = user_data;
            s_interface.listeners[i].config = bsp_get_default_watch_config();
            s_interface.listeners[i].active = true;
            s_interface.listeners[i].last_trigger_time = 0;
            
            ESP_LOGI(TAG, "已注册状态监听器 %d", i);
            return ESP_OK;
        }
    }
    
    ESP_LOGE(TAG, "无可用的监听器槽，最大支持 %d 个监听器", MAX_LISTENERS);
    return ESP_ERR_NO_MEM;
}

esp_err_t bsp_unregister_status_listener(bsp_status_change_callback_t callback) {
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < MAX_LISTENERS; i++) {
        if (s_interface.listeners[i].active && s_interface.listeners[i].callback == callback) {
            s_interface.listeners[i].active = false;
            s_interface.listeners[i].callback = NULL;
            s_interface.listeners[i].user_data = NULL;
            
            ESP_LOGI(TAG, "已注销状态监听器 %d", i);
            return ESP_OK;
        }
    }
    
    ESP_LOGW(TAG, "未找到要注销的监听器");
    return ESP_ERR_NOT_FOUND;
}

// ========== 调试和监控接口实现 ==========

void bsp_print_system_status_report(void) {
    unified_system_status_t status;
    esp_err_t ret = bsp_get_system_status(&status);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取系统状态失败: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "========== BSP统一系统状态报告 ==========");
    ESP_LOGI(TAG, "系统运行时间: %" PRIu32 " 秒", status.system_uptime_seconds);
    ESP_LOGI(TAG, "当前状态: %s", bsp_state_manager_get_state_name(status.current_state));
    ESP_LOGI(TAG, "前一状态: %s", bsp_state_manager_get_state_name(status.previous_state));
    ESP_LOGI(TAG, "状态变化次数: %" PRIu32, status.state_change_count);
    ESP_LOGI(TAG, "在当前状态时间: %" PRIu32 " 秒", status.time_in_current_state);
    
    ESP_LOGI(TAG, "网络连接状态:");
    ESP_LOGI(TAG, "  算力模组: %s", status.network.computing_module_connected ? "已连接" : "断开");
    ESP_LOGI(TAG, "  应用模组: %s", status.network.application_module_connected ? "已连接" : "断开");
    ESP_LOGI(TAG, "  用户主机: %s", status.network.user_host_connected ? "已连接" : "断开");
    ESP_LOGI(TAG, "  互联网: %s", status.network.internet_connected ? "已连接" : "断开");
    
    ESP_LOGI(TAG, "系统性能状态:");
    ESP_LOGI(TAG, "  温度: %.1f°C", status.performance.current_temperature);
    ESP_LOGI(TAG, "  功耗: %.1fW", status.performance.current_power_consumption);
    ESP_LOGI(TAG, "  高计算负载: %s", status.performance.high_compute_load ? "是" : "否");
    
    ESP_LOGI(TAG, "显示控制状态:");
    ESP_LOGI(TAG, "  当前动画: %d", status.display.current_animation_index);
    ESP_LOGI(TAG, "  动画切换次数: %" PRIu32, status.display.total_animation_switches);
    ESP_LOGI(TAG, "  手动模式: %s", status.display.manual_display_mode ? "是" : "否");
    ESP_LOGI(TAG, "  控制器激活: %s", status.display.display_controller_active ? "是" : "否");
    
    ESP_LOGI(TAG, "状态数据来源: %s", status.status_source);
    ESP_LOGI(TAG, "状态时间戳: %" PRIu32, status.status_timestamp);
    ESP_LOGI(TAG, "==========================================");
}

bool bsp_is_system_healthy(void) {
    unified_system_status_t status;
    esp_err_t ret = bsp_get_system_status_cached(&status, 5000);  // 5秒缓存
    
    if (ret != ESP_OK) {
        return false;
    }
    
    // 检查关键健康指标
    bool healthy = true;
    
    // 检查是否有网络连接
    if (!status.network.computing_module_connected && 
        !status.network.application_module_connected && 
        !status.network.user_host_connected) {
        healthy = false;
    }
    
    // 检查显示控制器状态
    if (!status.display.display_controller_active) {
        healthy = false;
    }
    
    // 检查是否处于高温状态
    if (status.current_state == SYSTEM_STATE_HIGH_TEMP_1 || 
        status.current_state == SYSTEM_STATE_HIGH_TEMP_2) {
        healthy = false;
    }
    
    return healthy;
}

// ========== 静态函数实现 ==========

static esp_err_t collect_system_status(unified_system_status_t* status) {
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(status, 0, sizeof(unified_system_status_t));
    
    // 收集基础状态信息
    system_state_info_t state_info;
    esp_err_t ret = bsp_state_manager_get_info(&state_info);
    if (ret == ESP_OK) {
        status->current_state = state_info.current_state;
        status->previous_state = state_info.previous_state;
        status->state_change_count = state_info.state_change_count;
        status->time_in_current_state = state_info.time_in_current_state;
    }
    
    // 收集网络状态
    collect_network_status(&status->network);
    
    // 收集性能状态
    collect_performance_status(&status->performance);
    
    // 收集显示状态
    collect_display_status(&status->display);
    
    // 设置元数据
    status->system_uptime_seconds = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
    status->status_timestamp = get_time_ms();
    status->status_valid = true;
    strncpy(status->status_source, "bsp_status_interface", sizeof(status->status_source) - 1);
    
    return ESP_OK;
}

static esp_err_t collect_network_status(network_connection_status_t* network) {
    if (network == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 查询各个网络目标状态
    network->computing_module_connected = (nm_get_status(NM_COMPUTING_MODULE_IP) == NM_STATUS_UP);
    network->application_module_connected = (nm_get_status(NM_APPLICATION_MODULE_IP) == NM_STATUS_UP);
    network->user_host_connected = (nm_get_status(NM_USER_HOST_IP) == NM_STATUS_UP);
    network->internet_connected = (nm_get_status(NM_INTERNET_IP) == NM_STATUS_UP);
    
    // 设置其他网络统计信息（如果可用）
    network->network_check_count = 0;  // TODO: 从网络监控器获取
    network->last_network_change_time = get_time_ms();
    
    return ESP_OK;
}

static esp_err_t collect_performance_status(system_performance_status_t* performance) {
    if (performance == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 收集温度信息（XSP16芯片暂不提供温度数据）
    performance->current_temperature = 0.0f;
      // 收集功耗信息
    const bsp_power_chip_data_t* power_data = bsp_get_latest_power_chip_data();
    if (power_data != NULL && power_data->valid) {
        performance->current_power_consumption = power_data->power;
    } else {
        performance->current_power_consumption = 0.0f;
    }
    
    // 高计算负载检测（基于功耗）
    performance->high_compute_load = (performance->current_power_consumption > 50.0f);
    
    // CPU和内存使用率（待API集成后实现）
    performance->cpu_usage_percent = 0.0f;
    performance->memory_usage_percent = 0.0f;
    performance->active_task_count = 0;
    
    return ESP_OK;
}

static esp_err_t collect_display_status(display_control_status_t* display) {
    if (display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
      // 获取显示控制器状态（只处理WS2812状态显示）
    display_controller_status_t controller_status;
    esp_err_t ret = bsp_display_controller_get_status(&controller_status);
    if (ret == ESP_OK) {
        // 注意：animation相关字段已移除，只保留WS2812相关状态
        display->display_controller_active = controller_status.controller_active;
    }
    
    // LED Matrix Logo Display状态（如果需要的话）
    // 可以通过led_matrix_logo_display_get_status()获取
    
    // 其他显示状态信息
    display->manual_display_mode = false;  // TODO: 从LED Matrix Logo Display获取
    display->auto_switch_enabled = true;   // TODO: 从LED Matrix Logo Display获取
    
    return ESP_OK;
}

static void event_processing_task(void* pvParameters) {
    ESP_LOGI(TAG, "事件处理任务已启动");
    
    bsp_system_event_t event;
    
    while (s_interface.monitoring_active) {
        // 等待事件
        if (xQueueReceive(s_interface.event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // 处理事件
            for (int i = 0; i < MAX_LISTENERS; i++) {
                if (s_interface.listeners[i].active && 
                    should_trigger_listener(&s_interface.listeners[i], &event)) {
                    
                    // 触发回调
                    s_interface.listeners[i].callback(&event, s_interface.listeners[i].user_data);
                    s_interface.listeners[i].last_trigger_time = get_time_ms();
                    
                    // 更新统计
                    if (xSemaphoreTake(s_interface.stats_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        s_interface.stats.event_notifications_sent++;
                        xSemaphoreGive(s_interface.stats_mutex);
                    }
                }
            }
        }
    }
    
    ESP_LOGI(TAG, "事件处理任务已结束");
    vTaskDelete(NULL);
}

static void publish_system_event(bsp_event_type_t type, void* data, uint32_t data_size, const char* source) {
    bsp_system_event_t event = {
        .type = type,
        .data = data,
        .data_size = data_size,
        .timestamp = get_time_ms()
    };
    
    if (source != NULL) {
        strncpy(event.source_component, source, sizeof(event.source_component) - 1);
    }
    
    if (s_interface.event_queue != NULL) {
        if (xQueueSend(s_interface.event_queue, &event, 0) != pdTRUE) {
            // 队列满，丢弃事件
            if (xSemaphoreTake(s_interface.stats_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                s_interface.stats.error_count++;
                xSemaphoreGive(s_interface.stats_mutex);
            }
        }
    }
}

static void state_manager_callback(system_state_t old_state, system_state_t new_state, void* user_data) {
    // 发布状态变化事件
    struct {
        system_state_t old_state;
        system_state_t new_state;
    } state_change_data = {old_state, new_state};
    
    publish_system_event(BSP_EVENT_STATE_CHANGED, &state_change_data, sizeof(state_change_data), "state_manager");
    
    // 使缓存失效
    if (xSemaphoreTake(s_interface.cache.cache_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_interface.cache.cache_valid = false;
        xSemaphoreGive(s_interface.cache.cache_mutex);
    }
}

static bool should_trigger_listener(const event_listener_t* listener, const bsp_system_event_t* event) {
    if (listener == NULL || event == NULL) {
        return false;
    }
    
    // 检查事件类型
    if (!(listener->config.event_mask & (1 << event->type))) {
        return false;
    }
    
    // 检查防抖间隔
    uint32_t current_time = get_time_ms();
    if (current_time - listener->last_trigger_time < listener->config.min_change_interval_ms) {
        return false;
    }
    
    return true;
}

static uint32_t get_time_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static uint64_t get_time_us(void) {
    return esp_timer_get_time();
}

static void update_performance_stats(bool cache_hit, uint64_t query_time_us) {
    if (xSemaphoreTake(s_interface.stats_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_interface.stats.total_queries++;
        s_interface.stats.total_query_time_us += query_time_us;
        
        if (cache_hit) {
            s_interface.stats.cache_hits++;
        } else {
            s_interface.stats.cache_misses++;
        }
        
        xSemaphoreGive(s_interface.stats_mutex);
    }
}

// ========== 便利接口实现 ==========

status_cache_config_t bsp_get_default_cache_config(void) {
    status_cache_config_t config = {
        .cache_ttl_ms = 1000,
        .enable_auto_refresh = true,
        .auto_refresh_interval_ms = 5000
    };
    return config;
}

status_watch_config_t bsp_get_default_watch_config(void) {
    status_watch_config_t config = {
        .event_mask = (1 << BSP_EVENT_STATE_CHANGED) | (1 << BSP_EVENT_NETWORK_CHANGED),
        .min_change_interval_ms = 100,
        .numeric_change_threshold = 5.0f,  // 5%
        .batch_events = false
    };
    return config;
}
