#ifndef LED_ANIMATION_EXPORT_H
#define LED_ANIMATION_EXPORT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 导出所有动画为JSON文件
 * 
 * 该函数将所有内置的示例动画（示例动画、启动中、链接错误）
 * 导出为JSON格式文件，与example_animation.json保持同步
 * 
 * @param filename JSON文件路径
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t export_animation_to_json(const char *filename);

/**
 * @brief 获取内置动画数量
 * 
 * @return int 内置动画的数量
 */
int get_builtin_animation_count(void);

/**
 * @brief 获取指定动画的名称
 * 
 * @param index 动画索引（0开始）
 * @return const char* 动画名称，如果索引无效返回NULL
 */
const char* get_builtin_animation_name(int index);

/**
 * @brief 获取指定动画的点数量
 * 
 * @param index 动画索引（0开始）
 * @return int 点数量，如果索引无效返回-1
 */
int get_builtin_animation_point_count(int index);

#ifdef __cplusplus
}
#endif

#endif // LED_ANIMATION_EXPORT_H
