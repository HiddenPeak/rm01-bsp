/**
 * @file led_matrix_logo_display.c
 * @brief LED Matrix Logo显示控制器实现
 * 
 * 专门用于LED Matrix显示Logo动画，独立于系统状态控制
 * 支持从TF卡JSON文件加载多个Logo动画，可以循环播放或定时切换
 */

#include "led_matrix_logo_display.h"
#include "led_matrix.h"
#include "led_animation.h"
#include "led_animation_loader.h"
#include "bsp_storage.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "LED_LOGO_DISPLAY";

// 默认配置值
#define DEFAULT_SWITCH_INTERVAL_MS      5000    // 5秒切换间隔
#define DEFAULT_ANIMATION_SPEED_MS      50      // 50ms动画更新间隔
#define DEFAULT_BRIGHTNESS              128     // 中等亮度
#define DEFAULT_JSON_FILE_PATH          "/sdcard/matrix.json"
#define MAX_LOGO_COUNT                  10      // 最大Logo数量

// Logo显示控制器状态
typedef struct {
    logo_display_config_t config;              // 配置
    logo_display_status_t status;              // 状态
    bool is_initialized;                       // 是否已初始化
    bool is_paused;                           // 是否已暂停
    
    esp_timer_handle_t switch_timer;          // 切换定时器
    esp_timer_handle_t animation_timer;       // 动画更新定时器
    SemaphoreHandle_t status_mutex;           // 状态互斥锁
    
    char json_file_path[256];                 // JSON文件路径
    uint32_t logo_animations[MAX_LOGO_COUNT]; // Logo动画索引数组
    uint32_t logo_count;                      // 实际Logo数量
} logo_display_controller_t;

// 全局控制器实例
static logo_display_controller_t s_controller = {0};

// ========== 静态函数声明 ==========
static void switch_timer_callback(void* arg);
static void animation_timer_callback(void* arg);
static esp_err_t load_logos_from_json(void);
static esp_err_t switch_to_logo_internal(uint32_t logo_index);
static uint32_t get_next_logo_index(void);
static uint32_t get_previous_logo_index(void);
static uint32_t get_time_ms(void);
static void update_next_switch_time(void);
static const char* get_mode_name(logo_display_mode_t mode);

// ========== 核心接口实现 ==========

esp_err_t led_matrix_logo_display_init(const logo_display_config_t* config) {
    if (s_controller.is_initialized) {
        ESP_LOGW(TAG, "Logo显示控制器已经初始化");
        return ESP_OK;
    }

    // 确保LED Matrix基础硬件已经初始化
    ESP_LOGI(TAG, "确保LED Matrix基础硬件已经初始化");
    led_matrix_init();  // 确保基础硬件初始化
    led_animation_init();  // 确保动画系统初始化

    // 使用默认配置或用户配置
    if (config) {
        s_controller.config = *config;
    } else {
        s_controller.config = led_matrix_logo_display_get_default_config();
    }

    // 创建状态互斥锁
    s_controller.status_mutex = xSemaphoreCreateMutex();
    if (!s_controller.status_mutex) {
        ESP_LOGE(TAG, "创建状态互斥锁失败");
        return ESP_ERR_NO_MEM;
    }

    // 设置JSON文件路径
    if (s_controller.config.json_file_path) {
        strncpy(s_controller.json_file_path, s_controller.config.json_file_path, 
                sizeof(s_controller.json_file_path) - 1);
    } else {
        strncpy(s_controller.json_file_path, DEFAULT_JSON_FILE_PATH, 
                sizeof(s_controller.json_file_path) - 1);
    }
    s_controller.json_file_path[sizeof(s_controller.json_file_path) - 1] = '\0';

    // 初始化状态
    memset(&s_controller.status, 0, sizeof(s_controller.status));
    s_controller.status.current_mode = s_controller.config.mode;
    s_controller.is_paused = false;
    s_controller.logo_count = 0;

    // 创建定时器
    esp_timer_create_args_t switch_timer_args = {
        .callback = switch_timer_callback,
        .arg = NULL,
        .name = "logo_switch_timer"
    };
    
    esp_timer_create_args_t animation_timer_args = {
        .callback = animation_timer_callback,
        .arg = NULL,
        .name = "logo_animation_timer"
    };

    esp_err_t ret = esp_timer_create(&switch_timer_args, &s_controller.switch_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建切换定时器失败: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_controller.status_mutex);
        return ret;
    }

    ret = esp_timer_create(&animation_timer_args, &s_controller.animation_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建动画定时器失败: %s", esp_err_to_name(ret));
        esp_timer_delete(s_controller.switch_timer);
        vSemaphoreDelete(s_controller.status_mutex);
        return ret;
    }

    s_controller.is_initialized = true;
    
    ESP_LOGI(TAG, "Logo显示控制器初始化完成");
    ESP_LOGI(TAG, "模式: %s, JSON文件: %s", 
             get_mode_name(s_controller.config.mode), s_controller.json_file_path);

    // 如果配置为自动启动，则启动显示
    if (s_controller.config.auto_start) {
        return led_matrix_logo_display_start();
    }

    return ESP_OK;
}

esp_err_t led_matrix_logo_display_start(void) {
    if (!s_controller.is_initialized) {
        ESP_LOGE(TAG, "Logo显示控制器未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_controller.status.is_running) {
        ESP_LOGW(TAG, "Logo显示已经在运行");
        return ESP_OK;
    }

    // 加载Logo动画
    esp_err_t ret = load_logos_from_json();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "加载Logo动画失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 更新状态
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.is_running = true;
        s_controller.status.last_switch_time = get_time_ms();
        update_next_switch_time();
        xSemaphoreGive(s_controller.status_mutex);
    }

    // 切换到第一个Logo
    if (s_controller.logo_count > 0) {
        ret = switch_to_logo_internal(0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "切换到首个Logo失败: %s", esp_err_to_name(ret));
            s_controller.status.is_running = false;
            return ret;
        }
    }

    // 启动动画更新定时器
    if (s_controller.config.enable_effects) {
        ret = esp_timer_start_periodic(s_controller.animation_timer, 
                                      s_controller.config.animation_speed_ms * 1000);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "启动动画定时器失败: %s", esp_err_to_name(ret));
            s_controller.status.is_running = false;
            return ret;
        }
    }

    // 启动切换定时器（根据模式）
    if (s_controller.config.mode == LOGO_DISPLAY_MODE_SEQUENCE || 
        s_controller.config.mode == LOGO_DISPLAY_MODE_TIMED_SWITCH ||
        s_controller.config.mode == LOGO_DISPLAY_MODE_RANDOM) {
        
        ret = esp_timer_start_periodic(s_controller.switch_timer, 
                                      s_controller.config.switch_interval_ms * 1000);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "启动切换定时器失败: %s", esp_err_to_name(ret));
            esp_timer_stop(s_controller.animation_timer);
            s_controller.status.is_running = false;
            return ret;
        }
    }

    ESP_LOGI(TAG, "Logo显示启动成功，模式: %s，Logo数量: %lu", 
             get_mode_name(s_controller.config.mode), s_controller.logo_count);

    return ESP_OK;
}

void led_matrix_logo_display_stop(void) {
    if (!s_controller.is_initialized || !s_controller.status.is_running) {
        return;
    }

    // 停止定时器
    esp_timer_stop(s_controller.switch_timer);
    esp_timer_stop(s_controller.animation_timer);

    // 更新状态
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.is_running = false;
        xSemaphoreGive(s_controller.status_mutex);
    }

    // 清空LED Matrix显示
    led_matrix_clear();

    ESP_LOGI(TAG, "Logo显示已停止");
}

esp_err_t led_matrix_logo_display_reload(const char* json_file_path) {
    if (!s_controller.is_initialized) {
        ESP_LOGE(TAG, "Logo显示控制器未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    // 更新文件路径
    if (json_file_path) {
        strncpy(s_controller.json_file_path, json_file_path, 
                sizeof(s_controller.json_file_path) - 1);
        s_controller.json_file_path[sizeof(s_controller.json_file_path) - 1] = '\0';
    }

    // 如果正在运行，先停止再重新启动
    bool was_running = s_controller.status.is_running;
    if (was_running) {
        led_matrix_logo_display_stop();
    }

    esp_err_t ret = load_logos_from_json();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "重新加载Logo动画失败: %s", esp_err_to_name(ret));
        return ret;
    }

    if (was_running) {
        return led_matrix_logo_display_start();
    }

    return ESP_OK;
}

esp_err_t led_matrix_logo_display_set_mode(logo_display_mode_t mode) {
    if (!s_controller.is_initialized) {
        ESP_LOGE(TAG, "Logo显示控制器未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (!led_matrix_logo_display_is_mode_supported(mode)) {
        ESP_LOGE(TAG, "不支持的显示模式: %d", mode);
        return ESP_ERR_INVALID_ARG;
    }

    bool was_running = s_controller.status.is_running;
    if (was_running) {
        led_matrix_logo_display_stop();
    }

    s_controller.config.mode = mode;
    
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.current_mode = mode;
        xSemaphoreGive(s_controller.status_mutex);
    }

    ESP_LOGI(TAG, "切换到显示模式: %s", get_mode_name(mode));

    if (was_running) {
        return led_matrix_logo_display_start();
    }

    return ESP_OK;
}

esp_err_t led_matrix_logo_display_switch_to(uint32_t logo_index) {
    if (!s_controller.is_initialized) {
        ESP_LOGE(TAG, "Logo显示控制器未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (logo_index >= s_controller.logo_count) {
        ESP_LOGE(TAG, "Logo索引无效: %lu (最大: %lu)", logo_index, s_controller.logo_count - 1);
        return ESP_ERR_INVALID_ARG;
    }

    return switch_to_logo_internal(logo_index);
}

esp_err_t led_matrix_logo_display_next(void) {
    if (!s_controller.is_initialized || s_controller.logo_count == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t next_index = get_next_logo_index();
    return switch_to_logo_internal(next_index);
}

esp_err_t led_matrix_logo_display_previous(void) {
    if (!s_controller.is_initialized || s_controller.logo_count == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t prev_index = get_previous_logo_index();
    return switch_to_logo_internal(prev_index);
}

// ========== 配置接口实现 ==========

void led_matrix_logo_display_set_switch_interval(uint32_t interval_ms) {
    s_controller.config.switch_interval_ms = interval_ms;
    
    // 如果正在运行且使用定时切换，重启定时器
    if (s_controller.status.is_running && 
        (s_controller.config.mode == LOGO_DISPLAY_MODE_SEQUENCE ||
         s_controller.config.mode == LOGO_DISPLAY_MODE_TIMED_SWITCH ||
         s_controller.config.mode == LOGO_DISPLAY_MODE_RANDOM)) {
        
        esp_timer_stop(s_controller.switch_timer);
        esp_timer_start_periodic(s_controller.switch_timer, interval_ms * 1000);
    }
    
    ESP_LOGI(TAG, "设置切换间隔: %lu ms", interval_ms);
}

void led_matrix_logo_display_set_animation_speed(uint32_t speed_ms) {
    s_controller.config.animation_speed_ms = speed_ms;
    
    // 如果动画定时器正在运行，重启它
    if (s_controller.status.is_running && s_controller.config.enable_effects) {
        esp_timer_stop(s_controller.animation_timer);
        esp_timer_start_periodic(s_controller.animation_timer, speed_ms * 1000);
    }
    
    ESP_LOGI(TAG, "设置动画速度: %lu ms", speed_ms);
}

void led_matrix_logo_display_set_brightness(uint8_t brightness) {
    s_controller.config.brightness = brightness;
    // TODO: 实现亮度控制
    ESP_LOGI(TAG, "设置亮度: %d", brightness);
}

void led_matrix_logo_display_set_effects(bool enable) {
    bool was_enabled = s_controller.config.enable_effects;
    s_controller.config.enable_effects = enable;
    
    if (s_controller.status.is_running) {
        if (enable && !was_enabled) {
            // 启用动画效果
            esp_timer_start_periodic(s_controller.animation_timer, 
                                    s_controller.config.animation_speed_ms * 1000);
        } else if (!enable && was_enabled) {
            // 禁用动画效果
            esp_timer_stop(s_controller.animation_timer);
        }
    }
    
    ESP_LOGI(TAG, "动画效果: %s", enable ? "启用" : "禁用");
}

// ========== 状态查询接口实现 ==========

esp_err_t led_matrix_logo_display_get_status(logo_display_status_t* status) {
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        *status = s_controller.status;
        xSemaphoreGive(s_controller.status_mutex);
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}

void led_matrix_logo_display_print_status(void) {
    logo_display_status_t status;
    if (led_matrix_logo_display_get_status(&status) == ESP_OK) {
        ESP_LOGI(TAG, "=== Logo显示状态 ===");
        ESP_LOGI(TAG, "运行状态: %s", status.is_running ? "运行中" : "已停止");
        ESP_LOGI(TAG, "显示模式: %s", get_mode_name(status.current_mode));
        ESP_LOGI(TAG, "当前Logo: %lu/%lu (%s)", 
                 status.current_logo_index + 1, status.total_logos, status.current_logo_name);
        ESP_LOGI(TAG, "总切换次数: %lu", status.total_switches);
        ESP_LOGI(TAG, "上次切换: %lu ms前", get_time_ms() - status.last_switch_time);
        if (status.next_switch_time > 0) {
            ESP_LOGI(TAG, "下次切换: %lu ms后", 
                     status.next_switch_time > get_time_ms() ? 
                     status.next_switch_time - get_time_ms() : 0);
        }
    }
}

esp_err_t led_matrix_logo_display_get_logo_name(uint32_t logo_index, char* name_buffer, size_t buffer_size) {
    if (!name_buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (logo_index >= s_controller.logo_count) {
        return ESP_ERR_INVALID_ARG;
    }

    // 从动画系统获取Logo名称
    const char* name = led_animation_get_name(s_controller.logo_animations[logo_index]);
    if (name) {
        strncpy(name_buffer, name, buffer_size - 1);
        name_buffer[buffer_size - 1] = '\0';
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

logo_display_config_t led_matrix_logo_display_get_default_config(void) {
    logo_display_config_t config = {
        .mode = LOGO_DISPLAY_MODE_SEQUENCE,
        .switch_interval_ms = DEFAULT_SWITCH_INTERVAL_MS,
        .animation_speed_ms = DEFAULT_ANIMATION_SPEED_MS,
        .auto_start = false,
        .enable_effects = true,
        .brightness = DEFAULT_BRIGHTNESS,
        .json_file_path = DEFAULT_JSON_FILE_PATH
    };
    return config;
}

// ========== 高级接口实现 ==========

esp_err_t led_matrix_logo_display_set_json_file(const char* json_file_path) {
    if (!json_file_path) {
        return ESP_ERR_INVALID_ARG;
    }

    return led_matrix_logo_display_reload(json_file_path);
}

void led_matrix_logo_display_force_update(void) {
    if (s_controller.status.is_running && s_controller.config.enable_effects) {
        led_animation_update();
    }
}

void led_matrix_logo_display_pause(bool pause) {
    if (pause == s_controller.is_paused) {
        return;
    }

    s_controller.is_paused = pause;

    if (s_controller.status.is_running) {
        if (pause) {
            esp_timer_stop(s_controller.switch_timer);
            esp_timer_stop(s_controller.animation_timer);
            ESP_LOGI(TAG, "Logo显示已暂停");
        } else {
            if (s_controller.config.mode == LOGO_DISPLAY_MODE_SEQUENCE ||
                s_controller.config.mode == LOGO_DISPLAY_MODE_TIMED_SWITCH ||
                s_controller.config.mode == LOGO_DISPLAY_MODE_RANDOM) {
                esp_timer_start_periodic(s_controller.switch_timer, 
                                        s_controller.config.switch_interval_ms * 1000);
            }
            
            if (s_controller.config.enable_effects) {
                esp_timer_start_periodic(s_controller.animation_timer, 
                                        s_controller.config.animation_speed_ms * 1000);
            }
            ESP_LOGI(TAG, "Logo显示已恢复");
        }
    }
}

bool led_matrix_logo_display_is_mode_supported(logo_display_mode_t mode) {
    return (mode >= LOGO_DISPLAY_MODE_OFF && mode <= LOGO_DISPLAY_MODE_RANDOM);
}

uint32_t led_matrix_logo_display_get_max_logos(void) {
    return MAX_LOGO_COUNT;
}

// ========== 静态函数实现 ==========

static void switch_timer_callback(void* arg) {
    (void)arg;  // 避免未使用警告

    if (s_controller.is_paused || !s_controller.status.is_running) {
        return;
    }

    uint32_t next_index;
    
    switch (s_controller.config.mode) {
        case LOGO_DISPLAY_MODE_SEQUENCE:
        case LOGO_DISPLAY_MODE_TIMED_SWITCH:
            next_index = get_next_logo_index();
            break;
            
        case LOGO_DISPLAY_MODE_RANDOM:
            // 随机选择，但避免连续显示同一个
            if (s_controller.logo_count > 1) {
                do {
                    next_index = esp_random() % s_controller.logo_count;
                } while (next_index == s_controller.status.current_logo_index);
            } else {
                next_index = 0;
            }
            break;
            
        default:
            return;
    }

    switch_to_logo_internal(next_index);
}

static void animation_timer_callback(void* arg) {
    (void)arg;  // 避免未使用警告

    if (s_controller.is_paused || !s_controller.status.is_running) {
        return;
    }

    led_animation_update();
}

static esp_err_t load_logos_from_json(void) {
    ESP_LOGI(TAG, "从JSON文件加载Logo: %s", s_controller.json_file_path);

    // 检查SD卡是否挂载
    if (!bsp_storage_sdcard_is_mounted()) {
        ESP_LOGE(TAG, "SD卡未挂载");
        return ESP_ERR_INVALID_STATE;
    }

    // 加载JSON动画文件
    esp_err_t ret = load_animation_from_json(s_controller.json_file_path);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "加载JSON动画文件失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 获取加载的动画数量
    int total_animations = led_animation_get_count();
    if (total_animations <= 0) {
        ESP_LOGE(TAG, "没有加载任何动画");
        return ESP_ERR_NOT_FOUND;
    }

    // 构建Logo动画索引数组
    s_controller.logo_count = 0;
    for (int i = 0; i < total_animations && s_controller.logo_count < MAX_LOGO_COUNT; i++) {
        // 假设所有动画都是Logo（也可以根据名称或其他标识过滤）
        s_controller.logo_animations[s_controller.logo_count] = i;
        s_controller.logo_count++;
    }

    // 更新状态
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.total_logos = s_controller.logo_count;
        s_controller.status.current_logo_index = 0;
        xSemaphoreGive(s_controller.status_mutex);
    }

    ESP_LOGI(TAG, "成功加载 %lu 个Logo动画", s_controller.logo_count);
    return ESP_OK;
}

static esp_err_t switch_to_logo_internal(uint32_t logo_index) {
    if (logo_index >= s_controller.logo_count) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t animation_index = s_controller.logo_animations[logo_index];
    
    // 切换到指定动画
    esp_err_t ret = led_animation_select(animation_index);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "切换到动画失败: %s (Logo索引: %lu, 动画索引: %lu)", 
                 esp_err_to_name(ret), logo_index, animation_index);
        return ret;
    }

    // 获取Logo名称
    const char* logo_name = led_animation_get_name(animation_index);
    
    // 更新状态
    if (xSemaphoreTake(s_controller.status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_controller.status.current_logo_index = logo_index;
        s_controller.status.total_switches++;
        s_controller.status.last_switch_time = get_time_ms();
        update_next_switch_time();
        
        if (logo_name) {
            strncpy(s_controller.status.current_logo_name, logo_name, 
                    sizeof(s_controller.status.current_logo_name) - 1);
            s_controller.status.current_logo_name[sizeof(s_controller.status.current_logo_name) - 1] = '\0';
        } else {
            snprintf(s_controller.status.current_logo_name, 
                     sizeof(s_controller.status.current_logo_name), "Logo%lu", logo_index);
        }
        
        xSemaphoreGive(s_controller.status_mutex);
    }

    ESP_LOGI(TAG, "切换到Logo: %s (索引: %lu)", 
             s_controller.status.current_logo_name, logo_index);

    return ESP_OK;
}

static uint32_t get_next_logo_index(void) {
    if (s_controller.logo_count == 0) {
        return 0;
    }
    return (s_controller.status.current_logo_index + 1) % s_controller.logo_count;
}

static uint32_t get_previous_logo_index(void) {
    if (s_controller.logo_count == 0) {
        return 0;
    }
    return (s_controller.status.current_logo_index + s_controller.logo_count - 1) % s_controller.logo_count;
}

static uint32_t get_time_ms(void) {
    return esp_timer_get_time() / 1000;
}

static void update_next_switch_time(void) {
    if (s_controller.config.mode == LOGO_DISPLAY_MODE_SEQUENCE ||
        s_controller.config.mode == LOGO_DISPLAY_MODE_TIMED_SWITCH ||
        s_controller.config.mode == LOGO_DISPLAY_MODE_RANDOM) {
        s_controller.status.next_switch_time = get_time_ms() + s_controller.config.switch_interval_ms;
    } else {
        s_controller.status.next_switch_time = 0;
    }
}

static const char* get_mode_name(logo_display_mode_t mode) {
    switch (mode) {
        case LOGO_DISPLAY_MODE_OFF:          return "关闭";
        case LOGO_DISPLAY_MODE_SINGLE:       return "单个显示";
        case LOGO_DISPLAY_MODE_SEQUENCE:     return "顺序循环";
        case LOGO_DISPLAY_MODE_TIMED_SWITCH: return "定时切换";
        case LOGO_DISPLAY_MODE_RANDOM:       return "随机切换";
        default:                             return "未知";
    }
}

// ========== 状态查询接口实现 ==========

bool led_matrix_logo_display_is_initialized(void) {
    return s_controller.is_initialized;
}

bool led_matrix_logo_display_is_running(void) {
    // 控制器运行状态由初始化状态和非暂停状态共同决定
    return s_controller.is_initialized && !s_controller.is_paused && s_controller.status.is_running;
}
