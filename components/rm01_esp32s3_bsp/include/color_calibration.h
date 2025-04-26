#ifndef COLOR_CALIBRATION_H
#define COLOR_CALIBRATION_H

#include <stdint.h>

// 校准白点基准值
typedef struct {
    uint8_t r;
    uint8_t g; 
    uint8_t b;
} white_point_t;

// 全局校准配置
extern const white_point_t CALIB_WHITE;

// 色彩映射模式
typedef enum {
    COLOR_MAP_RAW = 0,
    // 可扩展其他模式
} color_map_mode_t;

// 白点参考值宏定义（与 CALIB_WHITE 保持一致）
#define WHITE_R 42
#define WHITE_G 28
#define WHITE_B 19

// 色彩映射接口
void color_map_calibrate(uint8_t *r, uint8_t *g, uint8_t *b);

// 色彩映射主函数
void map_color(color_map_mode_t mode, uint8_t *r, uint8_t *g, uint8_t *b);

#endif // COLOR_CALIBRATION_H