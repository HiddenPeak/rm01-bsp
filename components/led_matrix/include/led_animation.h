#ifndef LED_ANIMATION_H
#define LED_ANIMATION_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

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

// ========== 多动画管理接口 ==========

/**
 * @brief 创建新动画槽位
 * 
 * @param name 动画名称，NULL时使用默认名称
 * @return int 动画索引，-1表示失败
 */
int led_animation_create_new(const char* name);

/**
 * @brief 选择当前播放的动画
 * 
 * @param animation_index 动画索引
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t led_animation_select(int animation_index);

/**
 * @brief 获取当前动画索引
 * 
 * @return int 当前动画索引
 */
int led_animation_get_current_index(void);

/**
 * @brief 获取已加载的动画数量
 * 
 * @return int 动画数量
 */
int led_animation_get_count(void);

/**
 * @brief 获取指定动画的名称
 * 
 * @param animation_index 动画索引
 * @return const char* 动画名称，NULL表示无效索引
 */
const char* led_animation_get_name(int animation_index);

/**
 * @brief 切换到下一个动画
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t led_animation_next(void);

/**
 * @brief 切换到上一个动画
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t led_animation_previous(void);

/**
 * @brief 删除指定动画
 * 
 * @param animation_index 动画索引
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t led_animation_delete(int animation_index);

/**
 * @brief 清除所有动画
 */
void led_animation_clear_all(void);

#endif // LED_ANIMATION_H
