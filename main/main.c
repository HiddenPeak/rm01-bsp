#include <stdio.h>
#include "bsp_board.h"

void app_main(void) {
    ESP_LOGI("MAIN", "应用程序启动");
    
    // BSP板级初始化（包含所有硬件初始化、服务启动和系统配置）
    esp_err_t ret = bsp_board_init();
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "BSP初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI("MAIN", "ESP32-S3 BSP初始化完成！");
    
    // 进入BSP应用主循环
    bsp_board_run_main_loop();
}