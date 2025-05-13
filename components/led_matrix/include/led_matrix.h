#ifndef LED_MATRIX_H
#define LED_MATRIX_H

#include <stdint.h>
#include <stdbool.h>
#include "led_color.h"

// 矩阵尺寸定义
#define LED_MATRIX_WIDTH 32
#define LED_MATRIX_HEIGHT 32
#define LED_MATRIX_NUM_LEDS (LED_MATRIX_WIDTH * LED_MATRIX_HEIGHT)

// 灯板GPIO和配置
#define LED_MATRIX_GPIO_PIN 9

// 初始化函数
void led_matrix_init(void);

// 清除所有LED
void led_matrix_clear(void);

// 设置单个像素
void led_matrix_set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);

// 获取单个像素
void led_matrix_get_pixel(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b);

// 更新显示（刷新整个矩阵）
void led_matrix_refresh(void);

// 填充全部
void led_matrix_fill(uint8_t r, uint8_t g, uint8_t b);

// 动画更新（将在动画模块中实现）
void led_matrix_update_animation(void);

// 启用/禁用矩阵更新
void led_matrix_set_enabled(bool enabled);

// 检查矩阵是否启用
bool led_matrix_is_enabled(void);

// 简单测试模式
void led_matrix_test(void);

#endif // LED_MATRIX_H
