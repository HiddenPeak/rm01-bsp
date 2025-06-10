/**
 * @file bsp_display_controller.c
 * @brief BSP显示控制器实现 - 专门负责Touch WS2812和Board WS2812状态显示
 * 
 * 根据系统状态控制Touch WS2812和Board WS2812显示，不涉及状态检测逻辑
 * 订阅系统状态变化事件，实现状态与显示的解耦
 * 
 * 注意：LED Matrix现在由独立的led_matrix_logo_display模块管理，专门显示Logo
 */

#include "bsp_display_controller.h"
#include "bsp_touch_ws2812_display.h"  // Touch WS2812显示支持
#include "bsp_board_ws2812_display.h"  // Board WS2812显示支持
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "BSP_DISP_CTRL";

// 显示控制器状态结构
typedef struct {
    display_controller_config_t config;
    display_controller_status_t status;
    bool is_initialized;
    bool manual_mode;
    SemaphoreHandle_t status_mutex;
} bsp_display_controller_t;

// 全局显示控制器实例
static bsp_display_controller_t s_controller = {0};

// 检查显示控制器是否已初始化
static bool is_display_controller_initialized(void) {
    return (s_controller.is_initialized && s_controller.status_mutex != NULL);
}

// 静态函数声明
static void state_change_callback(system_state_t old_state, system_state_t new_state, void* user_data);
static esp_err_t update_displays_for_state(system_state_t state);
static uint32_t get_time_ms(void);

// ========== 核心接口实现 ==========

esp_err_t bsp_display_controller_init(const display_controller_config_t* config) {
    if (s_controller.is_initialized) {
        ESP_LOGW(TAG, "显示控制器已经初始化");
        return ESP_OK;
    }

    // 使用默认配置或用户配置
    if (config) {
        s_controller.config = *config;
    } else {
        s_controller.config = bsp_display_controller_get_default_config();
    }

    // 创建状态互斥锁
    s_controller.status_mutex = xSemaphoreCreateMutex();
    if (!s_controller.status_mutex) {
        ESP_LOGE(TAG, "创建状态互斥锁失败");
        return ESP_ERR_NO_MEM;
    }

    // 初始化状态信息
    memset(&s_controller.status, 0, sizeof(s_controller.status));
    s_controller.status.controller_active = false;
    s_controller.manual_mode = false;

    // 初始化Touch WS2812显示控制器
    ESP_LOGI(TAG, "初始化Touch WS2812显示控制器");
    esp_err_t ret = bsp_touch_ws2812_display_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Touch WS2812显示控制器初始化失败: %s", esp_err_to_name(ret));
    }

    // 初始化Board WS2812显示控制器
    ESP_LOGI(TAG, "初始化Board WS2812显示控制器");
    ret = bsp_board_ws2812_display_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Board WS2812显示控制器初始化失败: %s", esp_err_to_name(ret));
    }

    s_controller.is_initialized = true;
    
    ESP_LOGI(TAG, "显示控制器初始化完成 (Touch WS2812 + Board WS2812)");
    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "调试模式已启用");
    }

    return ESP_OK;
}

esp_err_t bsp_display_controller_start(void) {
    if (!s_controller.is_initialized) {
        ESP_LOGE(TAG, "显示控制器未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    // 注册状态变化回调
    esp_err_t ret = bsp_state_manager_register_callback(state_change_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册状态变化回调失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 启动Touch WS2812显示控制器
    ESP_LOGI(TAG, "启动Touch WS2812显示控制器");
    ret = bsp_touch_ws2812_display_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Touch WS2812显示控制器启动失败: %s", esp_err_to_name(ret));
    }

    // 启动Board WS2812显示控制器
    ESP_LOGI(TAG, "启动Board WS2812显示控制器");
    ret = bsp_board_ws2812_display_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Board WS2812显示控制器启动失败: %s", esp_err_to_name(ret));
    }

    // 更新状态
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.controller_active = true;
        s_controller.status.current_state = bsp_state_manager_get_current_state();
        xSemaphoreGive(s_controller.status_mutex);
    }

    // 根据当前状态更新显示
    bsp_display_controller_update_display();

    ESP_LOGI(TAG, "显示控制器已启动");
    return ESP_OK;
}

void bsp_display_controller_stop(void) {
    if (!is_display_controller_initialized()) {
        ESP_LOGW(TAG, "显示控制器未初始化");
        return;
    }

    // 停止Touch WS2812显示控制器
    bsp_touch_ws2812_display_stop();

    // 停止Board WS2812显示控制器
    bsp_board_ws2812_display_stop();

    // 注销状态变化回调
    bsp_state_manager_unregister_callback(state_change_callback);

    // 更新状态
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.controller_active = false;
        xSemaphoreGive(s_controller.status_mutex);
    }

    ESP_LOGI(TAG, "显示控制器已停止");
}

void bsp_display_controller_update_display(void) {
    if (!s_controller.is_initialized || s_controller.manual_mode) {
        return;
    }

    if (!s_controller.config.auto_switch_enabled) {
        ESP_LOGD(TAG, "自动切换已禁用，跳过显示更新");
        return;
    }

    // 获取当前系统状态
    system_state_t current_state = bsp_state_manager_get_current_state();

    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "更新显示: 状态=%s", 
                 bsp_state_manager_get_state_name(current_state));
    }

    esp_err_t ret = update_displays_for_state(current_state);
    if (ret != ESP_OK && s_controller.config.debug_mode) {
        ESP_LOGW(TAG, "显示更新失败: %s", esp_err_to_name(ret));
    }
}

esp_err_t bsp_display_controller_get_status(display_controller_status_t* status) {
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!is_display_controller_initialized()) {
        ESP_LOGW(TAG, "显示控制器未初始化，无法获取状态");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        *status = s_controller.status;
        xSemaphoreGive(s_controller.status_mutex);
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}

void bsp_display_controller_print_status(void) {
    display_controller_status_t status;
    esp_err_t ret = bsp_display_controller_get_status(&status);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取显示控制器状态失败");
        return;
    }

    ESP_LOGI(TAG, "========== 显示控制器状态 ==========");
    ESP_LOGI(TAG, "控制器激活: %s", status.controller_active ? "是" : "否");
    ESP_LOGI(TAG, "手动模式: %s", s_controller.manual_mode ? "是" : "否");
    ESP_LOGI(TAG, "自动切换: %s", s_controller.config.auto_switch_enabled ? "启用" : "禁用");
    ESP_LOGI(TAG, "调试模式: %s", s_controller.config.debug_mode ? "启用" : "禁用");
    ESP_LOGI(TAG, "当前系统状态: %s", bsp_state_manager_get_state_name(status.current_state));
    ESP_LOGI(TAG, "总切换次数: %" PRIu32, status.total_switches);
    ESP_LOGI(TAG, "上次切换时间: %" PRIu32 " ms", status.last_switch_time);
    ESP_LOGI(TAG, "=====================================");

    // 同时打印Touch WS2812和Board WS2812显示状态
    ESP_LOGI(TAG, "");
    bsp_touch_ws2812_display_print_status();
    ESP_LOGI(TAG, "");
    bsp_board_ws2812_display_print_status();
}

// ========== 配置接口实现 ==========

void bsp_display_controller_set_auto_switch(bool enabled) {
    s_controller.config.auto_switch_enabled = enabled;
    ESP_LOGI(TAG, "自动切换设置为: %s", enabled ? "启用" : "禁用");
}

void bsp_display_controller_set_debug_mode(bool debug_mode) {
    s_controller.config.debug_mode = debug_mode;
    ESP_LOGI(TAG, "调试模式设置为: %s", debug_mode ? "启用" : "禁用");
}

display_controller_config_t bsp_display_controller_get_default_config(void) {
    display_controller_config_t config = {
        .auto_switch_enabled = true,
        .animation_timeout_ms = 5000,
        .debug_mode = false
    };
    return config;
}

// ========== Touch WS2812 显示接口实现 ==========

esp_err_t bsp_display_controller_get_touch_ws2812_status(void* status) {
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return bsp_touch_ws2812_display_get_status((touch_display_status_t*)status);
}

esp_err_t bsp_display_controller_set_touch_ws2812_mode(int mode) {
    if (mode < 0 || mode >= 8) {  // TOUCH_DISPLAY_MODE_COUNT 为 8
        return ESP_ERR_INVALID_ARG;
    }
    return bsp_touch_ws2812_display_set_mode((touch_display_mode_t)mode);
}

esp_err_t bsp_display_controller_set_touch_ws2812_color(uint8_t r, uint8_t g, uint8_t b) {
    return bsp_touch_ws2812_display_set_color(r, g, b);
}

void bsp_display_controller_resume_touch_ws2812_auto(void) {
    bsp_touch_ws2812_display_resume_auto();
}

void bsp_display_controller_set_touch_ws2812_brightness(uint8_t brightness) {
    bsp_touch_ws2812_display_set_brightness(brightness);
}

// ========== Board WS2812 显示接口实现 ==========

esp_err_t bsp_display_controller_get_board_ws2812_status(void* status) {
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return bsp_board_ws2812_display_get_status((board_display_status_t*)status);
}

esp_err_t bsp_display_controller_set_board_ws2812_mode(int mode) {
    if (mode < 0 || mode >= BOARD_DISPLAY_MODE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    return bsp_board_ws2812_display_set_mode((board_display_mode_t)mode);
}

esp_err_t bsp_display_controller_set_board_ws2812_color(uint8_t r, uint8_t g, uint8_t b) {
    return bsp_board_ws2812_display_set_color(r, g, b);
}

void bsp_display_controller_resume_board_ws2812_auto(void) {
    bsp_board_ws2812_display_resume_auto();
}

void bsp_display_controller_set_board_ws2812_brightness(uint8_t brightness) {
    bsp_board_ws2812_display_set_brightness(brightness);
}

esp_err_t bsp_display_controller_get_board_ws2812_metrics(void* metrics) {
    if (metrics == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return bsp_board_ws2812_display_get_metrics((system_metrics_t*)metrics);
}

esp_err_t bsp_display_controller_update_board_ws2812_metrics(void) {
    return bsp_board_ws2812_display_update_metrics();
}

// ========== 静态函数实现 ==========

static void state_change_callback(system_state_t old_state, system_state_t new_state, void* user_data) {
    if (!is_display_controller_initialized() || s_controller.manual_mode) {
        return;
    }

    ESP_LOGI(TAG, "收到状态变化通知: [%s] -> [%s]", 
             bsp_state_manager_get_state_name(old_state),
             bsp_state_manager_get_state_name(new_state));

    // 更新当前状态
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.current_state = new_state;
        xSemaphoreGive(s_controller.status_mutex);
    }

    // 根据新状态更新显示
    if (s_controller.config.auto_switch_enabled) {
        esp_err_t ret = update_displays_for_state(new_state);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "显示已更新到新状态");
        } else {
            ESP_LOGW(TAG, "显示更新失败: %s", esp_err_to_name(ret));
        }
    }
}

static esp_err_t update_displays_for_state(system_state_t state) {
    if (!is_display_controller_initialized()) {
        ESP_LOGW(TAG, "显示控制器未初始化，无法更新显示");
        return ESP_ERR_INVALID_STATE;
    }

    // 更新统计信息
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.total_switches++;
        s_controller.status.last_switch_time = get_time_ms();
        xSemaphoreGive(s_controller.status_mutex);
    }

    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "更新Touch WS2812和Board WS2812显示到状态: %s", 
                 bsp_state_manager_get_state_name(state));
    }

    // Touch WS2812和Board WS2812有自己的状态管理，这里不需要做额外操作
    // 它们会自动响应系统状态变化

    return ESP_OK;
}

static uint32_t get_time_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}
