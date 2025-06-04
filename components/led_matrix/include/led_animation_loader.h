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

/**
 * @brief 从JSON文件加载单个指定动画
 * 
 * @param filename JSON文件路径
 * @param animation_name 要加载的动画名称
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t load_specific_animation_from_json(const char *filename, const char *animation_name);

/**
 * @brief 获取JSON文件中的动画数量
 * 
 * @param filename JSON文件路径
 * @return int 动画数量，-1表示错误
 */
int get_animation_count_from_json(const char *filename);

/**
 * @brief 获取JSON文件中指定索引的动画名称
 * 
 * @param filename JSON文件路径
 * @param animation_index 动画索引
 * @param name_buffer 存储动画名称的缓冲区
 * @param buffer_size 缓冲区大小
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t get_animation_name_from_json(const char *filename, int animation_index, 
                                      char *name_buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // LED_ANIMATION_LOADER_H
