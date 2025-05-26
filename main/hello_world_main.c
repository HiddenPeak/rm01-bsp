#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_board.h"
#include "bsp_ws2812.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "led_matrix.h"
#include "ping_utils.h"

void app_main(void) {
    ESP_LOGI("MAIN", "应用程序启动");
    
    // BSP板级初始化（包含硬件初始化、电源管理、网络控制器、LED矩阵、Web服务器、网络监控等）
    bsp_board_init();
    ESP_LOGI("MAIN", "ESP32-S3 BSP初始化完成！");
    
    // 应用主循环
    ESP_LOGI("MAIN", "进入应用主循环");
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS); // 每秒休眠，保持系统运行
    }
}