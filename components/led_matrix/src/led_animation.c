#include "led_animation.h"
#include "led_matrix.h"
#include "led_color.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "LED_ANIMATION";

// 动画数据
static uint8_t mask[LED_MATRIX_HEIGHT][LED_MATRIX_WIDTH]; // 掩码，标记哪些像素应该被照亮
static uint8_t original_colors[LED_MATRIX_HEIGHT][LED_MATRIX_WIDTH][3]; // 每个点的原始颜色
static int flash_position = 0; // 闪光位置（从 0,0 开始）
static bool animation_running = true; // 动画是否正在运行
static uint8_t animation_speed = ANIMATION_SPEED; // 动画速度

// 初始化动画系统
void led_animation_init(void) {
    // 清空掩码和原始颜色
    memset(mask, 0, sizeof(mask));
    memset(original_colors, 0, sizeof(original_colors));
    
    flash_position = 0;
    animation_running = true;
    animation_speed = ANIMATION_SPEED;
    
    ESP_LOGI(TAG, "动画系统初始化完成");
}

// 设置动画点位置和颜色
void led_animation_set_point(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    // 边界检查
    if (x < 0 || x >= LED_MATRIX_WIDTH || y < 0 || y >= LED_MATRIX_HEIGHT) {
        return;
    }
    
    // 设置掩码和原始颜色
    mask[y][x] = 1;
    original_colors[y][x][0] = r;
    original_colors[y][x][1] = g;
    original_colors[y][x][2] = b;
}

// 更新动画点的颜色
void led_animation_update_point(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    // 边界检查
    if (x < 0 || x >= LED_MATRIX_WIDTH || y < 0 || y >= LED_MATRIX_HEIGHT) {
        return;
    }
    
    // 仅更新颜色，不改变掩码
    original_colors[y][x][0] = r;
    original_colors[y][x][1] = g;
    original_colors[y][x][2] = b;
}

// 清除所有动画点
void led_animation_clear_points(void) {
    memset(mask, 0, sizeof(mask));
    memset(original_colors, 0, sizeof(original_colors));
}

// 计算闪光亮度（基于到闪光中心线的距离）
static float calculate_flash_brightness(int y, int x, int flash_pos) {
    // 到对角线闪光线的距离
    // 对角线闪光线由方程表示：y = x - flash_pos
    // 点(x,y)到线y = x - flash_pos的距离为：
    // |y - x + flash_pos| / sqrt(2)
    float distance = fabsf((float)y - (float)x + (float)flash_pos) / 1.414f;
    
    // 计算亮度 - 中心处为全亮度，向边缘逐渐衰减
    if (distance < FLASH_WIDTH) {
        // 余弦亮度衰减，让光线效果更自然
        return cosf(distance * 3.14159f / (2.0f * FLASH_WIDTH));
    }
    
    return 0.0f; // 闪光宽度外无亮度
}

// 更新并渲染当前动画
void led_animation_update(void) {
    // 如果动画没有运行，不更新
    if (!animation_running) {
        return;
    }
    
    // 清空网格
    for (int y = 0; y < LED_MATRIX_HEIGHT; y++) {
        for (int x = 0; x < LED_MATRIX_WIDTH; x++) {
            if (mask[y][x]) {
                // 应用原始颜色（经过亮度和饱和度调整）
                rgb_t adjusted = adjust_brightness_saturation(
                    original_colors[y][x][0], 
                    original_colors[y][x][1], 
                    original_colors[y][x][2]
                );
                led_matrix_set_pixel(x, y, adjusted.r, adjusted.g, adjusted.b);
            } else {
                led_matrix_set_pixel(x, y, 0, 0, 0);
            }
        }
    }
    
    // 更新闪光位置
    flash_position += animation_speed;
    
    // 当闪光离开屏幕时重置位置
    if (flash_position > LED_MATRIX_WIDTH + LED_MATRIX_HEIGHT + FLASH_WIDTH) {
        flash_position = 0; // 从(0,0)开始
    }
    
    // 应用闪光效果到掩码中的所有像素
    for (int y = 0; y < LED_MATRIX_HEIGHT; y++) {
        for (int x = 0; x < LED_MATRIX_WIDTH; x++) {
            if (mask[y][x]) {
                // 根据到闪光线的距离计算亮度
                float brightness = calculate_flash_brightness(y, x, flash_position);
                
                if (brightness > 0.0f) {
                    // 增强原始颜色的亮度
                    float brighten_factor = 1.0f + brightness * 1.5f; // 最大2.5倍亮度
                    
                    // 获取原始调整后的颜色
                    rgb_t adjusted = adjust_brightness_saturation(
                        original_colors[y][x][0], 
                        original_colors[y][x][1], 
                        original_colors[y][x][2]
                    );
                    
                    // 增加亮度
                    uint16_t r = (uint16_t)(adjusted.r * brighten_factor);
                    uint16_t g = (uint16_t)(adjusted.g * brighten_factor);
                    uint16_t b = (uint16_t)(adjusted.b * brighten_factor);
                    
                    // 限制到有效范围
                    r = (r > 255) ? 255 : r;
                    g = (g > 255) ? 255 : g;
                    b = (b > 255) ? 255 : b;
                    
                    // 设置像素
                    led_matrix_set_pixel(x, y, (uint8_t)r, (uint8_t)g, (uint8_t)b);
                }
            }
        }
    }
    
    // 刷新矩阵显示
    led_matrix_refresh();
}

// 暂停/继续动画
void led_animation_set_running(bool running) {
    animation_running = running;
}

// 检查动画是否运行中
bool led_animation_is_running(void) {
    return animation_running;
}

// 设置动画速度
void led_animation_set_speed(uint8_t speed) {
    animation_speed = speed;
}

// 获取动画速度
uint8_t led_animation_get_speed(void) {
    return animation_speed;
}
