#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_board.h"
#include "esp_log.h"
#include "sdkconfig.h"

// 声明 matrix_init 和 update_matrix
extern void matrix_init();
extern void update_matrix();

void app_main(void) {
    ESP_LOGI("MAIN", "应用程序启动");
    bsp_board_init();
    ESP_LOGI("MAIN", "ESP32-S3 BSP Initialized!");
    
    // LP N100电源切换
    bsp_lpn100_power_toggle();
    ESP_LOGI("MAIN", "LP N100电源切换完成");

    // 板载WS2812测试
    bsp_ws2812_onboard_test();
    ESP_LOGI("MAIN", "板载WS2812测试完成");
    // bsp_ws2812_array_test();
    // ESP_LOGI("MAIN", "WS2812阵列测试完成");
    // bsp_ws2812_touch_test();
    // ESP_LOGI("MAIN", "触摸WS2812测试完成");
    
    // 重启交换机
    // bsp_rtl8367_init();

    // 读取电压
    float main_v = bsp_get_main_voltage();
    float aux_v = bsp_get_aux_12v_voltage();
    printf("Main Voltage: %.2fV, Aux 12V: %.2fV\n", main_v, aux_v);
    // 初始化LED矩阵
    matrix_init();
    // 重启交换机
    // bsp_rtl8367_init();
    while (1) {
        // 更新矩阵
        update_matrix();
        vTaskDelay(30 / portTICK_PERIOD_MS); // 约33FPS
    }
}