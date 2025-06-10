/**
 * @file bsp_board_ws2812_display.c
 * @brief Board WS2812 显示控制器实现
 * 
 * 基于优先级的系统状态监控显示： * - 高温: 红色慢速呼吸 (高优先级) 
 * - 功率过高: 紫色快速呼吸 (中优先级)
 * - 内存高使用率: 紫色慢速呼吸 (低优先级)
 *  * 通过Prometheus API获取N305和Jetson监控数据
 * 支持温度、功率、内存使用率监控
 */

#include "bsp_board_ws2812_display.h"
#include "bsp_ws2812.h"
#include "network_monitor.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"
#include "cJSON.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *TAG = "BOARD_WS2812_DISP";

// ========== 颜色定义 ==========

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

// 预定义颜色 - Board WS2812专用于系统状态监控
static const rgb_color_t COLOR_RED = {255, 0, 0};      // 红色 - 高温警告
static const rgb_color_t COLOR_PURPLE = {128, 0, 128}; // 紫色 - GPU/内存警告
static const rgb_color_t COLOR_OFF = {0, 0, 0};        // 关闭

// ========== 显示控制器状态结构 ==========

typedef struct {
    board_display_config_t config;
    board_display_status_t status;
    bool is_initialized;
    bool manual_mode;
    SemaphoreHandle_t status_mutex;
    TaskHandle_t display_task_handle;
    TaskHandle_t metrics_task_handle;
    bool task_running;
    
    // 动画状态
    uint32_t animation_start_time;
    uint32_t last_update_time;
    uint8_t breath_brightness;          // 呼吸灯亮度
    bool breath_increasing;             // 呼吸灯方向
    
    // HTTP客户端状态
    char* n305_response_buffer;
    char* jetson_response_buffer;
    size_t buffer_size;
} board_display_controller_t;

// 全局控制器实例
static board_display_controller_t s_controller = {0};

// 显示模式名称映射
static const char* MODE_NAMES[] = {
    "关闭状态",
    "高温警告",
    "功率过高",
    "内存高使用率"
};

// ========== 静态函数声明 ==========

static bool is_board_display_initialized(void);
static void board_display_task(void *pvParameters);
static void metrics_collection_task(void *pvParameters);
static board_display_mode_t determine_display_mode(void);
static void execute_display_mode(board_display_mode_t mode);
static void set_board_led_color_all(uint8_t r, uint8_t g, uint8_t b);
static void handle_breath_animation(const rgb_color_t* color, board_breath_speed_t speed);
static uint32_t get_time_ms(void);
static uint8_t apply_brightness(uint8_t color_value, uint8_t brightness);

// Prometheus数据解析相关
static esp_err_t fetch_prometheus_metrics(const char* url, char* response_buffer, size_t buffer_size);
static esp_err_t fetch_n305_temperature(system_metrics_t* metrics);
static esp_err_t fetch_jetson_metrics(system_metrics_t* metrics);
static esp_err_t query_prometheus_api(const char* base_url, const char* query, char* response_buffer, size_t buffer_size);
static esp_err_t parse_prometheus_query_response(const char* response, float* value);
static esp_err_t parse_n305_metrics(const char* data, system_metrics_t* metrics);
static esp_err_t parse_jetson_metrics(const char* data, system_metrics_t* metrics);
static float extract_metric_value(const char* data, const char* metric_name);
static esp_err_t http_event_handler(esp_http_client_event_t *evt);

// ========== 核心接口实现 ==========

esp_err_t bsp_board_ws2812_display_init(const board_display_config_t* config) {
    ESP_LOGI(TAG, "初始化Board WS2812显示控制器");
    
    if (s_controller.is_initialized) {
        ESP_LOGW(TAG, "Board WS2812显示控制器已初始化");
        return ESP_OK;
    }
    
    // 检查Board WS2812是否已初始化
    if (bsp_ws2812_get_handle(BSP_WS2812_ONBOARD) == NULL) {
        ESP_LOGE(TAG, "Board WS2812未初始化，请先调用bsp_ws2812_init()");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 创建互斥锁
    s_controller.status_mutex = xSemaphoreCreateMutex();
    if (s_controller.status_mutex == NULL) {
        ESP_LOGE(TAG, "创建状态互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    // 设置配置
    if (config != NULL) {
        s_controller.config = *config;
    } else {
        s_controller.config = bsp_board_ws2812_display_get_default_config();
    }
      // 分配HTTP响应缓冲区 - 优化内存使用
    s_controller.buffer_size = 4096;  // 4KB缓冲区，适合ESP32S3内存限制
    s_controller.n305_response_buffer = malloc(s_controller.buffer_size);
    s_controller.jetson_response_buffer = malloc(s_controller.buffer_size);
    
    if (!s_controller.n305_response_buffer || !s_controller.jetson_response_buffer) {
        ESP_LOGE(TAG, "分配HTTP响应缓冲区失败");
        if (s_controller.n305_response_buffer) free(s_controller.n305_response_buffer);
        if (s_controller.jetson_response_buffer) free(s_controller.jetson_response_buffer);
        vSemaphoreDelete(s_controller.status_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // 初始化状态
    memset(&s_controller.status, 0, sizeof(s_controller.status));
    s_controller.status.current_mode = BOARD_DISPLAY_MODE_OFF;
    s_controller.manual_mode = false;
    s_controller.task_running = false;
    
    // 初始化动画状态
    s_controller.animation_start_time = get_time_ms();
    s_controller.last_update_time = s_controller.animation_start_time;
    s_controller.breath_brightness = 0;
    s_controller.breath_increasing = true;
    
    // 初始化监控数据
    s_controller.status.metrics.n305_data_valid = false;
    s_controller.status.metrics.jetson_data_valid = false;
    s_controller.status.metrics.last_update_time = get_time_ms();
    
    // 设置初始显示状态（全部关闭）
    ESP_LOGI(TAG, "设置初始显示状态为关闭");
    set_board_led_color_all(COLOR_OFF.r, COLOR_OFF.g, COLOR_OFF.b);
    
    s_controller.is_initialized = true;
    
    ESP_LOGI(TAG, "Board WS2812显示控制器初始化完成");
    ESP_LOGI(TAG, "  自动模式: %s", s_controller.config.auto_mode_enabled ? "启用" : "禁用");
    ESP_LOGI(TAG, "  调试模式: %s", s_controller.config.debug_mode ? "启用" : "禁用");
    ESP_LOGI(TAG, "  亮度: %d", s_controller.config.brightness);
    ESP_LOGI(TAG, "  更新间隔: %ld ms", s_controller.config.update_interval_ms);
    
    return ESP_OK;
}

esp_err_t bsp_board_ws2812_display_start(void) {
    if (!s_controller.is_initialized) {
        ESP_LOGE(TAG, "Board WS2812显示控制器未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_controller.task_running) {
        ESP_LOGW(TAG, "Board WS2812显示任务已在运行");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "启动Board WS2812显示控制器");
    
    // 先设置标志位，确保任务不会立即退出
    s_controller.task_running = true;
    
    // 创建显示任务
    BaseType_t ret = xTaskCreate(
        board_display_task,
        "board_ws2812_display",
        4096,
        NULL,
        5,  // 较高优先级
        &s_controller.display_task_handle
    );
    
    if (ret != pdPASS) {
        s_controller.task_running = false;
        ESP_LOGE(TAG, "创建Board WS2812显示任务失败");
        return ESP_FAIL;
    }
    
    // 创建监控数据收集任务
    ret = xTaskCreate(
        metrics_collection_task,
        "board_metrics_collection",
        8192,  // 更大的堆栈用于HTTP请求
        NULL,
        3,  // 中等优先级
        &s_controller.metrics_task_handle
    );
    
    if (ret != pdPASS) {
        s_controller.task_running = false;
        vTaskDelete(s_controller.display_task_handle);
        s_controller.display_task_handle = NULL;
        ESP_LOGE(TAG, "创建监控数据收集任务失败");
        return ESP_FAIL;
    }
    
    // 更新状态
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.is_active = true;
        xSemaphoreGive(s_controller.status_mutex);
    }
    
    ESP_LOGI(TAG, "Board WS2812显示控制器已启动");
    return ESP_OK;
}

void bsp_board_ws2812_display_stop(void) {
    if (!s_controller.is_initialized) {
        ESP_LOGW(TAG, "Board WS2812显示控制器未初始化");
        return;
    }
    
    ESP_LOGI(TAG, "停止Board WS2812显示控制器");
    
    // 停止任务
    s_controller.task_running = false;
    
    if (s_controller.display_task_handle != NULL) {
        vTaskDelete(s_controller.display_task_handle);
        s_controller.display_task_handle = NULL;
    }
    
    if (s_controller.metrics_task_handle != NULL) {
        vTaskDelete(s_controller.metrics_task_handle);
        s_controller.metrics_task_handle = NULL;
    }
    
    // 关闭LED
    bsp_board_ws2812_display_off();
    
    // 更新状态
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.is_active = false;
        xSemaphoreGive(s_controller.status_mutex);
    }
    
    ESP_LOGI(TAG, "Board WS2812显示控制器已停止");
}

esp_err_t bsp_board_ws2812_display_set_mode(board_display_mode_t mode) {
    if (!s_controller.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (mode >= BOARD_DISPLAY_MODE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "手动设置Board WS2812显示模式: %s", bsp_board_ws2812_display_get_mode_name(mode));
    
    // 进入手动模式
    s_controller.manual_mode = true;
    
    // 更新状态
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        board_display_mode_t old_mode = s_controller.status.current_mode;
        s_controller.status.previous_mode = old_mode;
        s_controller.status.current_mode = mode;
        
        if (old_mode != mode) {
            s_controller.status.mode_change_count++;
            s_controller.animation_start_time = get_time_ms();
            
            // 重置动画状态
            s_controller.breath_brightness = 0;
            s_controller.breath_increasing = true;
            
            xSemaphoreGive(s_controller.status_mutex);
        }
        
        ESP_LOGI(TAG, "Board WS2812显示模式变化: [%s] -> [%s]", 
                 bsp_board_ws2812_display_get_mode_name(old_mode),
                 bsp_board_ws2812_display_get_mode_name(mode));
    }
    
    return ESP_OK;
}

void bsp_board_ws2812_display_resume_auto(void) {
    ESP_LOGI(TAG, "恢复Board WS2812自动模式");
    s_controller.manual_mode = false;
}

esp_err_t bsp_board_ws2812_display_get_status(board_display_status_t* status) {
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_board_display_initialized()) {
        ESP_LOGW(TAG, "Board WS2812显示控制器未初始化，无法获取状态");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        *status = s_controller.status;
        status->time_in_current_mode = get_time_ms() - s_controller.animation_start_time;
        xSemaphoreGive(s_controller.status_mutex);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

void bsp_board_ws2812_display_print_status(void) {
    board_display_status_t status;
    esp_err_t ret = bsp_board_ws2812_display_get_status(&status);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取Board WS2812显示状态失败");
        return;
    }
    
    ESP_LOGI(TAG, "========== Board WS2812显示状态 ==========");
    ESP_LOGI(TAG, "激活状态: %s", status.is_active ? "是" : "否");
    ESP_LOGI(TAG, "手动模式: %s", s_controller.manual_mode ? "是" : "否");
    ESP_LOGI(TAG, "自动模式: %s", s_controller.config.auto_mode_enabled ? "启用" : "禁用");
    ESP_LOGI(TAG, "调试模式: %s", s_controller.config.debug_mode ? "启用" : "禁用");
    ESP_LOGI(TAG, "当前显示模式: %s", bsp_board_ws2812_display_get_mode_name(status.current_mode));
    ESP_LOGI(TAG, "前一个模式: %s", bsp_board_ws2812_display_get_mode_name(status.previous_mode));
    ESP_LOGI(TAG, "模式变化次数: %ld", status.mode_change_count);
    ESP_LOGI(TAG, "在当前模式时间: %ld ms", status.time_in_current_mode);
    ESP_LOGI(TAG, "系统运行时间: %ld ms", status.system_uptime_ms);
    
    ESP_LOGI(TAG, "监控数据状态:");
    ESP_LOGI(TAG, "  N305数据有效: %s", status.metrics.n305_data_valid ? "是" : "否");
    ESP_LOGI(TAG, "  Jetson数据有效: %s", status.metrics.jetson_data_valid ? "是" : "否");
    if (status.metrics.n305_data_valid) {
        ESP_LOGI(TAG, "  N305 CPU温度: %.1f°C", status.metrics.n305_cpu_temp);
    }
    if (status.metrics.jetson_data_valid) {
        ESP_LOGI(TAG, "  Jetson CPU温度: %.1f°C", status.metrics.jetson_cpu_temp);
        ESP_LOGI(TAG, "  Jetson GPU温度: %.1f°C", status.metrics.jetson_gpu_temp);
        ESP_LOGI(TAG, "  Jetson功率: %.1f mW (%.2f W)", status.metrics.jetson_power_mw, status.metrics.jetson_power_mw/1000.0f);
        ESP_LOGI(TAG, "  Jetson内存使用率: %.1f%%", status.metrics.jetson_memory_usage);
    }
    ESP_LOGI(TAG, "========================================");
}

// ========== 配置接口实现 ==========

board_display_config_t bsp_board_ws2812_display_get_default_config(void) {
    board_display_config_t config = {
        .auto_mode_enabled = true,
        .debug_mode = true,             // 暂时启用调试模式以便排查问题
        .brightness = 255,              // 设置为最大亮度以便观察
        .update_interval_ms = 200,      // 200ms显示更新间隔
        .metrics_interval_ms = BOARD_METRICS_UPDATE_INTERVAL  // 10秒监控数据更新间隔
    };
    return config;
}

void bsp_board_ws2812_display_set_auto_mode(bool enabled) {
    s_controller.config.auto_mode_enabled = enabled;
    ESP_LOGI(TAG, "Board WS2812自动模式设置为: %s", enabled ? "启用" : "禁用");
}

void bsp_board_ws2812_display_set_brightness(uint8_t brightness) {
    s_controller.config.brightness = brightness;
    ESP_LOGI(TAG, "Board WS2812亮度设置为: %d", brightness);
}

void bsp_board_ws2812_display_set_debug_mode(bool debug_mode) {
    s_controller.config.debug_mode = debug_mode;
    ESP_LOGI(TAG, "Board WS2812调试模式设置为: %s", debug_mode ? "启用" : "禁用");
}

// ========== 手动控制接口实现 ==========

esp_err_t bsp_board_ws2812_display_set_color(uint8_t r, uint8_t g, uint8_t b) {
    if (!is_board_display_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "手动设置Board WS2812颜色: RGB(%d,%d,%d)", r, g, b);
    set_board_led_color_all(r, g, b);
    
    return ESP_OK;
}

esp_err_t bsp_board_ws2812_display_set_breath(uint8_t r, uint8_t g, uint8_t b, board_breath_speed_t speed) {
    if (!is_board_display_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    rgb_color_t color = {r, g, b};
    handle_breath_animation(&color, speed);
    return ESP_OK;
}

esp_err_t bsp_board_ws2812_display_off(void) {
    if (!is_board_display_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return bsp_ws2812_clear(BSP_WS2812_ONBOARD);
}

// ========== 监控数据接口实现 ==========

esp_err_t bsp_board_ws2812_display_get_metrics(system_metrics_t* metrics) {
    if (metrics == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_board_display_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        *metrics = s_controller.status.metrics;
        xSemaphoreGive(s_controller.status_mutex);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

esp_err_t bsp_board_ws2812_display_update_metrics(void) {
    ESP_LOGI(TAG, "手动更新监控数据");
    
    system_metrics_t new_metrics = {0};
    esp_err_t n305_result = ESP_FAIL;
    esp_err_t jetson_result = ESP_FAIL;
    
    // 获取N305数据 - 使用Prometheus查询API
    ESP_LOGI(TAG, "正在获取N305监控数据...");
    n305_result = fetch_n305_temperature(&new_metrics);
    
    if (n305_result == ESP_OK) {
        ESP_LOGI(TAG, "N305监控数据获取成功: CPU温度=%.1f°C", new_metrics.n305_cpu_temp);
    } else {
        ESP_LOGW(TAG, "N305数据获取失败");
    }
    
    // 获取Jetson数据 - 同样使用Prometheus查询API
    ESP_LOGI(TAG, "正在获取Jetson监控数据...");
    jetson_result = fetch_jetson_metrics(&new_metrics);
    
    if (jetson_result == ESP_OK) {
        ESP_LOGI(TAG, "Jetson监控数据获取成功: CPU=%.1f°C, GPU=%.1f°C/%.1f%%, 内存=%.1f%%", 
                 new_metrics.jetson_cpu_temp, new_metrics.jetson_gpu_temp, 
                 new_metrics.jetson_power_mw, new_metrics.jetson_memory_usage);
    } else {
        ESP_LOGW(TAG, "Jetson数据获取失败");
    }
    
    // 更新时间戳
    new_metrics.last_update_time = get_time_ms();
    
    // 更新缓存的监控数据
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // 保留之前有效的数据，只更新成功获取的数据
        if (new_metrics.n305_data_valid) {
            s_controller.status.metrics.n305_cpu_temp = new_metrics.n305_cpu_temp;
            s_controller.status.metrics.n305_data_valid = true;
        }
        if (new_metrics.jetson_data_valid) {
            s_controller.status.metrics.jetson_cpu_temp = new_metrics.jetson_cpu_temp;
            s_controller.status.metrics.jetson_gpu_temp = new_metrics.jetson_gpu_temp;
            s_controller.status.metrics.jetson_power_mw = new_metrics.jetson_power_mw;
            s_controller.status.metrics.jetson_memory_total = new_metrics.jetson_memory_total;
            s_controller.status.metrics.jetson_memory_used = new_metrics.jetson_memory_used;
            s_controller.status.metrics.jetson_memory_usage = new_metrics.jetson_memory_usage;
            s_controller.status.metrics.jetson_data_valid = true;
        }
        s_controller.status.metrics.last_update_time = new_metrics.last_update_time;
        xSemaphoreGive(s_controller.status_mutex);
    }
    
    // 返回综合结果
    if (n305_result == ESP_OK || jetson_result == ESP_OK) {
        ESP_LOGI(TAG, "监控数据更新完成 (N305: %s, Jetson: %s)", 
                 n305_result == ESP_OK ? "成功" : "失败",
                 jetson_result == ESP_OK ? "成功" : "失败");
        return ESP_OK;  // 至少有一个成功就算成功
    } else {
        ESP_LOGW(TAG, "所有监控数据获取失败，可能存在网络问题");
        return ESP_FAIL;
    }
}

// ========== 工具函数实现 ==========

const char* bsp_board_ws2812_display_get_mode_name(board_display_mode_t mode) {
    if (mode >= 0 && mode < BOARD_DISPLAY_MODE_COUNT) {
        return MODE_NAMES[mode];
    }
    return "未知模式";
}

// ========== 静态函数实现 ==========

static bool is_board_display_initialized(void) {
    return s_controller.is_initialized;
}

static void board_display_task(void *pvParameters) {
    ESP_LOGI(TAG, "Board WS2812显示任务开始运行");
    
    while (s_controller.task_running) {
        if (s_controller.config.auto_mode_enabled && !s_controller.manual_mode) {
            // 自动模式：根据监控数据确定显示模式
            board_display_mode_t new_mode = determine_display_mode();
            
            // 检查模式是否变化
            if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                if (s_controller.status.current_mode != new_mode) {
                    s_controller.status.previous_mode = s_controller.status.current_mode;
                    s_controller.status.current_mode = new_mode;
                    s_controller.status.mode_change_count++;
                    s_controller.animation_start_time = get_time_ms();
                    
                    // 重置动画状态
                    s_controller.breath_brightness = 0;
                    s_controller.breath_increasing = true;
                    
                    if (s_controller.config.debug_mode) {
                        ESP_LOGI(TAG, "Board WS2812显示模式变化: [%s] -> [%s]", 
                                bsp_board_ws2812_display_get_mode_name(s_controller.status.previous_mode),
                                bsp_board_ws2812_display_get_mode_name(new_mode));
                    }
                }
                xSemaphoreGive(s_controller.status_mutex);
            }
        }
        
        // 执行当前显示模式
        if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            board_display_mode_t current_mode = s_controller.status.current_mode;
            s_controller.status.system_uptime_ms = get_time_ms();
            xSemaphoreGive(s_controller.status_mutex);
            
            execute_display_mode(current_mode);
        }
        
        vTaskDelay(pdMS_TO_TICKS(s_controller.config.update_interval_ms));
    }
    
    ESP_LOGI(TAG, "Board WS2812显示任务结束");
    vTaskDelete(NULL);
}

static void metrics_collection_task(void *pvParameters) {
    ESP_LOGI(TAG, "监控数据收集任务开始运行");
    
    while (s_controller.task_running) {
        esp_err_t ret = bsp_board_ws2812_display_update_metrics();
        
        if (ret != ESP_OK && s_controller.config.debug_mode) {
            ESP_LOGW(TAG, "监控数据更新失败");
        }
        
        vTaskDelay(pdMS_TO_TICKS(s_controller.config.metrics_interval_ms));
    }
    
    ESP_LOGI(TAG, "监控数据收集任务结束");
    vTaskDelete(NULL);
}

static board_display_mode_t determine_display_mode(void) {
    system_metrics_t metrics;
    if (bsp_board_ws2812_display_get_metrics(&metrics) != ESP_OK) {
        return BOARD_DISPLAY_MODE_OFF;
    }
    
    // 基于优先级的模式选择
    // 1. 高优先级：高温警告
    if (metrics.n305_data_valid && metrics.n305_cpu_temp >= BOARD_TEMP_THRESHOLD_HIGH) {
        if (s_controller.config.debug_mode) {
            ESP_LOGI(TAG, "检测到N305高温: %.1f°C >= %.1f°C", metrics.n305_cpu_temp, BOARD_TEMP_THRESHOLD_HIGH);
        }
        return BOARD_DISPLAY_MODE_HIGH_TEMP;
    }
    
    if (metrics.jetson_data_valid && 
        (metrics.jetson_cpu_temp >= BOARD_TEMP_THRESHOLD_HIGH || 
         metrics.jetson_gpu_temp >= BOARD_TEMP_THRESHOLD_HIGH)) {
        if (s_controller.config.debug_mode) {
            ESP_LOGI(TAG, "检测到Jetson高温: CPU=%.1f°C, GPU=%.1f°C >= %.1f°C", 
                     metrics.jetson_cpu_temp, metrics.jetson_gpu_temp, BOARD_TEMP_THRESHOLD_HIGH);
        }
        return BOARD_DISPLAY_MODE_HIGH_TEMP;
    }
      // 2. 中优先级：功率过高
    if (metrics.jetson_data_valid && metrics.jetson_power_mw >= BOARD_POWER_THRESHOLD_HIGH) {
        if (s_controller.config.debug_mode) {
            ESP_LOGI(TAG, "检测到Jetson功率过高: %.1f mW >= %.1f mW", 
                     metrics.jetson_power_mw, BOARD_POWER_THRESHOLD_HIGH);
        }
        return BOARD_DISPLAY_MODE_HIGH_POWER;
    }
    
    // 3. 低优先级：内存高使用率
    if (metrics.jetson_data_valid && metrics.jetson_memory_usage >= BOARD_MEMORY_USAGE_THRESHOLD) {
        if (s_controller.config.debug_mode) {
            ESP_LOGI(TAG, "检测到内存高使用率: %.1f%% >= %.1f%%", 
                     metrics.jetson_memory_usage, BOARD_MEMORY_USAGE_THRESHOLD);
        }
        return BOARD_DISPLAY_MODE_MEMORY_HIGH_USAGE;
    }
    
    return BOARD_DISPLAY_MODE_OFF;
}

static void execute_display_mode(board_display_mode_t mode) {
    switch (mode) {
        case BOARD_DISPLAY_MODE_HIGH_TEMP:
            // 高温警告 - 红色慢速呼吸
            handle_breath_animation(&COLOR_RED, BOARD_BREATH_SPEED_SLOW);
            break;
              case BOARD_DISPLAY_MODE_HIGH_POWER:
            // 功率过高 - 紫色快速呼吸
            handle_breath_animation(&COLOR_PURPLE, BOARD_BREATH_SPEED_FAST);
            break;
            
        case BOARD_DISPLAY_MODE_MEMORY_HIGH_USAGE:
            // 内存高使用率 - 紫色慢速呼吸
            handle_breath_animation(&COLOR_PURPLE, BOARD_BREATH_SPEED_SLOW);
            break;
            
        case BOARD_DISPLAY_MODE_OFF:
        default:
            // 默认关闭
            set_board_led_color_all(COLOR_OFF.r, COLOR_OFF.g, COLOR_OFF.b);
            break;
    }
}

static void set_board_led_color_all(uint8_t r, uint8_t g, uint8_t b) {
    // 应用亮度调整
    uint8_t adj_r = apply_brightness(r, s_controller.config.brightness);
    uint8_t adj_g = apply_brightness(g, s_controller.config.brightness);
    uint8_t adj_b = apply_brightness(b, s_controller.config.brightness);
    
    // 设置所有28个LED为相同颜色
    for (int i = 0; i < BSP_WS2812_ONBOARD_COUNT; i++) {
        esp_err_t ret = bsp_ws2812_set_pixel(BSP_WS2812_ONBOARD, i, adj_r, adj_g, adj_b);
        if (ret != ESP_OK) {
            if (s_controller.config.debug_mode) {
                ESP_LOGE(TAG, "设置Board WS2812像素%d失败: %s", i, esp_err_to_name(ret));
            }
            return;
        }
    }
    
    esp_err_t ret = bsp_ws2812_refresh(BSP_WS2812_ONBOARD);
    if (ret != ESP_OK) {
        if (s_controller.config.debug_mode) {
            ESP_LOGE(TAG, "刷新Board WS2812失败: %s", esp_err_to_name(ret));
        }
    }
}

static void handle_breath_animation(const rgb_color_t* color, board_breath_speed_t speed) {
    uint32_t current_time = get_time_ms();
    uint32_t period_ms;
    
    // 根据速度设置周期
    switch (speed) {
        case BOARD_BREATH_SPEED_FAST:
            period_ms = 1000;
            break;
        case BOARD_BREATH_SPEED_NORMAL:
            period_ms = 2000;
            break;
        case BOARD_BREATH_SPEED_SLOW:
        default:
            period_ms = 3000;
            break;
    }
    
    // 计算呼吸亮度 (使用正弦波实现平滑呼吸效果)
    uint32_t elapsed = (current_time - s_controller.animation_start_time) % period_ms;
    float phase = (float)elapsed / period_ms * 2.0f * M_PI;
    float brightness_factor = (sin(phase) + 1.0f) / 2.0f;  // 0.0 到 1.0
    
    uint8_t breath_r = (uint8_t)(color->r * brightness_factor);
    uint8_t breath_g = (uint8_t)(color->g * brightness_factor);
    uint8_t breath_b = (uint8_t)(color->b * brightness_factor);
    
    set_board_led_color_all(breath_r, breath_g, breath_b);
}

static uint32_t get_time_ms(void) {
    return esp_timer_get_time() / 1000;
}

static uint8_t apply_brightness(uint8_t color_value, uint8_t brightness) {
    return (color_value * brightness) / 255;
}

// ========== Prometheus数据处理实现 ==========

static esp_err_t fetch_prometheus_metrics(const char* url, char* response_buffer, size_t buffer_size) {
    // 先检查网络连接状态
    bool network_available = false;
    const char* target_ip = NULL;
    
    // 从URL中提取IP地址用于网络状态检查
    if (strstr(url, "10.10.99.99") != NULL) {
        target_ip = "10.10.99.99";  // N305应用模块
    } else if (strstr(url, "10.10.99.98") != NULL) {
        target_ip = "10.10.99.98";  // Jetson算力模块
    }
    
    // 检查网络连接状态
    if (target_ip != NULL) {
        nm_status_t status = nm_get_status(target_ip);
        network_available = (status == NM_STATUS_UP);
        
        if (s_controller.config.debug_mode) {
            ESP_LOGI(TAG, "网络状态检查: %s -> %s", target_ip, 
                     network_available ? "可达" : "不可达");
        }
        
        if (!network_available) {
            ESP_LOGW(TAG, "目标设备不可达: %s，跳过HTTP请求", target_ip);
            return ESP_ERR_NOT_FOUND;  // 使用更明确的错误码
        }
    }
    
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = BOARD_HTTP_TIMEOUT_MS,
        .buffer_size = 1024,        // 较小的缓冲区以节省内存
        .buffer_size_tx = 512,      // 发送缓冲区
        .disable_auto_redirect = true,
        .max_redirection_count = 0,
        .keep_alive_enable = false, // 不保持连接以节省资源
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "创建HTTP客户端失败");
        return ESP_FAIL;
    }
    
    // 清空响应缓冲区
    memset(response_buffer, 0, buffer_size);
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP客户端打开失败: %s (目标: %s)", esp_err_to_name(err), target_ip ? target_ip : "未知");
        esp_http_client_cleanup(client);
        
        // 如果是连接失败，可能是网络问题
        if (err == ESP_ERR_HTTP_CONNECT) {
            ESP_LOGW(TAG, "网络连接失败，建议检查:");
            ESP_LOGW(TAG, "  1. 目标设备是否开机且网络正常");
            ESP_LOGW(TAG, "  2. ESP32S3与目标设备网络连通性");
            ESP_LOGW(TAG, "  3. Prometheus服务是否在正确端口运行");
        }
        
        return err;
    }
    
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP请求失败，状态码: %d, URL: %s", status_code, url);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "HTTP请求成功，内容长度: %d字节，目标: %s", content_length, target_ip ? target_ip : "未知");
    }
    
    // 分块读取响应以避免内存不足
    int total_read = 0;
    int chunk_size = 512;  // 512字节块大小
    
    while (total_read < buffer_size - 1) {
        int remaining = buffer_size - 1 - total_read;
        int to_read = (remaining > chunk_size) ? chunk_size : remaining;
        
        int data_read = esp_http_client_read_response(client, 
                                                      response_buffer + total_read, 
                                                      to_read);
        if (data_read <= 0) {
            break;  // 读取完成或出错
        }
        
        total_read += data_read;
        
        // 简单的进度指示 (仅调试模式)
        if (s_controller.config.debug_mode && (total_read % 2048 == 0)) {
            ESP_LOGI(TAG, "已读取: %d字节", total_read);
        }
    }
    
    if (total_read < 0) {
        ESP_LOGE(TAG, "HTTP响应读取失败");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    response_buffer[total_read] = '\0';  // 确保字符串结尾
    
    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "HTTP响应读取完成，总计: %d字节，目标: %s", total_read, target_ip ? target_ip : "未知");
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    
    return ESP_OK;
}

static esp_err_t fetch_n305_temperature(system_metrics_t* metrics) {
    ESP_LOGI(TAG, "使用Prometheus查询API获取N305温度数据");
    
    // 尝试多个可能的温度指标查询
    const char* temp_queries[] = {
        N305_TEMP_QUERY,  // 使用预定义的查询
        "node_hwmon_temp_celsius{chip=\"coretemp-isa-0000\",sensor=\"temp1\"}",
        "node_thermal_zone_temp{zone=\"thermal_zone0\"}",
        "node_hwmon_temp_celsius"
    };
    
    size_t num_queries = sizeof(temp_queries) / sizeof(temp_queries[0]);
    
    for (size_t i = 0; i < num_queries; i++) {
        ESP_LOGI(TAG, "尝试N305温度查询 %d/%d: %s", (int)(i+1), (int)num_queries, temp_queries[i]);
        
        esp_err_t ret = query_prometheus_api(BOARD_PROMETHEUS_API, temp_queries[i], 
                                           s_controller.n305_response_buffer, 
                                           s_controller.buffer_size);
        
        if (ret == ESP_OK) {
            float temperature;
            ret = parse_prometheus_query_response(s_controller.n305_response_buffer, &temperature);
            
            if (ret == ESP_OK && temperature > 0 && temperature < 150) {  // 合理的温度范围
                metrics->n305_cpu_temp = temperature;
                metrics->n305_data_valid = true;
                ESP_LOGI(TAG, "N305温度查询成功: %.1f°C (查询: %s)", temperature, temp_queries[i]);
                return ESP_OK;
            } else if (ret == ESP_OK) {
                ESP_LOGW(TAG, "温度值不合理: %.1f°C", temperature);
            }
        }
        
        // 短暂延迟再尝试下一个查询
        vTaskDelay(pdMS_TO_TICKS(100));
    }
      ESP_LOGW(TAG, "所有N305温度查询都失败");
    return ESP_FAIL;
}

static esp_err_t fetch_jetson_metrics(system_metrics_t* metrics) {
    ESP_LOGI(TAG, "使用Prometheus查询API获取Jetson监控数据");
    
    bool success = false;
    
    // 1. 查询Jetson CPU温度
    ESP_LOGI(TAG, "查询Jetson CPU温度...");
    esp_err_t ret = query_prometheus_api(BOARD_PROMETHEUS_API, JETSON_CPU_TEMP_QUERY, 
                                       s_controller.jetson_response_buffer, 
                                       s_controller.buffer_size);
    if (ret == ESP_OK) {
        float cpu_temp;
        if (parse_prometheus_query_response(s_controller.jetson_response_buffer, &cpu_temp) == ESP_OK) {
            if (cpu_temp >= 0 && cpu_temp < 150) {
                metrics->jetson_cpu_temp = cpu_temp;
                success = true;
                ESP_LOGI(TAG, "Jetson CPU温度: %.1f°C", cpu_temp);
            }
        }
    }
    
    // 2. 查询Jetson GPU温度
    ESP_LOGI(TAG, "查询Jetson GPU温度...");
    ret = query_prometheus_api(BOARD_PROMETHEUS_API, JETSON_GPU_TEMP_QUERY, 
                             s_controller.jetson_response_buffer, 
                             s_controller.buffer_size);
    if (ret == ESP_OK) {
        float gpu_temp;
        if (parse_prometheus_query_response(s_controller.jetson_response_buffer, &gpu_temp) == ESP_OK) {
            if (gpu_temp >= 0 && gpu_temp < 150 && gpu_temp != -256.0f) {  // -256.0表示传感器无效
                metrics->jetson_gpu_temp = gpu_temp;
                success = true;
                ESP_LOGI(TAG, "Jetson GPU温度: %.1f°C", gpu_temp);
            }
        }
    }
      // 3. 查询Jetson功率
    ESP_LOGI(TAG, "查询Jetson功率...");
    float power_mw = -1;
    
    ret = query_prometheus_api(BOARD_PROMETHEUS_API, JETSON_POWER_QUERY, 
                             s_controller.jetson_response_buffer, 
                             s_controller.buffer_size);
    if (ret == ESP_OK) {
        if (parse_prometheus_query_response(s_controller.jetson_response_buffer, &power_mw) == ESP_OK) {
            metrics->jetson_power_mw = power_mw;
            success = true;
            ESP_LOGI(TAG, "Jetson功率: %.1f mW (%.2f W)", power_mw, power_mw/1000.0f);
        }
    }
    
    // 4. 查询内存使用情况
    ESP_LOGI(TAG, "查询Jetson内存使用情况...");
    float memory_total = -1, memory_used = -1;
    
    // 获取总内存
    ret = query_prometheus_api(BOARD_PROMETHEUS_API, JETSON_MEMORY_TOTAL_QUERY, 
                             s_controller.jetson_response_buffer, 
                             s_controller.buffer_size);
    if (ret == ESP_OK) {
        parse_prometheus_query_response(s_controller.jetson_response_buffer, &memory_total);
    }
    
    // 获取已用内存
    ret = query_prometheus_api(BOARD_PROMETHEUS_API, JETSON_MEMORY_USED_QUERY, 
                             s_controller.jetson_response_buffer, 
                             s_controller.buffer_size);
    if (ret == ESP_OK) {
        parse_prometheus_query_response(s_controller.jetson_response_buffer, &memory_used);
    }
    
    if (memory_total > 0 && memory_used >= 0) {
        metrics->jetson_memory_total = memory_total / 1024.0f; // 转换为MB
        metrics->jetson_memory_used = memory_used / 1024.0f;   // 转换为MB
        metrics->jetson_memory_usage = (memory_used / memory_total) * 100.0f;
        success = true;
        ESP_LOGI(TAG, "Jetson内存使用情况: %.1f%% (%.1fMB/%.1fMB)", 
                 metrics->jetson_memory_usage, 
                 metrics->jetson_memory_used, metrics->jetson_memory_total);
    }
    
    if (success) {
        metrics->jetson_data_valid = true;
        ESP_LOGI(TAG, "Jetson监控数据查询完成");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Jetson监控数据查询失败");
        return ESP_FAIL;
    }
}

static esp_err_t query_prometheus_api(const char* base_url, const char* query, char* response_buffer, size_t buffer_size) {
    // 构建完整的查询URL
    char full_url[512];
    int url_len = snprintf(full_url, sizeof(full_url), "%s?query=%s", base_url, query);
    
    if (url_len >= sizeof(full_url)) {
        ESP_LOGE(TAG, "查询URL太长");
        return ESP_ERR_INVALID_ARG;
    }
    
    // URL编码处理（简单替换空格为%20）
    for (int i = 0; i < url_len && i < sizeof(full_url) - 3; i++) {
        if (full_url[i] == ' ') {
            memmove(&full_url[i+3], &full_url[i+1], strlen(&full_url[i+1]) + 1);
            full_url[i] = '%';
            full_url[i+1] = '2';
            full_url[i+2] = '0';
            url_len += 2;
        }
    }
    
    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "查询URL: %s", full_url);
    }
    
    esp_http_client_config_t config = {
        .url = full_url,
        .timeout_ms = BOARD_HTTP_TIMEOUT_MS,
        .buffer_size = 1024,
        .buffer_size_tx = 512,
        .disable_auto_redirect = true,
        .max_redirection_count = 0,
        .keep_alive_enable = false,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "创建HTTP客户端失败");
        return ESP_FAIL;
    }
    
    // 设置请求头
    esp_http_client_set_header(client, "Accept", "application/json");
    
    // 清空响应缓冲区
    memset(response_buffer, 0, buffer_size);
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP客户端打开失败: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }
    
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP请求失败，状态码: %d, URL: %s", status_code, full_url);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "HTTP请求成功，内容长度: %d字节", content_length);
    }
    
    // 读取响应数据
    int total_read = 0;
    int chunk_size = 512;
    
    while (total_read < buffer_size - 1) {
        int remaining = buffer_size - 1 - total_read;
        int to_read = (remaining > chunk_size) ? chunk_size : remaining;
        
        int data_read = esp_http_client_read_response(client, 
                                                      response_buffer + total_read, 
                                                      to_read);
        if (data_read <= 0) {
            break;
        }
        
        total_read += data_read;
    }
    
    response_buffer[total_read] = '\0';
    
    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "读取响应完成，总计: %d字节", total_read);
        ESP_LOGI(TAG, "响应内容: %.500s", response_buffer);  // 打印前500字符
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    
    return ESP_OK;
}

static esp_err_t parse_prometheus_query_response(const char* response, float* value) {
    // 使用cJSON解析Prometheus查询API返回的JSON格式
    // 格式示例: {"status":"success","data":{"resultType":"vector","result":[{"metric":{},"value":[timestamp,"42"]}]}}
    
    if (response == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    cJSON *json = cJSON_Parse(response);
    if (json == NULL) {
        ESP_LOGE(TAG, "JSON解析失败");
        return ESP_FAIL;
    }
    
    // 检查状态
    cJSON *status = cJSON_GetObjectItem(json, "status");
    if (!cJSON_IsString(status) || strcmp(status->valuestring, "success") != 0) {
        ESP_LOGE(TAG, "查询失败，状态: %s", status ? status->valuestring : "未知");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    // 获取数据部分
    cJSON *data = cJSON_GetObjectItem(json, "data");
    if (!cJSON_IsObject(data)) {
        ESP_LOGE(TAG, "响应中没有data字段");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    // 获取result数组
    cJSON *result = cJSON_GetObjectItem(data, "result");
    if (!cJSON_IsArray(result) || cJSON_GetArraySize(result) == 0) {
        ESP_LOGW(TAG, "没有找到查询结果数据");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    // 获取第一个结果项
    cJSON *first_result = cJSON_GetArrayItem(result, 0);
    if (!cJSON_IsObject(first_result)) {
        ESP_LOGE(TAG, "结果格式错误");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    // 获取value数组 [timestamp, "temperature_value"]
    cJSON *value_array = cJSON_GetObjectItem(first_result, "value");
    if (!cJSON_IsArray(value_array) || cJSON_GetArraySize(value_array) < 2) {
        ESP_LOGE(TAG, "value字段格式错误");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    // 获取温度值（第二个元素，字符串格式）
    cJSON *temp_value = cJSON_GetArrayItem(value_array, 1);
    if (!cJSON_IsString(temp_value)) {
        ESP_LOGE(TAG, "温度值不是字符串格式");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    // 转换温度值
    char *endptr;
    float parsed_value = strtof(temp_value->valuestring, &endptr);
    
    if (endptr == temp_value->valuestring) {
        ESP_LOGE(TAG, "温度值解析失败: %s", temp_value->valuestring);
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    *value = parsed_value;
    
    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "JSON解析成功，温度值: %.2f°C", parsed_value);
        
        // 打印metric信息
        cJSON *metric = cJSON_GetObjectItem(first_result, "metric");
        if (cJSON_IsObject(metric)) {
            cJSON *chip = cJSON_GetObjectItem(metric, "chip");
            cJSON *sensor = cJSON_GetObjectItem(metric, "sensor");
            if (cJSON_IsString(chip) && cJSON_IsString(sensor)) {
                ESP_LOGI(TAG, "温度传感器: chip=%s, sensor=%s", chip->valuestring, sensor->valuestring);
            }
        }
    }
    
    cJSON_Delete(json);
    return ESP_OK;
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    // 简单的HTTP事件处理器，可以根据需要扩展
    return ESP_OK;
}

/**
 * @brief 网络连接诊断测试
 * 
 * 检查N305和Jetson的网络连接状态，并提供诊断建议
 */
void bsp_board_ws2812_display_test_network_connectivity(void) {
    ESP_LOGI(TAG, "========== Board WS2812网络连接诊断 ==========");
    
    // 检查网络监控系统状态
    bool network_monitor_available = true;
    nm_status_t test_status = nm_get_status("8.8.8.8");  // 测试网络监控是否工作
    if (test_status == NM_STATUS_UNKNOWN) {
        ESP_LOGW(TAG, "网络监控系统可能未初始化");
        network_monitor_available = false;
    }
    
    if (network_monitor_available) {
        ESP_LOGI(TAG, "网络监控系统状态正常");
        
        // 详细检查各个目标
        const char* target_ips[] = {"10.10.99.99", "10.10.99.98", "10.10.99.100", "8.8.8.8"};
        const char* target_names[] = {"N305应用模块", "Jetson算力模块", "用户主机", "互联网"};
        
        for (int i = 0; i < 4; i++) {
            nm_status_t status = nm_get_status(target_ips[i]);
            const char* status_str = (status == NM_STATUS_UP) ? "可达" : 
                                   (status == NM_STATUS_DOWN) ? "不可达" : "未知";
            
            ESP_LOGI(TAG, "%s (%s): %s", target_names[i], target_ips[i], status_str);
            
            if (status != NM_STATUS_UP) {
                ESP_LOGW(TAG, "  建议检查:");
                ESP_LOGW(TAG, "    - 设备是否开机");
                ESP_LOGW(TAG, "    - 网络线缆连接");
                ESP_LOGW(TAG, "    - IP地址配置");
                if (i < 2) {  // N305和Jetson
                    ESP_LOGW(TAG, "    - Prometheus服务状态");
                }
            }
        }
    } else {
        ESP_LOGW(TAG, "网络监控系统不可用，建议:");
        ESP_LOGW(TAG, "  1. 检查网络初始化");
        ESP_LOGW(TAG, "  2. 检查以太网连接");
        ESP_LOGW(TAG, "  3. 重启网络监控");
    }
      // 测试HTTP连接能力
    ESP_LOGI(TAG, "测试HTTP连接能力...");
      esp_http_client_config_t test_config = {
        .url = BOARD_PROMETHEUS_API,
        .timeout_ms = 2000,  // 较短超时用于快速测试
        .method = HTTP_METHOD_HEAD,  // 只请求头部，减少数据传输
    };
    
    esp_http_client_handle_t test_client = esp_http_client_init(&test_config);
    if (test_client) {
        esp_err_t err = esp_http_client_perform(test_client);
        int status_code = esp_http_client_get_status_code(test_client);
        
        if (err == ESP_OK && status_code == 200) {
            ESP_LOGI(TAG, "N305 HTTP连接测试: 成功 (状态码: %d)", status_code);
        } else {
            ESP_LOGW(TAG, "N305 HTTP连接测试: 失败 (错误: %s, 状态码: %d)", 
                     esp_err_to_name(err), status_code);
        }
        
        esp_http_client_cleanup(test_client);
    }
    
    ESP_LOGI(TAG, "===========================================");
}

// ========== 缺失的函数实现 ==========

static esp_err_t parse_jetson_metrics(const char* data, system_metrics_t* metrics) {
    bool success = false;
    
    // 解析Jetson CPU温度
    float cpu_temp = extract_metric_value(data, "temperature_C{statistic=\"cpu\"}");
    if (cpu_temp >= 0) {
        metrics->jetson_cpu_temp = cpu_temp;
        success = true;
    }
    
    // 解析Jetson GPU温度
    float gpu_temp = extract_metric_value(data, "temperature_C{statistic=\"gpu\"}");
    if (gpu_temp >= 0 && gpu_temp != -256.0f) {  // -256.0表示传感器无效
        metrics->jetson_gpu_temp = gpu_temp;
        success = true;
    }
    
    // 解析GPU使用率 - 优化算法
    // 方法1: 直接从GPU频率计算使用率
    float gpu_freq = extract_metric_value(data, "gpu_utilization_percentage_Hz{nvidia_gpu=\"freq\",statistic=\"gpu\"}");
    float gpu_max_freq = extract_metric_value(data, "gpu_utilization_percentage_Hz{nvidia_gpu=\"max_freq\",statistic=\"gpu\"}");
      // GPU频率使用率计算已弃用，现在使用功率监控替代
    // if (gpu_freq >= 0 && gpu_max_freq > 0) {
    //     float freq_usage = (gpu_freq / gpu_max_freq) * 100.0f;
    //     metrics->jetson_gpu_usage = freq_usage;
    //     success = true;
    //     if (s_controller.config.debug_mode) {
    //         ESP_LOGI(TAG, "GPU频率使用率: %.1f%% (%.0f/%.0f MHz)", 
    //                  freq_usage, gpu_freq/1000000, gpu_max_freq/1000000);
    //     }
    // } else {
    //     // 方法2: 尝试从GPU内存使用率估算
    //     float gpu_mem_used = extract_metric_value(data, "gpuram_kB{nvidia_gpu=\"mem\",statistic=\"gpu\"}");
    //     if (gpu_mem_used >= 0) {
    //         // 如果GPU内存有使用，假设有一定的GPU活动
    //         metrics->jetson_gpu_usage = gpu_mem_used > 0 ? 20.0f : 0.0f;  // 简单估算
    //         success = true;
    //     }
    // }
    
    // 解析内存使用情况 (单位: kB) - 使用更准确的计算
    float memory_total = extract_metric_value(data, "ram_kB{statistic=\"total\"}");
    float memory_used = extract_metric_value(data, "ram_kB{statistic=\"used\"}");
    float memory_buffers = extract_metric_value(data, "ram_kB{statistic=\"buffers\"}");
    float memory_cached = extract_metric_value(data, "ram_kB{statistic=\"cached\"}");
    
    if (memory_total > 0 && memory_used >= 0) {
        metrics->jetson_memory_total = memory_total / 1024.0f; // 转换为MB
        
        // 计算实际使用的内存 (排除buffers和cached)
        float actual_used = memory_used;
        if (memory_buffers >= 0) actual_used -= memory_buffers;
        if (memory_cached >= 0) actual_used -= memory_cached;
        
        // 确保不会出现负值
        if (actual_used < 0) actual_used = memory_used;
        
        metrics->jetson_memory_used = actual_used / 1024.0f;   // 转换为MB
        metrics->jetson_memory_usage = (actual_used / memory_total) * 100.0f;
        success = true;
        
        if (s_controller.config.debug_mode) {
            ESP_LOGI(TAG, "内存使用情况: %.1f%% (%.1fMB/%.1fMB, 实际使用:%.1fMB)", 
                     metrics->jetson_memory_usage, 
                     metrics->jetson_memory_used, metrics->jetson_memory_total,
                     actual_used / 1024.0f);
        }
    }
    
    return success ? ESP_OK : ESP_FAIL;
}

static float extract_metric_value(const char* data, const char* metric_name) {
    char* line_start = strstr(data, metric_name);
    if (line_start == NULL) {
        if (s_controller.config.debug_mode) {
            ESP_LOGW(TAG, "未找到指标: %s", metric_name);
        }
        return -1.0f;
    }
    
    // 找到值的开始位置 (跳过指标名和空格)
    char* value_start = strchr(line_start, ' ');
    if (value_start == NULL) {
        // 尝试查找制表符
        value_start = strchr(line_start, '\t');
        if (value_start == NULL) {
            if (s_controller.config.debug_mode) {
                ESP_LOGW(TAG, "指标格式错误: %s", metric_name);
            }
            return -1.0f;
        }
    }
    
    // 跳过空格和制表符
    while (*value_start == ' ' || *value_start == '\t') {
        value_start++;
    }
    
    // 检查行尾，确保不跨行解析
    char* line_end = strchr(value_start, '\n');
    if (line_end) {
        *line_end = '\0';  // 临时截断以确保安全解析
    }
    
    // 解析浮点数值
    char* endptr;
    float value = strtof(value_start, &endptr);
    
    // 恢复换行符
    if (line_end) {
        *line_end = '\n';
    }
    
    if (endptr == value_start) {
        if (s_controller.config.debug_mode) {
            ESP_LOGW(TAG, "数值解析失败: %s", metric_name);
        }
        return -1.0f;  // 解析失败
    }
    
    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "解析成功 %s = %.2f", metric_name, value);
    }
    
    return value;
}
