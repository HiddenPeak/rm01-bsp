#include "color_calibration.h"
#include <math.h>

// 校准基准值定义（与头文件严格对应）
const white_point_t CALIB_WHITE = {
    .r = 42,  // 您的理想值
    .g = 28,
    .b = 19
};

static uint8_t gamma_adjust(uint8_t value, float ratio) {
    float normalized = powf(value / 255.0f, 1.0f / ratio);
    return (uint8_t)(normalized * 255);
}

void color_map_calibrate(uint8_t *r, uint8_t *g, uint8_t *b) {
    // 计算各通道压缩比
    const float ratio_r = CALIB_WHITE.r / 255.0f;
    const float ratio_g = CALIB_WHITE.g / 255.0f;
    const float ratio_b = CALIB_WHITE.b / 255.0f;
    
    // 应用非线性映射
    *r = gamma_adjust(*r, ratio_r);
    *g = gamma_adjust(*g, ratio_g);
    *b = gamma_adjust(*b, ratio_b);
    
    // 硬限幅保护
    *r = (*r > CALIB_WHITE.r) ? CALIB_WHITE.r : *r;
    *g = (*g > CALIB_WHITE.g) ? CALIB_WHITE.g : *g;
    *b = (*b > CALIB_WHITE.b) ? CALIB_WHITE.b : *b;
}

// 非线性补偿曲线（基于您的柔光材料特性）
static uint8_t gamma_correction(uint8_t v, float gamma) {
    return (uint8_t)(powf(v / 255.0f, gamma) * 255);
}

void map_color(color_map_mode_t mode, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (mode == COLOR_MAP_RAW) return;
    
    // 第一步：归一化到白点比例
    float scale_r = WHITE_R / 255.0f;
    float scale_g = WHITE_G / 255.0f; 
    float scale_b = WHITE_B / 255.0f;
    
    // 第二步：应用非线性补偿
    *r = gamma_correction(*r, 1.0f / scale_r);
    *g = gamma_correction(*g, 1.0f / scale_g);
    *b = gamma_correction(*b, 1.0f / scale_b);
    
    // 第三步：限制输出范围
    *r = (*r > WHITE_R) ? WHITE_R : *r;
    *g = (*g > WHITE_G) ? WHITE_G : *g;
    *b = (*b > WHITE_B) ? WHITE_B : *b;
}