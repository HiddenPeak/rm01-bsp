/**
 * @file bsp_touch_ws2812_display.h
 * @brief Touch WS2812 显示控制器 - 专门负责触摸LED状态显示
 * 
 * 根据系统状态和网络连接状态控制Touch WS2812 LED显示
 * 支持多种颜色模式和动画效果
 */

#ifndef BSP_TOUCH_WS2812_DISPLAY_H
#define BSP_TOUCH_WS2812_DISPLAY_H

#include "esp_err.h"
#include "bsp_state_manager.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========== 显示模式枚举 ==========

/**
 * @brief Touch WS2812 显示模式
 */
typedef enum {
    TOUCH_DISPLAY_MODE_INIT = 0,            // 初始化模式 - 白色常亮（最初3秒）
    TOUCH_DISPLAY_MODE_N305_ERROR,          // N305错误 - 蓝色闪烁（超过60秒未ping到）
    TOUCH_DISPLAY_MODE_JETSON_ERROR,        // Jetson错误 - 黄色闪烁（超过60秒未ping到）
    TOUCH_DISPLAY_MODE_USER_HOST_WARNING,   // 用户主机警告 - 绿色闪烁（未连接用户主机）
    TOUCH_DISPLAY_MODE_STARTUP,             // 启动中 - 白色快速呼吸（N305/Jetson可ping到）
    TOUCH_DISPLAY_MODE_STANDBY_NO_INTERNET, // 无互联网待机 - 白色慢速呼吸（240秒后）
    TOUCH_DISPLAY_MODE_STANDBY_WITH_INTERNET, // 有互联网 - 橙色慢速呼吸（进入待机状态后）
    TOUCH_DISPLAY_MODE_MULTI_ERROR,         // 多重错误 - 多种颜色闪烁切换
    TOUCH_DISPLAY_MODE_INTERNET_ONLY,       // 仅互联网连接 - 橙色闪烁（核心模块未就绪但有互联网）
    TOUCH_DISPLAY_MODE_COUNT
} touch_display_mode_t;

/**
 * @brief 闪烁速度类型
 */
typedef enum {
    BLINK_SPEED_SLOW = 0,       // 慢速闪烁 (1000ms)
    BLINK_SPEED_NORMAL,         // 正常闪烁 (500ms)
    BLINK_SPEED_FAST,           // 快速闪烁 (200ms)
    BLINK_SPEED_VERY_FAST       // 极快闪烁 (100ms)
} blink_speed_t;

/**
 * @brief 呼吸灯速度类型
 */
typedef enum {
    BREATH_SPEED_SLOW = 0,      // 慢速呼吸 (3000ms周期)
    BREATH_SPEED_NORMAL,        // 正常呼吸 (2000ms周期)
    BREATH_SPEED_FAST           // 快速呼吸 (1000ms周期)
} breath_speed_t;

// ========== 配置结构体 ==========

/**
 * @brief Touch WS2812 显示控制器配置
 */
typedef struct {
    bool auto_mode_enabled;          // 是否启用自动模式
    uint32_t init_duration_ms;       // 初始化持续时间 (默认30秒)
    uint32_t error_timeout_ms;       // 错误判断超时时间 (默认60秒)
    uint32_t standby_delay_ms;       // 进入待机的延迟时间 (默认240秒)
    bool debug_mode;                 // 调试模式
    uint8_t brightness;              // 亮度 (0-255)
} touch_display_config_t;

/**
 * @brief 显示状态信息
 */
typedef struct {
    touch_display_mode_t current_mode;    // 当前显示模式
    touch_display_mode_t previous_mode;   // 前一个显示模式
    uint32_t mode_change_count;           // 模式变化次数
    uint32_t time_in_current_mode;        // 在当前模式的时间（毫秒）
    bool is_active;                       // 是否激活
    bool n305_connected;                  // N305连接状态
    bool jetson_connected;                // Jetson连接状态
    bool user_host_connected;             // 用户主机连接状态
    bool internet_connected;              // 互联网连接状态
    uint32_t system_uptime_ms;            // 系统运行时间
} touch_display_status_t;

// ========== 核心接口 ==========

/**
 * @brief 初始化Touch WS2812显示控制器
 * 
 * @param config 配置参数，NULL使用默认配置
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_touch_ws2812_display_init(const touch_display_config_t* config);

/**
 * @brief 启动Touch WS2812显示控制器
 * 
 * 开始自动显示控制，注册网络状态回调
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_touch_ws2812_display_start(void);

/**
 * @brief 停止Touch WS2812显示控制器
 */
void bsp_touch_ws2812_display_stop(void);

/**
 * @brief 手动设置显示模式
 * 
 * @param mode 显示模式
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_touch_ws2812_display_set_mode(touch_display_mode_t mode);

/**
 * @brief 恢复自动模式
 */
void bsp_touch_ws2812_display_resume_auto(void);

/**
 * @brief 强制更新显示状态
 * 
 * 手动触发状态检查和显示更新
 */
void bsp_touch_ws2812_display_update(void);

/**
 * @brief 获取显示状态
 * 
 * @param status 状态信息结构指针
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_touch_ws2812_display_get_status(touch_display_status_t* status);

/**
 * @brief 打印显示状态信息
 */
void bsp_touch_ws2812_display_print_status(void);

// ========== 配置接口 ==========

/**
 * @brief 获取默认配置
 * 
 * @return touch_display_config_t 默认配置
 */
touch_display_config_t bsp_touch_ws2812_display_get_default_config(void);

/**
 * @brief 设置自动模式开关
 * 
 * @param enabled 是否启用自动模式
 */
void bsp_touch_ws2812_display_set_auto_mode(bool enabled);

/**
 * @brief 设置亮度
 * 
 * @param brightness 亮度 (0-255)
 */
void bsp_touch_ws2812_display_set_brightness(uint8_t brightness);

/**
 * @brief 设置调试模式
 * 
 * @param debug_mode 是否启用调试模式
 */
void bsp_touch_ws2812_display_set_debug_mode(bool debug_mode);

// ========== 手动控制接口 ==========

/**
 * @brief 手动设置颜色（固定颜色）
 * 
 * @param r 红色值 (0-255)
 * @param g 绿色值 (0-255)
 * @param b 蓝色值 (0-255)
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_touch_ws2812_display_set_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief 手动设置闪烁模式
 * 
 * @param r 红色值 (0-255)
 * @param g 绿色值 (0-255)
 * @param b 蓝色值 (0-255)
 * @param speed 闪烁速度
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_touch_ws2812_display_set_blink(uint8_t r, uint8_t g, uint8_t b, blink_speed_t speed);

/**
 * @brief 手动设置呼吸灯模式
 * 
 * @param r 红色值 (0-255)
 * @param g 绿色值 (0-255)
 * @param b 蓝色值 (0-255)
 * @param speed 呼吸速度
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_touch_ws2812_display_set_breath(uint8_t r, uint8_t g, uint8_t b, breath_speed_t speed);

/**
 * @brief 关闭Touch WS2812显示
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_touch_ws2812_display_off(void);

// ========== 工具函数 ==========

/**
 * @brief 获取显示模式名称
 * 
 * @param mode 显示模式
 * @return const char* 模式名称字符串
 */
const char* bsp_touch_ws2812_display_get_mode_name(touch_display_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif // BSP_TOUCH_WS2812_DISPLAY_H
