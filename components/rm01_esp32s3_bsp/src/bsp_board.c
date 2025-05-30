#include "bsp_board.h"
#include "bsp_network.h"
#include "network_monitor.h"
#include "bsp_ws2812.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
// #include "color_calibration.h"
#include <math.h>
#include <inttypes.h> // 添加此头文件以支持 PRIuXX 宏
#include "sdmmc_cmd.h" // 用于SD卡命令和信息打印
#include "bsp_power.h" // 引入电源管理头文件
#include "led_matrix.h" // 引入LED矩阵头文件
#include "bsp_webserver.h" // 引入Web服务器头文件

static const char *TAG = "BSP";

// 动画更新任务句柄
static TaskHandle_t animation_task_handle = NULL;

// 动画更新任务
static void animation_update_task(void *pvParameters) {
    ESP_LOGI(TAG, "LED矩阵动画更新任务启动");
    
    while (1) {
        led_matrix_update_animation();
        vTaskDelay(30 / portTICK_PERIOD_MS); // 10FPS，降低频率减少RMT压力
    }
}

// BSP LED矩阵服务初始化
void bsp_init_led_matrix_service(void) {
    ESP_LOGI(TAG, "初始化LED矩阵服务");
    led_matrix_init();
    ESP_LOGI(TAG, "LED矩阵系统初始化完成");
}

// BSP网络监控服务初始化
void bsp_init_network_monitoring_service(void) {
    ESP_LOGI(TAG, "启动网络监控服务");
    
    // 等待网络监控系统收集数据
    ESP_LOGI(TAG, "等待网络监控系统启动...");
    vTaskDelay(5000 / portTICK_PERIOD_MS); // 等待5秒，与main函数中的延迟一致
    
    ESP_LOGI(TAG, "查询网络状态:");
    nm_get_network_status();
    
    ESP_LOGI(TAG, "网络监控服务启动完成");
}

// BSP Web服务器服务初始化
void bsp_init_webserver_service(void) {
    ESP_LOGI(TAG, "启动Web服务器服务");
    esp_err_t ret = bsp_start_webserver();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Web服务器启动失败: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Web服务器已启动，请使用浏览器访问 http://10.10.99.97/");
    }
}

// 启动动画更新任务
void bsp_start_animation_task(void) {
    if (animation_task_handle == NULL) {
        BaseType_t ret = xTaskCreate(
            animation_update_task,
            "animation_task",
            4096,
            NULL,
            5, // 中等优先级
            &animation_task_handle
        );
        
        if (ret == pdPASS) {
            ESP_LOGI(TAG, "LED矩阵自动动画更新任务已启动");
        } else {
            ESP_LOGE(TAG, "LED矩阵动画更新任务创建失败");
        }
    }
}

// 停止动画更新任务
void bsp_stop_animation_task(void) {
    if (animation_task_handle != NULL) {
        vTaskDelete(animation_task_handle);
        animation_task_handle = NULL;
        ESP_LOGI(TAG, "LED矩阵动画更新任务已停止");
    }
}

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
    ESP_LOGI(TAG, "开始初始化ESP32-S3 BSP");
    
    // ============ 硬件层初始化 ============
    ESP_LOGI(TAG, "初始化硬件层");
    
    // 初始化电源管理模块
    bsp_power_init();
      // 初始化板载WS2812
    bsp_ws2812_init(BSP_WS2812_ONBOARD);
    
    // 初始化触摸WS2812
    bsp_ws2812_init(BSP_WS2812_TOUCH);
    
    // 注意：GPIO 9 的LED矩阵由led_matrix模块独立管理
    
    // 初始化网络控制器
    bsp_w5500_init(SPI3_HOST);
    // bsp_rtl8367_init(); // 如需要可启用
    
    // LPN100电源控制操作
    bsp_lpn100_power_toggle();
    ESP_LOGI(TAG, "LPN100电源控制完成");

    bsp_orin_power_control(false); // 开启ORIN电源
    
    // 读取并报告电压状态
    float main_v = bsp_get_main_voltage();
    float aux_v = bsp_get_aux_12v_voltage();
    ESP_LOGI(TAG, "电源状态 - 主电源: %.2fV, 辅助12V: %.2fV", main_v, aux_v);

    // ============ BSP服务层初始化 ============
    ESP_LOGI(TAG, "初始化BSP服务层");
    
    // 先初始化网络监控服务
    bsp_init_network_monitoring_service();
    
    // 初始化Web服务器服务
    bsp_init_webserver_service();
    
    // 延迟初始化LED矩阵服务，确保其他RMT设备完成初始化
    ESP_LOGI(TAG, "等待其他模块稳定后初始化LED矩阵...");
    vTaskDelay(1000 / portTICK_PERIOD_MS); // 等待1秒
    bsp_init_led_matrix_service();
    
    // 重新启用动画任务 - RMT问题已解决
    bsp_start_animation_task();
    ESP_LOGI(TAG, "LED矩阵初始化完成，动画任务已启动");

    // 板载WS2812测试 - 可选
    // bsp_ws2812_onboard_test();
    // ESP_LOGI("MAIN", "板载WS2812测试完成");
    bsp_ws2812_touch_test();
    ESP_LOGI("MAIN", "触摸WS2812测试完成");
    
    ESP_LOGI(TAG, "ESP32-S3 BSP及所有服务初始化完成");
}