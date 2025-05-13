#ifndef LED_COLOR_H
#define LED_COLOR_H

#include <stdint.h>

// RGB结构体
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

// HSL结构体
typedef struct {
    float h; // 色相 (0-360)
    float s; // 饱和度 (0-1)
    float l; // 亮度 (0-1)
} hsl_t;

// 白点参考值结构体
typedef struct {
    uint8_t r;
    uint8_t g; 
    uint8_t b;
} white_point_t;

// 色彩映射模式
typedef enum {
    COLOR_MAP_RAW = 0,     // 原始颜色，不进行调整
    COLOR_MAP_CALIBRATED,  // 校准后的颜色
    // 可扩展其他模式
} color_map_mode_t;

// 色彩校准的白点参考值
extern const white_point_t COLOR_CALIB_WHITE;

// 白点参考值宏定义
#define WHITE_R 42
#define WHITE_G 28
#define WHITE_B 19

// 颜色校准函数
rgb_t color_correct(uint8_t r, uint8_t g, uint8_t b);

// 亮度和饱和度调整
rgb_t adjust_brightness_saturation(uint8_t r, uint8_t g, uint8_t b);

// RGB转HSL
hsl_t rgb_to_hsl(uint8_t r, uint8_t g, uint8_t b);

// HSL转RGB
rgb_t hsl_to_rgb(float h, float s, float l);

// 色彩映射校准函数
void color_map_calibrate(uint8_t *r, uint8_t *g, uint8_t *b);

// 色彩映射函数
void map_color(color_map_mode_t mode, uint8_t *r, uint8_t *g, uint8_t *b);

#endif // LED_COLOR_H
