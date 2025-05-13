#include "led_color.h"
#include <math.h>

// 色彩校准的白点参考值（与头文件中的宏定义一致）
const white_point_t COLOR_CALIB_WHITE = {
    .r = WHITE_R,  // 42
    .g = WHITE_G,  // 28
    .b = WHITE_B   // 19
};

// 颜色校正函数
rgb_t color_correct(uint8_t input_r, uint8_t input_g, uint8_t input_b) {
    rgb_t result;
    const rgb_t black = {0, 0, 0};
    const rgb_t min_white = {5, 4, 3};
    const rgb_t max_white = {168, 112, 76};
    const float input_min = 5.0f;
    const float input_max = 255.0f;

    const float r_slope = (float)(max_white.r - min_white.r) / (input_max - input_min);
    const float g_slope = (float)(max_white.g - min_white.g) / (input_max - input_min);
    const float b_slope = (float)(max_white.b - min_white.b) / (input_max - input_min);

    const float r_intercept = min_white.r - r_slope * input_min;
    const float g_intercept = min_white.g - g_slope * input_min;
    const float b_intercept = min_white.b - b_slope * input_min;

    float temp_r, temp_g, temp_b;
    if (input_r <= 5 && input_g <= 5 && input_b <= 5) {
        temp_r = (float)input_r * (min_white.r / input_min);
        temp_g = (float)input_g * (min_white.g / input_min);
        temp_b = (float)input_b * (min_white.b / input_min);
    } else {
        temp_r = (float)input_r * r_slope + r_intercept;
        temp_g = (float)input_g * g_slope + g_intercept;
        temp_b = (float)input_b * b_slope + b_intercept;
    }

    temp_r = (temp_r < black.r) ? black.r : (temp_r > max_white.r) ? max_white.r : temp_r;
    temp_g = (temp_g < black.g) ? black.g : (temp_g > max_white.g) ? max_white.g : temp_g;
    temp_b = (temp_b < black.b) ? black.b : (temp_b > max_white.b) ? max_white.b : temp_b;

    result.r = (uint8_t)(temp_r + 0.5f);
    result.g = (uint8_t)(temp_g + 0.5f);
    result.b = (uint8_t)(temp_b + 0.5f);
    
    return result;
}

// RGB 转 HSL 转换
hsl_t rgb_to_hsl(uint8_t r, uint8_t g, uint8_t b) {
    hsl_t hsl;
    float r_norm = r / 255.0f;
    float g_norm = g / 255.0f;
    float b_norm = b / 255.0f;

    float max = fmaxf(fmaxf(r_norm, g_norm), b_norm);
    float min = fminf(fminf(r_norm, g_norm), b_norm);
    float delta = max - min;

    // 亮度
    hsl.l = (max + min) / 2.0f;

    // 饱和度
    if (delta == 0.0f) {
        hsl.s = 0.0f;
        hsl.h = 0.0f; // 未定义，但设为0
    } else {
        hsl.s = (hsl.l > 0.5f) ? (delta / (2.0f - max - min)) : (delta / (max + min));

        // 色相
        if (max == r_norm) {
            hsl.h = (g_norm - b_norm) / delta + (g_norm < b_norm ? 6.0f : 0.0f);
        } else if (max == g_norm) {
            hsl.h = (b_norm - r_norm) / delta + 2.0f;
        } else {
            hsl.h = (r_norm - g_norm) / delta + 4.0f;
        }
        hsl.h *= 60.0f;
    }

    return hsl;
}

// HSL 转 RGB 转换
rgb_t hsl_to_rgb(float h, float s, float l) {
    rgb_t rgb;
    float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = l - c / 2.0f;

    float r, g, b;
    if (h >= 0.0f && h < 60.0f) {
        r = c; g = x; b = 0.0f;
    } else if (h >= 60.0f && h < 120.0f) {
        r = x; g = c; b = 0.0f;
    } else if (h >= 120.0f && h < 180.0f) {
        r = 0.0f; g = c; b = x;
    } else if (h >= 180.0f && h < 240.0f) {
        r = 0.0f; g = x; b = c;
    } else if (h >= 240.0f && h < 300.0f) {
        r = x; g = 0.0f; b = c;
    } else {
        r = c; g = 0.0f; b = x;
    }

    rgb.r = (uint8_t)((r + m) * 255.0f + 0.5f);
    rgb.g = (uint8_t)((g + m) * 255.0f + 0.5f);
    rgb.b = (uint8_t)((b + m) * 255.0f + 0.5f);

    return rgb;
}

// 调整亮度和饱和度
rgb_t adjust_brightness_saturation(uint8_t r, uint8_t g, uint8_t b) {
    // 步骤1：降低亮度 52.4% (0.595 * 0.8)
    float brightness_factor = 0.476f; // 总亮度降低: 1 - 0.524
    float adjusted_r = r * brightness_factor;
    float adjusted_g = g * brightness_factor;
    float adjusted_b = b * brightness_factor;

    // 限制到有效范围
    adjusted_r = (adjusted_r < 0) ? 0 : (adjusted_r > 255) ? 255 : adjusted_r;
    adjusted_g = (adjusted_g < 0) ? 0 : (adjusted_g > 255) ? 255 : adjusted_g;
    adjusted_b = (adjusted_b < 0) ? 0 : (adjusted_b > 255) ? 255 : adjusted_b;

    // 步骤2：转换到HSL并增加饱和度 52.0875% (1.3225 * 1.15)
    hsl_t hsl = rgb_to_hsl((uint8_t)adjusted_r, (uint8_t)adjusted_g, (uint8_t)adjusted_b);
    hsl.s *= 1.520875f; // 总饱和度增加: 1 + 0.520875
    hsl.s = (hsl.s > 1.0f) ? 1.0f : hsl.s; // 限制饱和度为1.0

    // 步骤3：转换回RGB
    return hsl_to_rgb(hsl.h, hsl.s, hsl.l);
}

// 辅助函数：伽马调整
static uint8_t gamma_adjust(uint8_t value, float ratio) {
    float normalized = powf(value / 255.0f, 1.0f / ratio);
    return (uint8_t)(normalized * 255);
}

// 辅助函数：伽马校正
static uint8_t gamma_correction(uint8_t v, float gamma) {
    return (uint8_t)(powf(v / 255.0f, gamma) * 255);
}

// 色彩映射校准函数
void color_map_calibrate(uint8_t *r, uint8_t *g, uint8_t *b) {
    // 计算各通道压缩比
    const float ratio_r = COLOR_CALIB_WHITE.r / 255.0f;
    const float ratio_g = COLOR_CALIB_WHITE.g / 255.0f;
    const float ratio_b = COLOR_CALIB_WHITE.b / 255.0f;
    
    // 应用非线性映射
    *r = gamma_adjust(*r, ratio_r);
    *g = gamma_adjust(*g, ratio_g);
    *b = gamma_adjust(*b, ratio_b);
    
    // 硬限幅保护
    *r = (*r > COLOR_CALIB_WHITE.r) ? COLOR_CALIB_WHITE.r : *r;
    *g = (*g > COLOR_CALIB_WHITE.g) ? COLOR_CALIB_WHITE.g : *g;
    *b = (*b > COLOR_CALIB_WHITE.b) ? COLOR_CALIB_WHITE.b : *b;
}

// 色彩映射函数
void map_color(color_map_mode_t mode, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (mode == COLOR_MAP_RAW) {
        return; // 原始模式不进行调整
    }
    
    if (mode == COLOR_MAP_CALIBRATED) {
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
}
