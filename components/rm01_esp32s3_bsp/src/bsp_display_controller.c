/**
 * @file bsp_display_controller.c
 * @brief BSP显示控制器实现 - 专门负责状态显示
 * 
 * 根据系统状态控制LED动画显示，不涉及状态检测逻辑
 * 订阅系统状态变化事件，实现状态与显示的解耦
 */

#include "bsp_display_controller.h"
#include "led_animation.h"
#include "bsp_touch_ws2812_display.h"  // 添加Touch WS2812显示支持
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
    display_animation_index_t state_to_animation_map[SYSTEM_STATE_COUNT];
} bsp_display_controller_t;

// 全局显示控制器实例
static bsp_display_controller_t s_controller = {0};

// 检查显示控制器是否已初始化
static bool is_display_controller_initialized(void) {
    return (s_controller.is_initialized && s_controller.status_mutex != NULL);
}

// 动画名称映射（用于日志输出）
static const char* ANIMATION_NAMES[] = {
    "示例动画",
    "启动动画",
    "链接错误动画",
    "高温警告动画",
    "计算负载动画"
};

// 静态函数声明
static void state_change_callback(system_state_t old_state, system_state_t new_state, void* user_data);
static esp_err_t switch_to_animation(display_animation_index_t animation_index);
static const char* get_animation_name(display_animation_index_t animation_index);
static void initialize_default_mappings(void);
static uint32_t get_time_ms(void);

// ========== 核心接口实现 ==========

esp_err_t bsp_display_controller_init(const display_controller_config_t* config) {
    ESP_LOGI(TAG, "初始化BSP显示控制器");
    
    if (s_controller.is_initialized) {
        ESP_LOGW(TAG, "显示控制器已初始化");
        return ESP_OK;
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
        s_controller.config = bsp_display_controller_get_default_config();
    }
    
    // 初始化状态
    memset(&s_controller.status, 0, sizeof(s_controller.status));
    s_controller.status.current_animation_index = -1;
    s_controller.status.current_state = SYSTEM_STATE_STANDBY;
    s_controller.manual_mode = false;
    
    // 初始化默认状态到动画映射
    initialize_default_mappings();
    
    // 初始化Touch WS2812显示控制器
    ESP_LOGI(TAG, "初始化Touch WS2812显示控制器");
    esp_err_t ret = bsp_touch_ws2812_display_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Touch WS2812显示控制器初始化失败: %s", esp_err_to_name(ret));
        // 不作为致命错误，继续初始化
    } else {
        ESP_LOGI(TAG, "Touch WS2812显示控制器初始化成功");
    }
    
    s_controller.is_initialized = true;
    
    ESP_LOGI(TAG, "BSP显示控制器初始化完成");
    ESP_LOGI(TAG, "  自动切换: %s", s_controller.config.auto_switch_enabled ? "启用" : "禁用");
    ESP_LOGI(TAG, "  调试模式: %s", s_controller.config.debug_mode ? "启用" : "禁用");
    ESP_LOGI(TAG, "  动画超时: %" PRIu32 " ms", s_controller.config.animation_timeout_ms);
    
    return ESP_OK;
}

esp_err_t bsp_display_controller_start(void) {
    if (!s_controller.is_initialized) {
        ESP_LOGE(TAG, "显示控制器未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "启动BSP显示控制器");
    
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
        // 不作为致命错误，继续启动
    } else {
        ESP_LOGI(TAG, "Touch WS2812显示控制器启动成功");
    }
    
    // 根据当前系统状态设置初始显示
    system_state_t current_state = bsp_state_manager_get_current_state();
    display_animation_index_t animation = bsp_display_controller_get_animation_for_state(current_state);
    
    ret = switch_to_animation(animation);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "初始显示设置完成: %s", get_animation_name(animation));
    } else {
        ESP_LOGW(TAG, "初始显示设置失败: %s", esp_err_to_name(ret));
    }
    
    // 更新状态
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.controller_active = true;
        s_controller.status.current_state = current_state;
        xSemaphoreGive(s_controller.status_mutex);
    }
    
    ESP_LOGI(TAG, "BSP显示控制器已启动");
    return ESP_OK;
}

void bsp_display_controller_stop(void) {
    if (!is_display_controller_initialized()) {
        ESP_LOGW(TAG, "显示控制器未初始化");
        return;
    }
    
    ESP_LOGI(TAG, "停止BSP显示控制器");
    
    // 停止Touch WS2812显示控制器
    ESP_LOGI(TAG, "停止Touch WS2812显示控制器");
    bsp_touch_ws2812_display_stop();
    
    // 注销状态变化回调
    bsp_state_manager_unregister_callback(state_change_callback);
    
    // 更新状态
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.controller_active = false;
        xSemaphoreGive(s_controller.status_mutex);
    }
    
    ESP_LOGI(TAG, "BSP显示控制器已停止");
}

esp_err_t bsp_display_controller_set_animation(display_animation_index_t animation_index) {
    if (!is_display_controller_initialized()) {
        ESP_LOGE(TAG, "显示控制器未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "手动设置显示动画: %s", get_animation_name(animation_index));
    
    // 进入手动模式
    s_controller.manual_mode = true;
    
    return switch_to_animation(animation_index);
}

void bsp_display_controller_resume_auto(void) {
    if (!is_display_controller_initialized()) {
        ESP_LOGW(TAG, "显示控制器未初始化");
        return;
    }
    
    ESP_LOGI(TAG, "恢复自动显示控制");
    
    // 退出手动模式
    s_controller.manual_mode = false;
    
    // 立即根据当前状态更新显示
    bsp_display_controller_update_display();
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
    display_animation_index_t animation = bsp_display_controller_get_animation_for_state(current_state);
    
    if (s_controller.config.debug_mode) {
        ESP_LOGI(TAG, "更新显示: 状态=%s, 动画=%s", 
                 bsp_state_manager_get_state_name(current_state),
                 get_animation_name(animation));
    }
    
    esp_err_t ret = switch_to_animation(animation);
    if (ret != ESP_OK && s_controller.config.debug_mode) {
        ESP_LOGW(TAG, "显示更新失败: %s", esp_err_to_name(ret));
    }
}

esp_err_t bsp_display_controller_get_status(display_controller_status_t* status) {
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 检查显示控制器是否已初始化
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
    
    ESP_LOGI(TAG, "========== BSP显示控制器状态 ==========");
    ESP_LOGI(TAG, "控制器激活: %s", status.controller_active ? "是" : "否");
    ESP_LOGI(TAG, "手动模式: %s", s_controller.manual_mode ? "是" : "否");
    ESP_LOGI(TAG, "自动切换: %s", s_controller.config.auto_switch_enabled ? "启用" : "禁用");
    ESP_LOGI(TAG, "调试模式: %s", s_controller.config.debug_mode ? "启用" : "禁用");
    ESP_LOGI(TAG, "当前动画索引: %d (%s)", status.current_animation_index, 
             get_animation_name((display_animation_index_t)status.current_animation_index));
    ESP_LOGI(TAG, "当前系统状态: %s", bsp_state_manager_get_state_name(status.current_state));
    ESP_LOGI(TAG, "总切换次数: %" PRIu32, status.total_switches);
    ESP_LOGI(TAG, "上次切换时间: %" PRIu32 " ms", status.last_switch_time);
    ESP_LOGI(TAG, "====================================");
    
    // 同时打印Touch WS2812显示状态
    ESP_LOGI(TAG, "");
    bsp_touch_ws2812_display_print_status();
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

// ========== 状态映射接口实现 ==========

display_animation_index_t bsp_display_controller_get_animation_for_state(system_state_t state) {
    if (state >= 0 && state < SYSTEM_STATE_COUNT) {
        return s_controller.state_to_animation_map[state];
    }
    return DISPLAY_ANIM_DEMO;  // 默认动画
}

esp_err_t bsp_display_controller_set_state_mapping(system_state_t state, display_animation_index_t animation) {
    if (state >= SYSTEM_STATE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    s_controller.state_to_animation_map[state] = animation;
    ESP_LOGI(TAG, "设置状态映射: %s -> %s", 
             bsp_state_manager_get_state_name(state),
             get_animation_name(animation));
    
    return ESP_OK;
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
    
    // 根据新状态切换显示
    if (s_controller.config.auto_switch_enabled) {
        display_animation_index_t animation = bsp_display_controller_get_animation_for_state(new_state);
        
        esp_err_t ret = switch_to_animation(animation);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "显示已切换到: %s", get_animation_name(animation));
        } else {
            ESP_LOGW(TAG, "显示切换失败: %s", esp_err_to_name(ret));
        }
    }
}

static esp_err_t switch_to_animation(display_animation_index_t animation_index) {
    if (!is_display_controller_initialized()) {
        ESP_LOGW(TAG, "显示控制器未初始化，无法切换动画");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = led_animation_select((int)animation_index);
    
    if (ret == ESP_OK) {
        // 更新统计信息
        if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_controller.status.current_animation_index = (int)animation_index;
            s_controller.status.total_switches++;
            s_controller.status.last_switch_time = get_time_ms();
            xSemaphoreGive(s_controller.status_mutex);
        }
        
        if (s_controller.config.debug_mode) {
            ESP_LOGI(TAG, "成功切换到动画: %s (索引: %d)", 
                     get_animation_name(animation_index), (int)animation_index);
        }
    } else {
        ESP_LOGE(TAG, "切换动画失败: %s (索引: %d)", 
                 esp_err_to_name(ret), (int)animation_index);
    }
    
    return ret;
}

static const char* get_animation_name(display_animation_index_t animation_index) {
    if (animation_index >= 0 && animation_index < 5) {
        return ANIMATION_NAMES[animation_index];
    }
    return "未知动画";
}

static void initialize_default_mappings(void) {
    // 初始化默认的状态到动画映射
    s_controller.state_to_animation_map[SYSTEM_STATE_STANDBY] = DISPLAY_ANIM_DEMO;
    s_controller.state_to_animation_map[SYSTEM_STATE_STARTUP_0] = DISPLAY_ANIM_STARTUP;
    s_controller.state_to_animation_map[SYSTEM_STATE_STARTUP_1] = DISPLAY_ANIM_STARTUP;
    s_controller.state_to_animation_map[SYSTEM_STATE_STARTUP_2] = DISPLAY_ANIM_STARTUP;
    s_controller.state_to_animation_map[SYSTEM_STATE_STARTUP_3] = DISPLAY_ANIM_STARTUP;
    s_controller.state_to_animation_map[SYSTEM_STATE_HIGH_TEMP_1] = DISPLAY_ANIM_HIGH_TEMP;
    s_controller.state_to_animation_map[SYSTEM_STATE_HIGH_TEMP_2] = DISPLAY_ANIM_HIGH_TEMP;
    s_controller.state_to_animation_map[SYSTEM_STATE_USER_HOST_DISCONNECTED] = DISPLAY_ANIM_LINK_ERROR;
    s_controller.state_to_animation_map[SYSTEM_STATE_HIGH_COMPUTE_LOAD] = DISPLAY_ANIM_COMPUTING;
    
    ESP_LOGI(TAG, "默认状态到动画映射已初始化");
}

static uint32_t get_time_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}
