#ifndef LED_ANIMATION_H
#define LED_ANIMATION_H

#include <stdint.h>
#include <stdbool.h>

// 闪光动画参数
#define FLASH_WIDTH 2
#define ANIMATION_SPEED 1

// 初始化动画系统
void led_animation_init(void);

// 更新并渲染当前动画
void led_animation_update(void);

// 设置动画点位置和颜色
void led_animation_set_point(int x, int y, uint8_t r, uint8_t g, uint8_t b);

// 更新动画点的颜色
void led_animation_update_point(int x, int y, uint8_t r, uint8_t g, uint8_t b);

// 清除所有动画点
void led_animation_clear_points(void);

// 暂停/继续动画
void led_animation_set_running(bool running);

// 检查动画是否运行中
bool led_animation_is_running(void);

// 设置/获取动画速度
void led_animation_set_speed(uint8_t speed);
uint8_t led_animation_get_speed(void);

#endif // LED_ANIMATION_H
