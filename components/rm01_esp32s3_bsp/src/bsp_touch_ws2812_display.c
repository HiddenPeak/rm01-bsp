/**
 * @file bsp_touch_ws2812_display.c
 * @brief Touch WS2812 显示控制器实现
 * 
 * 根据系统状态和网络连接状态控制Touch WS2812 LED显示
 * 支持多种颜色模式和动画效果 * - touchu ws2812 的颜色配置
    - 初始化 白色常亮 （最初 3 秒）
    - 报错 3秒后立即检测，超过60秒未ping到 N305 蓝色闪烁
    - 报错 3秒后立即检测，超过60秒未ping到 Jetson 黄色闪烁
    - 警告 3秒后立即检测，超过60秒未ping到 用户主机 绿色闪烁
    - 启动中 白色快速呼吸（启动60秒以后，如果N305、Jetson、用户主机都可以ping到）
    - 无互联网链接待机 白色慢速呼吸（N305、Jetson、用户主机可以ping到，之后240秒以后）
    - 有互联网链接 橙色慢速呼吸（进入待机状态后，忽略其他颜色提示，仅显示该状态）
    - 如果网络状态出现多个未连接，则多种颜色闪烁切换
    * @note 该模块依赖于bsp_ws2812.h和network_monitor.h
 * @note 该模块使用FreeRTOS任务和信号量进行状态管理
 * @note 该模块使用ESP-IDF的日志系统进行调试输出
 * @note 该模块使用ESP-IDF的时间函数进行延时和计时
 * @note 该模块使用ESP-IDF的错误处理机制进行错误返回
 */

#include "bsp_touch_ws2812_display.h"
#include "bsp_ws2812.h"
#include "network_monitor.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *TAG = "TOUCH_WS2812_DISP";

// ========== 颜色定义 ==========

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

// 预定义颜色 - Touch WS2812专用于网络状态显示
static const rgb_color_t COLOR_WHITE = {255, 255, 255}; // 白色 - 初始化/启动/待机
static const rgb_color_t COLOR_BLUE = {0, 0, 255}; // 蓝色 - N305错误
static const rgb_color_t COLOR_YELLOW = {255, 255, 0}; // 黄色 - Jetson错误
static const rgb_color_t COLOR_GREEN = {255, 0, 255}; // 紫色 - 用户主机警告
static const rgb_color_t COLOR_ORANGE = {243, 112, 34}; // 浅橙色 - 有互联网待机
static const rgb_color_t COLOR_OFF = {0, 0, 0}; // 关闭

// 注意：红色用于高温警告，但应该由Board WS2812显示，不在Touch WS2812中使用

// ========== 显示控制器状态结构 ==========

typedef struct {
    touch_display_config_t config;
    touch_display_status_t status;
    bool is_initialized;
    bool manual_mode;
    SemaphoreHandle_t status_mutex;
    TaskHandle_t display_task_handle;
    bool task_running;
    
    // 动画状态
    uint32_t animation_start_time;
    uint32_t last_update_time;
    bool animation_state;  // 用于闪烁状态切换
    uint8_t breath_brightness;  // 呼吸灯亮度
    bool breath_increasing;     // 呼吸灯方向
    
    // 多重错误状态
    uint8_t multi_error_index;
    uint32_t multi_error_last_switch;
    
    // 网络状态缓存
    bool cached_n305_status;
    bool cached_jetson_status;
    bool cached_user_host_status;
    bool cached_internet_status;
    uint32_t last_network_check;
} touch_display_controller_t;

// 全局控制器实例
static touch_display_controller_t s_controller = {0};

// 显示模式名称映射
static const char* MODE_NAMES[] = {
    "初始化模式",
    "N305错误",
    "Jetson错误", 
    "用户主机警告",
    "启动中",
    "无互联网待机",
    "有互联网待机",
    "多重错误",
    "仅互联网连接"
};

// ========== 网络IP地址定义 ==========
// 注意：NM_INTERNET_IP 已在 network_monitor.h 中定义

#define NM_COMPUTING_MODULE_IP   "10.10.99.99"   // N305应用模组 (修正IP地址)
#define NM_APPLICATION_MODULE_IP "10.10.99.98"   // Jetson算力模组 (修正IP地址)
#define NM_USER_HOST_IP          "10.10.99.100"  // 用户主机

// ========== 静态函数声明 ==========

static bool is_touch_display_initialized(void);
static void touch_display_task(void *pvParameters);
static touch_display_mode_t determine_display_mode(void);
static void update_network_status_cache(void);
static void execute_display_mode(touch_display_mode_t mode);
static void set_touch_led_color(uint8_t r, uint8_t g, uint8_t b);
static void handle_blink_animation(const rgb_color_t* color, blink_speed_t speed);
static void handle_breath_animation(const rgb_color_t* color, breath_speed_t speed);
static void handle_multi_error_animation(void);
static uint32_t get_time_ms(void);
static uint8_t apply_brightness(uint8_t color_value, uint8_t brightness);

// ========== 核心接口实现 ==========

esp_err_t bsp_touch_ws2812_display_init(const touch_display_config_t* config) {
    ESP_LOGI(TAG, "初始化Touch WS2812显示控制器");
    
    if (s_controller.is_initialized) {
        ESP_LOGW(TAG, "Touch WS2812显示控制器已初始化");
        return ESP_OK;
    }
    
    // 检查Touch WS2812是否已初始化
    if (bsp_ws2812_get_handle(BSP_WS2812_TOUCH) == NULL) {
        ESP_LOGE(TAG, "Touch WS2812未初始化，请先调用bsp_ws2812_init()");
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
        s_controller.config = bsp_touch_ws2812_display_get_default_config();
    }
    
    // 初始化状态
    memset(&s_controller.status, 0, sizeof(s_controller.status));
    s_controller.status.current_mode = TOUCH_DISPLAY_MODE_INIT;
    s_controller.manual_mode = false;
    s_controller.task_running = false;
    
    // 初始化动画状态
    s_controller.animation_start_time = get_time_ms();
    s_controller.last_update_time = s_controller.animation_start_time;
    s_controller.animation_state = false;
    s_controller.breath_brightness = 0;
    s_controller.breath_increasing = true;
    s_controller.multi_error_index = 0;
    s_controller.multi_error_last_switch = s_controller.animation_start_time;
    
    // 初始化网络状态缓存
    update_network_status_cache();
      // 设置初始显示状态（白色常亮）
    ESP_LOGI(TAG, "设置初始显示状态为白色常亮");
    set_touch_led_color(COLOR_WHITE.r, COLOR_WHITE.g, COLOR_WHITE.b);
    
    s_controller.is_initialized = true;
    
    ESP_LOGI(TAG, "Touch WS2812显示控制器初始化完成");
    ESP_LOGI(TAG, "  自动模式: %s", s_controller.config.auto_mode_enabled ? "启用" : "禁用");
    ESP_LOGI(TAG, "  调试模式: %s", s_controller.config.debug_mode ? "启用" : "禁用");
    ESP_LOGI(TAG, "  亮度: %d", s_controller.config.brightness);
    ESP_LOGI(TAG, "  初始化持续时间: %ld ms", s_controller.config.init_duration_ms);
    
    return ESP_OK;
}

esp_err_t bsp_touch_ws2812_display_start(void) {
    if (!s_controller.is_initialized) {
        ESP_LOGE(TAG, "Touch WS2812显示控制器未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_controller.task_running) {
        ESP_LOGW(TAG, "Touch WS2812显示任务已在运行");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "启动Touch WS2812显示控制器");
    
    // 创建显示任务
    s_controller.task_running = true;
    BaseType_t ret = xTaskCreate(
        touch_display_task,
        "touch_ws2812_display",
        4096,
        NULL,
        4,  // 中等优先级
        &s_controller.display_task_handle
    );
    
    if (ret != pdPASS) {
        s_controller.task_running = false;
        ESP_LOGE(TAG, "创建Touch WS2812显示任务失败");
        return ESP_ERR_NO_MEM;
    }
      // 更新状态
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.is_active = true;
        s_controller.status.system_uptime_ms = get_time_ms();  // 记录显示系统启动时的时间戳
        xSemaphoreGive(s_controller.status_mutex);
    }
    
    ESP_LOGI(TAG, "Touch WS2812显示控制器已启动，使用系统上电时间作为基准");
    return ESP_OK;
}

void bsp_touch_ws2812_display_stop(void) {
    if (!is_touch_display_initialized()) {
        ESP_LOGW(TAG, "Touch WS2812显示控制器未初始化");
        return;
    }
    
    ESP_LOGI(TAG, "停止Touch WS2812显示控制器");
    
    // 停止任务
    s_controller.task_running = false;
    
    if (s_controller.display_task_handle != NULL) {
        vTaskDelete(s_controller.display_task_handle);
        s_controller.display_task_handle = NULL;
    }
    
    // 关闭LED
    bsp_touch_ws2812_display_off();
    
    // 更新状态
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.is_active = false;
        xSemaphoreGive(s_controller.status_mutex);
    }
    
    ESP_LOGI(TAG, "Touch WS2812显示控制器已停止");
}

esp_err_t bsp_touch_ws2812_display_set_mode(touch_display_mode_t mode) {
    if (!is_touch_display_initialized()) {
        ESP_LOGE(TAG, "Touch WS2812显示控制器未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (mode >= TOUCH_DISPLAY_MODE_COUNT) {
        ESP_LOGE(TAG, "无效的显示模式: %d", mode);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "手动设置Touch WS2812显示模式: %s", bsp_touch_ws2812_display_get_mode_name(mode));
    
    // 进入手动模式
    s_controller.manual_mode = true;
    
    // 更新模式
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.previous_mode = s_controller.status.current_mode;
        s_controller.status.current_mode = mode;
        s_controller.status.mode_change_count++;
        s_controller.animation_start_time = get_time_ms();
        xSemaphoreGive(s_controller.status_mutex);
    }
    
    // 立即执行新模式
    execute_display_mode(mode);
    
    return ESP_OK;
}

void bsp_touch_ws2812_display_resume_auto(void) {
    if (!is_touch_display_initialized()) {
        ESP_LOGW(TAG, "Touch WS2812显示控制器未初始化");
        return;
    }
    
    ESP_LOGI(TAG, "恢复Touch WS2812自动显示控制");
    
    // 退出手动模式
    s_controller.manual_mode = false;
    
    // 立即更新显示
    bsp_touch_ws2812_display_update();
}

void bsp_touch_ws2812_display_update(void) {
    if (!s_controller.is_initialized || s_controller.manual_mode) {
        return;
    }
    
    if (!s_controller.config.auto_mode_enabled) {
        ESP_LOGD(TAG, "自动模式已禁用，跳过显示更新");
        return;
    }
    
    // 更新网络状态缓存
    update_network_status_cache();
    
    // 确定当前应该的显示模式
    touch_display_mode_t new_mode = determine_display_mode();
    
    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "更新显示: 当前模式=%s, 新模式=%s", 
                 bsp_touch_ws2812_display_get_mode_name(s_controller.status.current_mode),
                 bsp_touch_ws2812_display_get_mode_name(new_mode));
    }
    
    // 如果模式发生变化，更新状态
    if (new_mode != s_controller.status.current_mode) {
        if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_controller.status.previous_mode = s_controller.status.current_mode;
            s_controller.status.current_mode = new_mode;
            s_controller.status.mode_change_count++;
            s_controller.animation_start_time = get_time_ms();
            
            // 重置动画状态
            s_controller.animation_state = false;
            s_controller.breath_brightness = 0;
            s_controller.breath_increasing = true;
            s_controller.multi_error_index = 0;
            
            xSemaphoreGive(s_controller.status_mutex);
        }
        
        ESP_LOGI(TAG, "Touch WS2812显示模式变化: [%s] -> [%s]", 
                 bsp_touch_ws2812_display_get_mode_name(s_controller.status.previous_mode),
                 bsp_touch_ws2812_display_get_mode_name(new_mode));
    }
}

esp_err_t bsp_touch_ws2812_display_get_status(touch_display_status_t* status) {
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_touch_display_initialized()) {
        ESP_LOGW(TAG, "Touch WS2812显示控制器未初始化，无法获取状态");
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

void bsp_touch_ws2812_display_print_status(void) {
    touch_display_status_t status;
    esp_err_t ret = bsp_touch_ws2812_display_get_status(&status);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取Touch WS2812显示状态失败");
        return;
    }
    
    ESP_LOGI(TAG, "========== Touch WS2812显示状态 ==========");
    ESP_LOGI(TAG, "激活状态: %s", status.is_active ? "是" : "否");
    ESP_LOGI(TAG, "手动模式: %s", s_controller.manual_mode ? "是" : "否");
    ESP_LOGI(TAG, "自动模式: %s", s_controller.config.auto_mode_enabled ? "启用" : "禁用");
    ESP_LOGI(TAG, "调试模式: %s", s_controller.config.debug_mode ? "启用" : "禁用");
    ESP_LOGI(TAG, "当前显示模式: %s", bsp_touch_ws2812_display_get_mode_name(status.current_mode));
    ESP_LOGI(TAG, "前一个模式: %s", bsp_touch_ws2812_display_get_mode_name(status.previous_mode));
    ESP_LOGI(TAG, "模式变化次数: %ld", status.mode_change_count);
    ESP_LOGI(TAG, "在当前模式时间: %ld ms", status.time_in_current_mode);
    ESP_LOGI(TAG, "系统运行时间: %ld ms", status.system_uptime_ms);
    ESP_LOGI(TAG, "网络连接状态:");
    ESP_LOGI(TAG, "  N305: %s", status.n305_connected ? "连接" : "断开");
    ESP_LOGI(TAG, "  Jetson: %s", status.jetson_connected ? "连接" : "断开");
    ESP_LOGI(TAG, "  用户主机: %s", status.user_host_connected ? "连接" : "断开");
    ESP_LOGI(TAG, "  互联网: %s", status.internet_connected ? "连接" : "断开");
    ESP_LOGI(TAG, "========================================");
}

// ========== 配置接口实现 ==========

touch_display_config_t bsp_touch_ws2812_display_get_default_config(void) {
    touch_display_config_t config = {
        .auto_mode_enabled = true,
        .init_duration_ms = 3000,       // 3秒初始化时间（改为3秒）
        .error_timeout_ms = 60000,      // 60秒错误超时
        .standby_delay_ms = 240000,     // 240秒待机延迟
        .debug_mode = false,            // 关闭调试模式，避免影响日志观察
        .brightness = 255               // 设置为最大亮度以便观察
    };
    return config;
}

void bsp_touch_ws2812_display_set_auto_mode(bool enabled) {
    s_controller.config.auto_mode_enabled = enabled;
    ESP_LOGI(TAG, "Touch WS2812自动模式设置为: %s", enabled ? "启用" : "禁用");
}

void bsp_touch_ws2812_display_set_brightness(uint8_t brightness) {
    s_controller.config.brightness = brightness;
    ESP_LOGI(TAG, "Touch WS2812亮度设置为: %d", brightness);
}

void bsp_touch_ws2812_display_set_debug_mode(bool debug_mode) {
    s_controller.config.debug_mode = debug_mode;
    ESP_LOGI(TAG, "Touch WS2812调试模式设置为: %s", debug_mode ? "启用" : "禁用");
}

// ========== 手动控制接口实现 ==========

esp_err_t bsp_touch_ws2812_display_set_color(uint8_t r, uint8_t g, uint8_t b) {
    if (!is_touch_display_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // 启用调试模式以查看详细信息
    bool old_debug = s_controller.config.debug_mode;
    s_controller.config.debug_mode = true;
    
    ESP_LOGI(TAG, "手动设置Touch WS2812颜色: RGB(%d,%d,%d)", r, g, b);
    set_touch_led_color(r, g, b);
    
    // 恢复调试模式设置
    s_controller.config.debug_mode = old_debug;
    
    return ESP_OK;
}

esp_err_t bsp_touch_ws2812_display_set_blink(uint8_t r, uint8_t g, uint8_t b, blink_speed_t speed) {
    if (!is_touch_display_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    rgb_color_t color = {r, g, b};
    handle_blink_animation(&color, speed);
    return ESP_OK;
}

esp_err_t bsp_touch_ws2812_display_set_breath(uint8_t r, uint8_t g, uint8_t b, breath_speed_t speed) {
    if (!is_touch_display_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    rgb_color_t color = {r, g, b};
    handle_breath_animation(&color, speed);
    return ESP_OK;
}

esp_err_t bsp_touch_ws2812_display_off(void) {
    if (!is_touch_display_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return bsp_ws2812_clear(BSP_WS2812_TOUCH);
}

// ========== 工具函数实现 ==========

const char* bsp_touch_ws2812_display_get_mode_name(touch_display_mode_t mode) {
    if (mode >= 0 && mode < TOUCH_DISPLAY_MODE_COUNT) {
        return MODE_NAMES[mode];
    }
    return "未知模式";
}

// ========== 静态函数实现 ==========

static bool is_touch_display_initialized(void) {
    return (s_controller.is_initialized && s_controller.status_mutex != NULL);
}

static void touch_display_task(void *pvParameters) {
    ESP_LOGI(TAG, "Touch WS2812显示任务开始运行");
    
    while (s_controller.task_running) {
        // 如果是自动模式，更新显示状态
        if (!s_controller.manual_mode && s_controller.config.auto_mode_enabled) {
            bsp_touch_ws2812_display_update();
        }
        
        // 执行当前模式的动画
        execute_display_mode(s_controller.status.current_mode);
          // 更新时间信息
        if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            s_controller.status.time_in_current_mode = get_time_ms() - s_controller.animation_start_time;
            // 注意：system_uptime_ms 不应该在这里更新，它应该保持启动时的时间戳
            xSemaphoreGive(s_controller.status_mutex);
        }
        
        // 50ms更新间隔，确保动画流畅
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    ESP_LOGI(TAG, "Touch WS2812显示任务结束");
    vTaskDelete(NULL);
}

static touch_display_mode_t determine_display_mode(void) {    // 直接使用系统启动后的时间，get_time_ms()返回的就是从系统启动开始的毫秒数
    uint32_t system_uptime = get_time_ms();
      // 模式判断的调试信息
    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "确定显示模式: 系统上电后运行时间=%ld ms", system_uptime);
        ESP_LOGI(TAG, "网络状态: N305=%s, Jetson=%s, 用户主机=%s, 互联网=%s", 
                 s_controller.cached_n305_status ? "连接" : "断开",
                 s_controller.cached_jetson_status ? "连接" : "断开",
                 s_controller.cached_user_host_status ? "连接" : "断开",
                 s_controller.cached_internet_status ? "连接" : "断开");
        
        // 特别强调互联网状态
        if (s_controller.cached_internet_status) {
            ESP_LOGI(TAG, "*** 检测到互联网连接! 应显示橙色提示 ***");
        }
    }
    
    // 更新网络状态到显示状态结构
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_controller.status.n305_connected = s_controller.cached_n305_status;
        s_controller.status.jetson_connected = s_controller.cached_jetson_status;
        s_controller.status.user_host_connected = s_controller.cached_user_host_status;
        s_controller.status.internet_connected = s_controller.cached_internet_status;
        xSemaphoreGive(s_controller.status_mutex);
    }    // 1. 初始化阶段（最初3秒）
    if (system_uptime < s_controller.config.init_duration_ms) {
        if (s_controller.config.debug_mode) {
            ESP_LOGI(TAG, "处于初始化阶段: %ld ms < %ld ms", system_uptime, s_controller.config.init_duration_ms);
        }
        return TOUCH_DISPLAY_MODE_INIT;
    }
      // 2. 实时网络状态检测（3秒后开始）
    bool n305_error = !s_controller.cached_n305_status;
    bool jetson_error = !s_controller.cached_jetson_status;
    bool user_host_warning = !s_controller.cached_user_host_status;
    
    int error_count = 0;
    if (n305_error) error_count++;
    if (jetson_error) error_count++;
    if (user_host_warning) error_count++;    // 在60秒内：显示实时网络错误状态（无需等待超时）
    if (system_uptime < s_controller.config.error_timeout_ms) {
        // 检查是否存在错误状态 + 互联网连接的组合情况
        // 如果有任何网络错误且同时有互联网连接，进入多重警告模式
        if (s_controller.cached_internet_status && error_count > 0) {
            if (s_controller.config.debug_mode) {
                ESP_LOGI(TAG, "选择显示模式: %s (检测到%d个网络错误 + 互联网连接)", 
                         bsp_touch_ws2812_display_get_mode_name(TOUCH_DISPLAY_MODE_MULTI_ERROR), error_count);
            }
            return TOUCH_DISPLAY_MODE_MULTI_ERROR;
        }
        
        // 如果有多个错误但没有互联网，显示多重错误模式
        if (error_count >= 2) {
            if (s_controller.config.debug_mode) {
                ESP_LOGI(TAG, "选择显示模式: %s (实时检测到%d个错误)", 
                         bsp_touch_ws2812_display_get_mode_name(TOUCH_DISPLAY_MODE_MULTI_ERROR), error_count);
            }
            return TOUCH_DISPLAY_MODE_MULTI_ERROR;
        }
        // 显示单一错误状态（实时反馈）
        if (n305_error) {
            if (s_controller.config.debug_mode) {
                ESP_LOGI(TAG, "选择显示模式: %s (实时检测)", 
                         bsp_touch_ws2812_display_get_mode_name(TOUCH_DISPLAY_MODE_N305_ERROR));
            }
            return TOUCH_DISPLAY_MODE_N305_ERROR;
        }
        if (jetson_error) {
            if (s_controller.config.debug_mode) {
                ESP_LOGI(TAG, "选择显示模式: %s (实时检测)", 
                         bsp_touch_ws2812_display_get_mode_name(TOUCH_DISPLAY_MODE_JETSON_ERROR));
            }
            return TOUCH_DISPLAY_MODE_JETSON_ERROR;
        }
        if (user_host_warning) {
            if (s_controller.config.debug_mode) {
                ESP_LOGI(TAG, "选择显示模式: %s (实时检测)", 
                         bsp_touch_ws2812_display_get_mode_name(TOUCH_DISPLAY_MODE_USER_HOST_WARNING));
            }
            return TOUCH_DISPLAY_MODE_USER_HOST_WARNING;
        }
        
        // 如果N305和Jetson都可ping到，立即进入启动中状态
        if (s_controller.cached_n305_status && s_controller.cached_jetson_status) {
            if (s_controller.config.debug_mode) {
                ESP_LOGI(TAG, "选择显示模式: %s (N305和Jetson已连接)", 
                         bsp_touch_ws2812_display_get_mode_name(TOUCH_DISPLAY_MODE_STARTUP));
            }
            return TOUCH_DISPLAY_MODE_STARTUP;
        }
        
        // 如果在60秒内没有错误且没有网络连接，继续初始化状态
        if (s_controller.config.debug_mode) {
            ESP_LOGI(TAG, "选择显示模式: %s (60秒内等待网络连接)", 
                     bsp_touch_ws2812_display_get_mode_name(TOUCH_DISPLAY_MODE_INIT));
        }
        return TOUCH_DISPLAY_MODE_INIT;
    } else {
        // 60秒后：继续显示持续的错误状态
        if (error_count >= 2) {
            if (s_controller.config.debug_mode) {
                ESP_LOGI(TAG, "选择显示模式: %s (持续超时，%d个错误)", 
                         bsp_touch_ws2812_display_get_mode_name(TOUCH_DISPLAY_MODE_MULTI_ERROR), error_count);
            }
            return TOUCH_DISPLAY_MODE_MULTI_ERROR;
        }
        if (n305_error) {
            if (s_controller.config.debug_mode) {
                ESP_LOGI(TAG, "选择显示模式: %s (持续超时)", 
                         bsp_touch_ws2812_display_get_mode_name(TOUCH_DISPLAY_MODE_N305_ERROR));
            }
            return TOUCH_DISPLAY_MODE_N305_ERROR;
        }
        if (jetson_error) {
            if (s_controller.config.debug_mode) {
                ESP_LOGI(TAG, "选择显示模式: %s (持续超时)", 
                         bsp_touch_ws2812_display_get_mode_name(TOUCH_DISPLAY_MODE_JETSON_ERROR));
            }
            return TOUCH_DISPLAY_MODE_JETSON_ERROR;
        }
        if (user_host_warning) {
            if (s_controller.config.debug_mode) {
                ESP_LOGI(TAG, "选择显示模式: %s (持续超时)", 
                         bsp_touch_ws2812_display_get_mode_name(TOUCH_DISPLAY_MODE_USER_HOST_WARNING));
            }
            return TOUCH_DISPLAY_MODE_USER_HOST_WARNING;
        }
    }// 3. 启动中状态（60秒后且N305、Jetson都可ping到）
    // 注意：用户主机连接状态不影响启动状态判断，因为用户主机是可选的
    if (system_uptime >= s_controller.config.error_timeout_ms &&
        s_controller.cached_n305_status && 
        s_controller.cached_jetson_status) {
        
        // 如果还没到待机时间，保持启动状态
        if (system_uptime < s_controller.config.standby_delay_ms) {
            return TOUCH_DISPLAY_MODE_STARTUP;
        }
        
        // 4. 待机状态（240秒后）
        // 只有当N305和Jetson都连接时才进入待机状态
        if (s_controller.cached_internet_status) {
            return TOUCH_DISPLAY_MODE_STANDBY_WITH_INTERNET;
        } else {
            return TOUCH_DISPLAY_MODE_STANDBY_NO_INTERNET;
        }    }
    
    // 如果到达这里，说明逻辑有问题，作为兜底返回启动状态
    if (s_controller.config.debug_mode) {
        ESP_LOGW(TAG, "未预期的逻辑分支，默认返回启动状态");
    }
    return TOUCH_DISPLAY_MODE_STARTUP;
}

static void update_network_status_cache(void) {
    uint32_t current_time = get_time_ms();
    
    // 每1秒更新一次网络状态缓存
    if (current_time - s_controller.last_network_check < 1000) {
        return;
    }
    
    // 获取网络状态
    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "开始更新网络状态缓存...");
    }
    
    nm_status_t n305_status = nm_get_status(NM_COMPUTING_MODULE_IP);
    nm_status_t jetson_status = nm_get_status(NM_APPLICATION_MODULE_IP);
    nm_status_t user_host_status = nm_get_status(NM_USER_HOST_IP);
    nm_status_t internet_status = nm_get_status(NM_INTERNET_IP);
    
    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "原始网络状态查询结果:");
        ESP_LOGI(TAG, "  N305 (%s): %d", NM_COMPUTING_MODULE_IP, n305_status);
        ESP_LOGI(TAG, "  Jetson (%s): %d", NM_APPLICATION_MODULE_IP, jetson_status);
        ESP_LOGI(TAG, "  用户主机 (%s): %d", NM_USER_HOST_IP, user_host_status);
        ESP_LOGI(TAG, "  互联网 (%s): %d", NM_INTERNET_IP, internet_status);
    }
    
    // 保存之前的状态用于比较
    bool prev_internet_status = s_controller.cached_internet_status;
    
    s_controller.cached_n305_status = (n305_status == NM_STATUS_UP);
    s_controller.cached_jetson_status = (jetson_status == NM_STATUS_UP);
    s_controller.cached_user_host_status = (user_host_status == NM_STATUS_UP);
    s_controller.cached_internet_status = (internet_status == NM_STATUS_UP);
    
    s_controller.last_network_check = current_time;
    
    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "网络状态更新: N305=%s, Jetson=%s, 用户主机=%s, 互联网=%s",
                 s_controller.cached_n305_status ? "连接" : "断开",
                 s_controller.cached_jetson_status ? "连接" : "断开",
                 s_controller.cached_user_host_status ? "连接" : "断开",
                 s_controller.cached_internet_status ? "连接" : "断开");
        
        // 特别提示互联网连接状态变化
        if (prev_internet_status != s_controller.cached_internet_status) {
            if (s_controller.cached_internet_status) {
                ESP_LOGI(TAG, "*** 互联网连接已建立! ***");
            } else {
                ESP_LOGI(TAG, "*** 互联网连接已断开! ***");
            }
        }
    }
}

static void execute_display_mode(touch_display_mode_t mode) {
    switch (mode) {
        case TOUCH_DISPLAY_MODE_INIT:
            // 白色常亮
            set_touch_led_color(COLOR_WHITE.r, COLOR_WHITE.g, COLOR_WHITE.b);
            break;
            
        case TOUCH_DISPLAY_MODE_N305_ERROR:
            // 蓝色闪烁
            handle_blink_animation(&COLOR_BLUE, BLINK_SPEED_NORMAL);
            break;
            
        case TOUCH_DISPLAY_MODE_JETSON_ERROR:
            // 黄色闪烁
            handle_blink_animation(&COLOR_YELLOW, BLINK_SPEED_NORMAL);
            break;
            
        case TOUCH_DISPLAY_MODE_USER_HOST_WARNING:
            // 绿色闪烁
            handle_blink_animation(&COLOR_GREEN, BLINK_SPEED_NORMAL);
            break;
              case TOUCH_DISPLAY_MODE_STARTUP:
            // 启动中状态：如果有互联网连接显示橙色快速呼吸，否则显示白色快速呼吸
            if (s_controller.cached_internet_status) {
                handle_breath_animation(&COLOR_ORANGE, BREATH_SPEED_FAST);
            } else {
                handle_breath_animation(&COLOR_WHITE, BREATH_SPEED_FAST);
            }
            break;
            
        case TOUCH_DISPLAY_MODE_STANDBY_NO_INTERNET:
            // 白色慢速呼吸
            handle_breath_animation(&COLOR_WHITE, BREATH_SPEED_SLOW);
            break;
            
        case TOUCH_DISPLAY_MODE_STANDBY_WITH_INTERNET:
            // 橙色慢速呼吸
            handle_breath_animation(&COLOR_ORANGE, BREATH_SPEED_SLOW);
            break;
            
        case TOUCH_DISPLAY_MODE_MULTI_ERROR:
            // 多种颜色闪烁切换
            handle_multi_error_animation();
            break;
            
        case TOUCH_DISPLAY_MODE_INTERNET_ONLY:
            // 仅互联网连接 - 橙色闪烁
            handle_blink_animation(&COLOR_ORANGE, BLINK_SPEED_NORMAL);
            break;
            
        default:
            // 默认关闭
            set_touch_led_color(COLOR_OFF.r, COLOR_OFF.g, COLOR_OFF.b);
            break;
    }
}

static void set_touch_led_color(uint8_t r, uint8_t g, uint8_t b) {
    // 应用亮度调整
    uint8_t adj_r = apply_brightness(r, s_controller.config.brightness);
    uint8_t adj_g = apply_brightness(g, s_controller.config.brightness);
    uint8_t adj_b = apply_brightness(b, s_controller.config.brightness);
    
    // 注释掉高频debug信息，避免影响其他调试信息
    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "设置Touch WS2812颜色: RGB(%d,%d,%d) -> 调整后RGB(%d,%d,%d) [亮度:%d]", 
                 r, g, b, adj_r, adj_g, adj_b, s_controller.config.brightness);
    }
    
    esp_err_t ret = bsp_ws2812_set_pixel(BSP_WS2812_TOUCH, 0, adj_r, adj_g, adj_b);
    if (ret == ESP_OK) {
        bsp_ws2812_refresh(BSP_WS2812_TOUCH);
        // 注释掉高频debug信息，避免影响其他调试信息
        if (s_controller.config.debug_mode) {
            ESP_LOGI(TAG, "Touch WS2812刷新成功");
        }
    } else {
        ESP_LOGE(TAG, "Touch WS2812设置失败: %s", esp_err_to_name(ret));
    }
}

static void handle_blink_animation(const rgb_color_t* color, blink_speed_t speed) {
    uint32_t current_time = get_time_ms();
    uint32_t blink_interval;
    
    // 根据速度设置闪烁间隔
    switch (speed) {
        case BLINK_SPEED_SLOW:
            blink_interval = 1000;
            break;
        case BLINK_SPEED_NORMAL:
            blink_interval = 500;
            break;
        case BLINK_SPEED_FAST:
            blink_interval = 200;
            break;
        case BLINK_SPEED_VERY_FAST:
            blink_interval = 100;
            break;
        default:
            blink_interval = 500;
            break;
    }
    
    // 检查是否需要切换状态
    if (current_time - s_controller.last_update_time >= blink_interval) {
        s_controller.animation_state = !s_controller.animation_state;
        s_controller.last_update_time = current_time;
        
        if (s_controller.animation_state) {
            set_touch_led_color(color->r, color->g, color->b);
        } else {
            set_touch_led_color(COLOR_OFF.r, COLOR_OFF.g, COLOR_OFF.b);
        }
    }
}

static void handle_breath_animation(const rgb_color_t* color, breath_speed_t speed) {
    uint32_t current_time = get_time_ms();
    uint32_t breath_period;
    
    // 根据速度设置呼吸周期
    switch (speed) {
        case BREATH_SPEED_SLOW:
            breath_period = 3000;
            break;
        case BREATH_SPEED_NORMAL:
            breath_period = 2000;
            break;
        case BREATH_SPEED_FAST:
            breath_period = 1000;
            break;
        default:
            breath_period = 2000;
            break;
    }
    
    // 计算呼吸亮度
    uint32_t time_in_cycle = (current_time - s_controller.animation_start_time) % breath_period;
    float progress = (float)time_in_cycle / breath_period;
    
    // 使用正弦波生成平滑的呼吸效果
    float brightness_factor = (sinf(progress * 2 * M_PI) + 1.0f) / 2.0f;
    uint8_t brightness = (uint8_t)(brightness_factor * 255);
    
    // 计算调整后的颜色
    uint8_t adj_r = (color->r * brightness) / 255;
    uint8_t adj_g = (color->g * brightness) / 255;
    uint8_t adj_b = (color->b * brightness) / 255;
    
    set_touch_led_color(adj_r, adj_g, adj_b);
}

static void handle_multi_error_animation(void) {
    uint32_t current_time = get_time_ms();
    
    // 每500ms切换一种颜色
    if (current_time - s_controller.multi_error_last_switch >= 500) {
        s_controller.multi_error_last_switch = current_time;
        
        // 构建需要显示的颜色列表
        const rgb_color_t* colors[4];
        int color_count = 0;
        
        // 根据错误状态添加对应颜色
        if (!s_controller.cached_n305_status) {
            colors[color_count++] = &COLOR_BLUE;  // N305错误 - 蓝色
        }
        if (!s_controller.cached_jetson_status) {
            colors[color_count++] = &COLOR_YELLOW;  // Jetson错误 - 黄色
        }
        if (!s_controller.cached_user_host_status) {
            colors[color_count++] = &COLOR_GREEN;  // 用户主机警告 - 绿色
        }
        
        // 如果有互联网连接，添加橙色作为警告
        if (s_controller.cached_internet_status) {
            colors[color_count++] = &COLOR_ORANGE;  // 互联网连接提示 - 橙色
        }
        
        // 确保至少有一种颜色
        if (color_count == 0) {
            colors[0] = &COLOR_WHITE;
            color_count = 1;
        }
        
        const rgb_color_t* current_color = colors[s_controller.multi_error_index % color_count];
        
        // 切换到下一个颜色
        s_controller.multi_error_index++;
        
        // 闪烁效果（在颜色和关闭之间切换）
        if (s_controller.animation_state) {
            set_touch_led_color(current_color->r, current_color->g, current_color->b);
        } else {
            set_touch_led_color(COLOR_OFF.r, COLOR_OFF.g, COLOR_OFF.b);
        }
        
        s_controller.animation_state = !s_controller.animation_state;
    }
}

static uint32_t get_time_ms(void) {
    return esp_timer_get_time() / 1000;
}

static uint8_t apply_brightness(uint8_t color_value, uint8_t brightness) {
    return (color_value * brightness) / 255;
}
