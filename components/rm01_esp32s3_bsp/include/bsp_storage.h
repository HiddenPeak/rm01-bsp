#pragma once

#include "esp_err.h"
#include "sdmmc_cmd.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// TF卡挂载点
#define MOUNT_POINT "/sdcard"
// Web文件目录
#define WEB_FOLDER "/sdcard/web"

/**
 * @brief 初始化并挂载SD卡
 * 
 * @param mount_point 挂载点路径
 * @return esp_err_t ESP_OK 成功，其他值表示失败
 */
esp_err_t bsp_storage_sdcard_mount(const char *mount_point);

/**
 * @brief 卸载SD卡
 * 
 * @param mount_point 挂载点路径
 * @return esp_err_t ESP_OK 成功，其他值表示失败
 */
esp_err_t bsp_storage_sdcard_unmount(const char *mount_point);

/**
 * @brief 获取SD卡信息
 * 
 * @return sdmmc_card_t* SD卡信息指针，NULL表示未挂载
 */
sdmmc_card_t* bsp_storage_sdcard_get_info(void);

/**
 * @brief 检查SD卡是否已挂载
 * 
 * @return true 已挂载
 * @return false 未挂载
 */
bool bsp_storage_sdcard_is_mounted(void);

/**
 * @brief 创建目录(如不存在)
 * 
 * @param dir_path 目录路径
 * @return esp_err_t ESP_OK 成功，其他值表示失败
 */
esp_err_t bsp_storage_create_dir_if_not_exists(const char *dir_path);

/**
 * @brief 列出目录内容
 * 
 * @param dir_path 目录路径
 */
void bsp_storage_list_dir(const char *dir_path);

#ifdef __cplusplus
}
#endif
