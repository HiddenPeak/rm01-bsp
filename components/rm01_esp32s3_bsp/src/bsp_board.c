#include "bsp_board.h"
#include "bsp_network.h"
#include "network_monitor.h"
#include "bsp_ws2812.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "color_calibration.h"
#include <math.h>
#include <inttypes.h> // 添加此头文件以支持 PRIuXX 宏
#include "sdmmc_cmd.h" // 用于SD卡命令和信息打印
#include "bsp_power.h" // 引入电源管理头文件

static const char *TAG = "BSP";

void bsp_w5500_init(spi_host_device_t host) {
    esp_err_t ret = bsp_w5500_network_init(host);
    if (ret == ESP_OK) {
        // 初始化并启动网络监控系统
        nm_init();
        nm_start_monitoring();
    }
}

void bsp_rtl8367_init(void) {
    esp_err_t ret = bsp_rtl8367_network_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RTL8367网络初始化失败: %s", esp_err_to_name(ret));
    }
}

void bsp_board_init(void) {
    bsp_ws2812_init(BSP_WS2812_ONBOARD);  // 初始化板载WS2812
    // bsp_ws2812_init(BSP_WS2812_ARRAY); // 与 matrix_init();二选一
    bsp_power_init(); // 初始化电源管理模块
    bsp_w5500_init(SPI3_HOST);
    // bsp_rtl8367_init();
}