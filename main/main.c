#include <stdio.h>
#include <inttypes.h>
#include "bsp_board.h"
#include "esp_log.h"

static const char *TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "ESP32-S3 BSP应用启动");
    
    // 初始化BSP（包含Touch WS2812显示系统的自动初始化）
    esp_err_t ret = bsp_board_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "ESP32-S3 BSP初始化完成！Touch WS2812显示系统已自动启动");
    
    // 进入BSP应用主循环
    bsp_board_run_main_loop();
}