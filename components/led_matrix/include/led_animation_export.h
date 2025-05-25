#ifndef LED_ANIMATION_EXPORT_H
#define LED_ANIMATION_EXPORT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 导出示例动画为JSON文件
 * 
 * @param filename JSON文件路径
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t export_animation_to_json(const char *filename);

#ifdef __cplusplus
}
#endif

#endif // LED_ANIMATION_EXPORT_H
