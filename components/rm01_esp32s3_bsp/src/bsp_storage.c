#include "bsp_storage.h"
#include "bsp_board.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

static const char *TAG = "BSP_STORAGE";

// SDMMC 相关静态变量
static sdmmc_card_t *s_card = NULL;
static bool s_sdmmc_mounted = false;

esp_err_t bsp_storage_sdcard_mount(const char *mount_point)
{
    if (s_sdmmc_mounted) {
        ESP_LOGI(TAG, "SD卡已挂载");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "挂载FAT文件系统到 %s", mount_point);

    // 挂载配置
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,  // 如果挂载失败，尝试格式化
        .max_files = 5,                  // 最大打开文件数
        .allocation_unit_size = 16 * 1024 // 簇大小
    };

    // 主机配置
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_1;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    // 插槽配置
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = BSP_TF_CK_PIN;
    slot_config.cmd = BSP_TF_CMD_PIN;
    slot_config.d0 = BSP_TF_D0_PIN;
    slot_config.d1 = BSP_TF_D1_PIN;
    slot_config.d2 = BSP_TF_D2_PIN;
    slot_config.d3 = BSP_TF_D3_PIN;
    slot_config.width = 4; // 4-bit模式
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP; // 使用内部上拉

    // 挂载文件系统
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "挂载文件系统失败，可能SD卡未分区或未格式化");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "未检测到SD卡");
        } else {
            ESP_LOGE(TAG, "挂载文件系统失败: %s", esp_err_to_name(ret));
        }
        return ret;
    }
    
    s_sdmmc_mounted = true;
    ESP_LOGI(TAG, "文件系统挂载成功");
    
    // 打印SD卡信息
    ESP_LOGI(TAG, "SD卡信息:");
    ESP_LOGI(TAG, "卡名称: %s", s_card->cid.name);
    ESP_LOGI(TAG, "卡类型: %s", (s_card->ocr & (1 << 30)) ? "SDHC/SDXC" : "SDSC");
    ESP_LOGI(TAG, "卡速度: %s", (s_card->csd.tr_speed > 25000000) ? "高速" : "标准速度");
    ESP_LOGI(TAG, "卡容量: %lluMB", ((uint64_t) s_card->csd.capacity) * s_card->csd.sector_size / (1024 * 1024));

    return ESP_OK;
}

esp_err_t bsp_storage_sdcard_unmount(const char *mount_point)
{
    if (!s_sdmmc_mounted) {
        return ESP_OK;
    }
    
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(mount_point, s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "卸载文件系统失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    s_sdmmc_mounted = false;
    s_card = NULL;
    ESP_LOGI(TAG, "文件系统已卸载");
    return ESP_OK;
}

sdmmc_card_t* bsp_storage_sdcard_get_info(void)
{
    return s_card;
}

bool bsp_storage_sdcard_is_mounted(void)
{
    return s_sdmmc_mounted;
}

esp_err_t bsp_storage_create_dir_if_not_exists(const char *dir_path)
{
    struct stat st;
    
    // 检查目录是否存在
    if (stat(dir_path, &st) != 0) {
        // 目录不存在，创建它
        ESP_LOGI(TAG, "创建目录: %s", dir_path);
        if (mkdir(dir_path, 0755) != 0) {
            ESP_LOGE(TAG, "创建目录失败: %s (errno: %d)", dir_path, errno);
            return ESP_FAIL;
        } else {
            ESP_LOGI(TAG, "成功创建目录");
            return ESP_OK;
        }
    }
    
    // 目录已存在
    return ESP_OK;
}

void bsp_storage_list_dir(const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (dir) {
        struct dirent *entry;
        int file_count = 0;
        ESP_LOGI(TAG, "目录 %s 内容:", dir_path);
        
        while ((entry = readdir(dir)) != NULL) {
            ESP_LOGI(TAG, "  %s", entry->d_name);
            file_count++;
        }
        
        if (file_count == 0) {
            ESP_LOGI(TAG, "  (空目录)");
        }
        
        closedir(dir);
    } else {
        ESP_LOGE(TAG, "无法打开目录: %s", dir_path);
    }
}
