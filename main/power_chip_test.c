/**
 * @file power_chip_test.c
 * @brief 电源芯片UART通信测试示例
 * 
 * 此文件演示如何使用电源芯片UART通信功能来读取连接在GPIO47上的电源芯片数据
 */

#include "bsp_power.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "POWER_CHIP_TEST";

/**
 * @brief 电源芯片数据测试任务
 * 演示如何读取和显示电源芯片数据
 */
void power_chip_test_task(void *pvParameters) {
    ESP_LOGI(TAG, "电源芯片测试任务启动");
    
    // 等待UART初始化完成
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    while (1) {
        // 方法1: 主动读取电源芯片数据
        bsp_power_chip_data_t chip_data;
        esp_err_t ret = bsp_get_power_chip_data(&chip_data);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "主动读取成功 - 电压: %.2fV, 电流: %.3fA, 功率: %.2fW, 温度: %.1f°C", 
                    chip_data.voltage, chip_data.current, chip_data.power, chip_data.temperature);
        } else if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "主动读取超时 - 未接收到数据");
        } else {
            ESP_LOGE(TAG, "主动读取失败: %s", esp_err_to_name(ret));
        }
        
        // 方法2: 获取缓存的最新数据（由后台任务持续更新）
        const bsp_power_chip_data_t* latest_data = bsp_get_latest_power_chip_data();
        if (latest_data != NULL) {
            ESP_LOGI(TAG, "缓存数据 - 电压: %.2fV, 电流: %.3fA, 功率: %.2fW, 温度: %.1f°C (时间戳: %lu)", 
                    latest_data->voltage, latest_data->current, latest_data->power, latest_data->temperature,
                    latest_data->timestamp);
        } else {
            ESP_LOGW(TAG, "缓存数据无效或已过期");
        }
        
        // 每5秒测试一次
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/**
 * @brief 启动电源芯片测试
 * 调用此函数来启动电源芯片数据读取测试
 */
void start_power_chip_test(void) {
    ESP_LOGI(TAG, "启动电源芯片UART通信测试");
    
    // 创建测试任务
    BaseType_t ret = xTaskCreate(power_chip_test_task, 
                                "power_chip_test", 
                                4096,               // 栈大小
                                NULL,              // 任务参数
                                3,                 // 任务优先级
                                NULL);             // 任务句柄
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建电源芯片测试任务失败");
    } else {
        ESP_LOGI(TAG, "电源芯片测试任务已启动");
    }
}

/**
 * @brief 显示电源系统状态
 * 综合显示主电源、辅助电源和电源芯片的状态信息
 */
void show_power_system_status(void) {
    ESP_LOGI(TAG, "=== 电源系统状态 ===");
    
    // 获取主电源和辅助电源状态
    float main_voltage, aux_voltage;
    esp_err_t ret = bsp_get_power_status(&main_voltage, &aux_voltage);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "主电源电压: %.2fV", main_voltage);
        ESP_LOGI(TAG, "辅助电源电压: %.2fV", aux_voltage);
    } else {
        ESP_LOGE(TAG, "获取电源状态失败");
    }
    
    // 获取电源芯片数据
    const bsp_power_chip_data_t* chip_data = bsp_get_latest_power_chip_data();
    if (chip_data != NULL) {
        ESP_LOGI(TAG, "电源芯片数据:");
        ESP_LOGI(TAG, "  电压: %.2fV", chip_data->voltage);
        ESP_LOGI(TAG, "  电流: %.3fA", chip_data->current);
        ESP_LOGI(TAG, "  功率: %.2fW", chip_data->power);
        ESP_LOGI(TAG, "  温度: %.1f°C", chip_data->temperature);
        ESP_LOGI(TAG, "  数据时间: %lu ms", chip_data->timestamp);
    } else {
        ESP_LOGW(TAG, "电源芯片数据无效");
    }
    
    ESP_LOGI(TAG, "==================");
}
