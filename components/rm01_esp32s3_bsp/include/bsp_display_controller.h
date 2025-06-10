/**
 * @file bsp_display_controller.h
 * @brief BSP显示控制器 - 专门负责Touch WS2812和Board WS2812状态显示
 * 
 * 根据系统状态控制Touch WS2812和Board WS2812显示，不涉及状态检测逻辑
 * 订阅系统状态变化事件，实现状态与显示的解耦
 * 
 * 注意：LED Matrix现在由独立的led_matrix_logo_display模块管理，专门显示Logo
 */

#ifndef BSP_DISPLAY_CONTROLLER_H
#define BSP_DISPLAY_CONTROLLER_H

#include "esp_err.h"
#include "bsp_state_manager.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 显示控制器配置
typedef struct {
    bool auto_switch_enabled;      // 是否启用自动切换
    uint32_t animation_timeout_ms; // 动画超时时间
    bool debug_mode;               // 调试模式
} display_controller_config_t;

// 显示统计信息
typedef struct {
    uint32_t total_switches;       // 总切换次数
    uint32_t last_switch_time;     // 上次切换时间
    system_state_t current_state;  // 当前对应的系统状态
    bool controller_active;        // 控制器是否激活
} display_controller_status_t;

// ========== 核心接口 ==========

/**
 * @brief 初始化显示控制器
 * 
 * @param config 配置参数，NULL使用默认配置
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_display_controller_init(const display_controller_config_t* config);

/**
 * @brief 启动显示控制器
 * 
 * 注册状态变化回调，开始自动显示控制
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_display_controller_start(void);

/**
 * @brief 停止显示控制器
 * 
 * 注销回调，停止自动显示控制
 */
void bsp_display_controller_stop(void);

/**
 * @brief 根据系统状态更新显示
 * 
 * 手动触发显示更新，根据当前系统状态控制Touch WS2812和Board WS2812
 */
void bsp_display_controller_update_display(void);

/**
 * @brief 获取显示控制器状态
 * 
 * @param status 状态信息结构指针
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_display_controller_get_status(display_controller_status_t* status);

/**
 * @brief 打印显示控制器状态
 */
void bsp_display_controller_print_status(void);

// ========== 配置接口 ==========

/**
 * @brief 设置自动切换开关
 * 
 * @param enabled true启用自动切换，false禁用
 */
void bsp_display_controller_set_auto_switch(bool enabled);

/**
 * @brief 设置调试模式
 * 
 * @param debug_mode true启用调试模式，false禁用
 */
void bsp_display_controller_set_debug_mode(bool debug_mode);

/**
 * @brief 获取默认配置
 * 
 * @return display_controller_config_t 默认配置结构
 */
display_controller_config_t bsp_display_controller_get_default_config(void);

// ========== Touch WS2812 显示接口 ==========

/**
 * @brief 获取Touch WS2812显示状态
 * 
 * @param status Touch WS2812显示状态结构指针
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_display_controller_get_touch_ws2812_status(void* status);

/**
 * @brief 手动设置Touch WS2812显示模式
 * 
 * @param mode 显示模式
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_display_controller_set_touch_ws2812_mode(int mode);

/**
 * @brief 设置Touch WS2812颜色
 * 
 * @param r 红色值 (0-255)
 * @param g 绿色值 (0-255)
 * @param b 蓝色值 (0-255)
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_display_controller_set_touch_ws2812_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief 恢复Touch WS2812自动模式
 */
void bsp_display_controller_resume_touch_ws2812_auto(void);

/**
 * @brief 设置Touch WS2812亮度
 * 
 * @param brightness 亮度 (0-255)
 */
void bsp_display_controller_set_touch_ws2812_brightness(uint8_t brightness);

// ========== Board WS2812 显示接口 ==========

/**
 * @brief 获取Board WS2812显示状态
 * 
 * @param status Board WS2812显示状态结构指针
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_display_controller_get_board_ws2812_status(void* status);

/**
 * @brief 手动设置Board WS2812显示模式
 * 
 * @param mode 显示模式
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_display_controller_set_board_ws2812_mode(int mode);

/**
 * @brief 设置Board WS2812颜色
 * 
 * @param r 红色值 (0-255)
 * @param g 绿色值 (0-255)
 * @param b 蓝色值 (0-255)
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_display_controller_set_board_ws2812_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief 恢复Board WS2812自动模式
 */
void bsp_display_controller_resume_board_ws2812_auto(void);

/**
 * @brief 设置Board WS2812亮度
 * 
 * @param brightness 亮度 (0-255)
 */
void bsp_display_controller_set_board_ws2812_brightness(uint8_t brightness);

/**
 * @brief 获取Board WS2812系统监控数据
 * 
 * @param metrics 监控数据结构指针
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_display_controller_get_board_ws2812_metrics(void* metrics);

/**
 * @brief 手动更新Board WS2812监控数据
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_display_controller_update_board_ws2812_metrics(void);

#ifdef __cplusplus
}
#endif

#endif // BSP_DISPLAY_CONTROLLER_H
