#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_board.h"
#include "bsp_ws2812.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "led_matrix.h"
#include "ping_utils.h"
#include "power_chip_test.h"

void app_main(void) {
    ESP_LOGI("MAIN", "应用程序启动");
    
    // BSP板级初始化（包含硬件初始化、电源管理、网络控制器、LED矩阵、Web服务器、网络监控等）
    bsp_board_init();
    ESP_LOGI("MAIN", "ESP32-S3 BSP初始化完成！");
    
    // 启动电源芯片UART通信测试
    start_power_chip_test();
    
    // 等待一段时间后显示电源系统状态
    vTaskDelay(pdMS_TO_TICKS(3000));
    show_power_system_status();
    
    // 应用主循环
    ESP_LOGI("MAIN", "进入应用主循环");
    int loop_count = 0;
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS); // 每秒休眠，保持系统运行
        
        // 每30秒显示一次电源系统状态
        loop_count++;
        if (loop_count >= 30) {
            show_power_system_status();
            loop_count = 0;
        }
    }
}