#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_board.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "led_matrix.h"
#include "led_animation_demo.h"
#include "ping_utils.h"
// 不再需要单独的webserver.h，因为它已包含在bsp_board.h中

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
    led_matrix_init();
    
    // 初始化示例动画
    initialize_animation_demo();
    ESP_LOGI("MAIN", "示例动画初始化完成");
      // 查询网络状态
    ESP_LOGI("MAIN", "查询网络状态:");
    vTaskDelay(5000 / portTICK_PERIOD_MS); // 等待5秒，让网络监控系统有时间收集数据
    bsp_get_network_status();
    
    // 启动Web服务器
    esp_err_t ret = bsp_start_webserver();
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "Web服务器启动失败: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI("MAIN", "Web服务器已启动，请使用浏览器访问 http://10.10.99.97/");
    }
    
    while (1) {
        // 更新矩阵动画
        led_matrix_update_animation();
        vTaskDelay(30 / portTICK_PERIOD_MS); // 约33FPS
    }
}