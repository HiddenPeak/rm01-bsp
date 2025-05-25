#ifndef LED_ANIMATION_LOADER_H
#define LED_ANIMATION_LOADER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 动画文件路径
#define ANIMATION_FILE_PATH "/sdcard/matrix.json"

/**
 * @brief 从JSON文件加载动画
 * 
 * @param filename JSON文件路径
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t load_animation_from_json(const char *filename);

/**
 * @brief 检查动画文件是否存在
 * 
 * @param filename JSON文件路径
 * @return true 文件存在
 * @return false 文件不存在
 */
bool animation_file_exists(const char *filename);

#ifdef __cplusplus
}
#endif

#endif // LED_ANIMATION_LOADER_H
