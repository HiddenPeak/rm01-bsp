#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_board.h"
#include "sdkconfig.h"

// 引入用于dhcp服务器的头文件


void app_main(void) {
    bsp_board_init();
    printf("ESP32-S3 BSP Initialized!\n");

    // 测试WS2812
    bsp_ws2812_onboard_test();
    // bsp_ws2812_array_test();

    // 测试LP N100电源切换
    bsp_lpn100_power_toggle();

    while (1) {
        // 重启交换机
        // bsp_rtl8367_init();
        bsp_ws2812_onboard_test();
        // 读取电压
        float main_v = bsp_get_main_voltage();
        float aux_v = bsp_get_aux_12v_voltage();
        printf("Main Voltage: %.2fV, Aux 12V: %.2fV\n", main_v, aux_v);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}