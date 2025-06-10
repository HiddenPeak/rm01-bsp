/**
 * @file led_matrix_logo_display.h
 * @brief LED Matrix Logo显示控制器
 * 
 * 专门用于LED Matrix显示Logo动画，独立于系统状态控制
 * 支持从TF卡JSON文件加载多个Logo动画，可以循环播放或定时切换
 */

#ifndef LED_MATRIX_LOGO_DISPLAY_H
#define LED_MATRIX_LOGO_DISPLAY_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========== 配置定义 ==========

// Logo显示模式
typedef enum {
    LOGO_DISPLAY_MODE_OFF = 0,          // 关闭显示
    LOGO_DISPLAY_MODE_SINGLE,           // 单个Logo静态显示
    LOGO_DISPLAY_MODE_SEQUENCE,         // 按顺序循环播放
    LOGO_DISPLAY_MODE_TIMED_SWITCH,     // 按时间间隔切换
    LOGO_DISPLAY_MODE_RANDOM            // 随机切换
} logo_display_mode_t;

// Logo显示配置
typedef struct {
    logo_display_mode_t mode;           // 显示模式
    uint32_t switch_interval_ms;        // 切换间隔（毫秒）
    uint32_t animation_speed_ms;        // 动画更新间隔（毫秒）
    bool auto_start;                    // 是否自动启动
    bool enable_effects;                // 是否启用动画效果
    uint8_t brightness;                 // 亮度 (0-255)
    const char* json_file_path;         // JSON文件路径
} logo_display_config_t;

// Logo显示状态
typedef struct {
    bool is_running;                    // 是否正在运行
    uint32_t current_logo_index;        // 当前Logo索引
    uint32_t total_logos;               // 总Logo数量
    uint32_t total_switches;            // 总切换次数
    uint32_t last_switch_time;          // 上次切换时间
    uint32_t next_switch_time;          // 下次切换时间
    logo_display_mode_t current_mode;   // 当前显示模式
    char current_logo_name[64];         // 当前Logo名称
} logo_display_status_t;

// ========== 核心接口 ==========

/**
 * @brief 初始化LED Matrix Logo显示控制器
 * 
 * @param config 配置参数，NULL使用默认配置
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t led_matrix_logo_display_init(const logo_display_config_t* config);

/**
 * @brief 启动Logo显示
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t led_matrix_logo_display_start(void);

/**
 * @brief 停止Logo显示
 */
void led_matrix_logo_display_stop(void);

/**
 * @brief 重新加载Logo动画
 * 
 * @param json_file_path JSON文件路径，NULL使用当前配置的路径
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t led_matrix_logo_display_reload(const char* json_file_path);

/**
 * @brief 设置显示模式
 * 
 * @param mode 显示模式
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t led_matrix_logo_display_set_mode(logo_display_mode_t mode);

/**
 * @brief 手动切换到指定Logo
 * 
 * @param logo_index Logo索引
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t led_matrix_logo_display_switch_to(uint32_t logo_index);

/**
 * @brief 切换到下一个Logo
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t led_matrix_logo_display_next(void);

/**
 * @brief 切换到上一个Logo
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t led_matrix_logo_display_previous(void);

// ========== 配置接口 ==========

/**
 * @brief 设置切换间隔
 * 
 * @param interval_ms 间隔时间（毫秒）
 */
void led_matrix_logo_display_set_switch_interval(uint32_t interval_ms);

/**
 * @brief 设置动画速度
 * 
 * @param speed_ms 动画更新间隔（毫秒）
 */
void led_matrix_logo_display_set_animation_speed(uint32_t speed_ms);

/**
 * @brief 设置亮度
 * 
 * @param brightness 亮度 (0-255)
 */
void led_matrix_logo_display_set_brightness(uint8_t brightness);

/**
 * @brief 启用/禁用动画效果
 * 
 * @param enable true启用，false禁用
 */
void led_matrix_logo_display_set_effects(bool enable);

// ========== 状态查询接口 ==========

/**
 * @brief 获取显示状态
 * 
 * @param status 状态信息结构指针
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t led_matrix_logo_display_get_status(logo_display_status_t* status);

/**
 * @brief 打印显示状态
 */
void led_matrix_logo_display_print_status(void);

/**
 * @brief 获取Logo名称
 * 
 * @param logo_index Logo索引
 * @param name_buffer 名称缓冲区
 * @param buffer_size 缓冲区大小
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t led_matrix_logo_display_get_logo_name(uint32_t logo_index, char* name_buffer, size_t buffer_size);

/**
 * @brief 获取默认配置
 * 
 * @return logo_display_config_t 默认配置结构
 */
logo_display_config_t led_matrix_logo_display_get_default_config(void);

// ========== 高级接口 ==========

/**
 * @brief 设置JSON文件路径
 * 
 * @param json_file_path JSON文件路径
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t led_matrix_logo_display_set_json_file(const char* json_file_path);

/**
 * @brief 强制更新显示
 * 
 * 立即更新当前Logo显示，忽略定时器
 */
void led_matrix_logo_display_force_update(void);

/**
 * @brief 暂停/恢复显示
 * 
 * @param pause true暂停，false恢复
 */
void led_matrix_logo_display_pause(bool pause);

/**
 * @brief 检查是否支持指定显示模式
 * 
 * @param mode 显示模式
 * @return bool true支持，false不支持
 */
bool led_matrix_logo_display_is_mode_supported(logo_display_mode_t mode);

/**
 * @brief 获取支持的Logo数量上限
 * 
 * @return uint32_t 最大支持的Logo数量
 */
uint32_t led_matrix_logo_display_get_max_logos(void);

/**
 * @brief 检查控制器是否已初始化
 * 
 * @return bool true已初始化，false未初始化
 */
bool led_matrix_logo_display_is_initialized(void);

/**
 * @brief 检查控制器是否正在运行
 * 
 * @return bool true正在运行，false已停止
 */
bool led_matrix_logo_display_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // LED_MATRIX_LOGO_DISPLAY_H
