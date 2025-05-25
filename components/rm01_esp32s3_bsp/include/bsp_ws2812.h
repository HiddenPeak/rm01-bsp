#pragma once

#include "esp_err.h"
#include "led_strip.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WS2812 LED配置参数
 */
#define BSP_WS2812_ONBOARD_PIN      42
#define BSP_WS2812_ONBOARD_COUNT    28
#define BSP_WS2812_ARRAY_PIN        9
#define BSP_WS2812_ARRAY_COUNT      1024  // 32x32
#define BSP_WS2812_Touch_LED_PIN    45
#define BSP_WS2812_Touch_LED_COUNT  1

/**
 * @brief WS2812 LED句柄类型定义
 */
typedef enum {
    BSP_WS2812_ONBOARD = 0,    ///< 板载LED
    BSP_WS2812_ARRAY,          ///< LED阵列
    BSP_WS2812_TOUCH,          ///< 触摸LED
    BSP_WS2812_MAX             ///< 最大值，用于数组大小
} bsp_ws2812_type_t;

/**
 * @brief 初始化所有WS2812 LED
 * 
 * @return 
 *     - ESP_OK: 成功
 *     - ESP_FAIL: 失败
 */
esp_err_t bsp_ws2812_init_all(void);

/**
 * @brief 初始化指定类型的WS2812 LED
 * 
 * @param type WS2812类型
 * @return 
 *     - ESP_OK: 成功
 *     - ESP_FAIL: 失败
 */
esp_err_t bsp_ws2812_init(bsp_ws2812_type_t type);

/**
 * @brief 反初始化所有WS2812 LED
 * 
 * @return 
 *     - ESP_OK: 成功
 *     - ESP_FAIL: 失败
 */
esp_err_t bsp_ws2812_deinit_all(void);

/**
 * @brief 反初始化指定类型的WS2812 LED
 * 
 * @param type WS2812类型
 * @return 
 *     - ESP_OK: 成功
 *     - ESP_FAIL: 失败
 */
esp_err_t bsp_ws2812_deinit(bsp_ws2812_type_t type);

/**
 * @brief 设置指定LED的颜色
 * 
 * @param type WS2812类型
 * @param index LED索引
 * @param red 红色值 (0-255)
 * @param green 绿色值 (0-255)
 * @param blue 蓝色值 (0-255)
 * @return 
 *     - ESP_OK: 成功
 *     - ESP_ERR_INVALID_ARG: 参数无效
 *     - ESP_FAIL: 操作失败
 */
esp_err_t bsp_ws2812_set_pixel(bsp_ws2812_type_t type, uint32_t index, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 刷新LED显示
 * 
 * @param type WS2812类型
 * @return 
 *     - ESP_OK: 成功
 *     - ESP_ERR_INVALID_ARG: 参数无效
 *     - ESP_FAIL: 操作失败
 */
esp_err_t bsp_ws2812_refresh(bsp_ws2812_type_t type);

/**
 * @brief 清除所有LED
 * 
 * @param type WS2812类型
 * @return 
 *     - ESP_OK: 成功
 *     - ESP_ERR_INVALID_ARG: 参数无效
 *     - ESP_FAIL: 操作失败
 */
esp_err_t bsp_ws2812_clear(bsp_ws2812_type_t type);

/**
 * @brief 获取WS2812句柄
 * 
 * @param type WS2812类型
 * @return LED strip句柄，如果未初始化则返回NULL
 */
led_strip_handle_t bsp_ws2812_get_handle(bsp_ws2812_type_t type);

/**
 * @brief 板载LED测试函数
 */
void bsp_ws2812_onboard_test(void);

/**
 * @brief LED阵列测试函数
 */
void bsp_ws2812_array_test(void);

/**
 * @brief 触摸LED测试函数
 */
void bsp_ws2812_touch_test(void);

#ifdef __cplusplus
}
#endif
