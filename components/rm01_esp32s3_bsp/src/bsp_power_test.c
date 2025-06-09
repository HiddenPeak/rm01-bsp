/**
 * @file bsp_power_test.c
 * @brief BSP电源芯片UART协商测试
 * 
 * 此文件演示如何使用电源芯片UART协商功能来读取连接在GPIO47上的电源芯片数据
 * 电源芯片数据作为协商信息，仅在系统启动和电压变化超过阈值时触发读取
 * 
 * 数据有效性逻辑：
 * - 开机时进行首次协商，数据会一直有效
 * - 只有当电压变化超过设定阈值时才会重新协商
 * - 数据不基于时间过期，而是基于电压变化事件更新
 * 
 * 重要说明：
 * - 避免频繁调用bsp_get_power_chip_data()以防止持续读取XSP16数据
 * - 推荐使用bsp_get_latest_power_chip_data()获取缓存的协商数据
 * - 只有在特殊需要时才手动触发bsp_trigger_power_chip_negotiation()
 */

#include "bsp_power_test.h"
#include "bsp_power.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BSP_POWER_TEST";

/**
 * @brief BSP电源芯片数据测试任务
 * 演示基于电压变化触发的电源协商读取方式
 * 注意：避免主动频繁调用bsp_get_power_chip_data()以防止持续读取XSP16数据
 */
static void bsp_power_test_task(void *pvParameters) {
    ESP_LOGI(TAG, "BSP电源芯片测试任务启动 - 基于电压变化触发模式");
    
    // 等待UART初始化完成
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 设置电压变化阈值（可选）
    bsp_set_voltage_change_threshold(3.0f, 3.0f); // 设置为3.0V阈值
    
    // 手动触发一次电源协商测试
    ESP_LOGI(TAG, "执行手动电源协商测试");
    bsp_trigger_power_chip_negotiation();
    
    int status_check_count = 0;
    while (1) {
        status_check_count++;
        
        // 只获取缓存的最新协商数据（由电压变化事件或手动触发更新）
        const bsp_power_chip_data_t* latest_data = bsp_get_latest_power_chip_data();
        if (latest_data != NULL) {
            ESP_LOGI(TAG, "缓存协商数据 - 电压: %.2fV, 电流: %.3fA, 功率: %.2fW (数据年龄: %lu ms)", 
                    latest_data->voltage, latest_data->current, latest_data->power,
                    esp_log_timestamp() - latest_data->timestamp);
        } else {
            ESP_LOGW(TAG, "缓存协商数据无效或尚未进行协商");
            
            // 显示详细的数据状态信息
            bool is_valid;
            uint32_t age_seconds;
            esp_err_t status_ret = bsp_get_power_chip_data_status(&is_valid, &age_seconds);
            if (status_ret == ESP_OK) {
                if (!is_valid) {
                    ESP_LOGI(TAG, "  状态: 数据无效（尚未进行协商）");
                } else {
                    ESP_LOGI(TAG, "  状态: 数据有效，年龄: %lu 秒", age_seconds);
                }
            }
        }
        
        // 每30秒手动触发一次协商测试（仅用于演示，实际应用中不需要）
        // if (status_check_count % 6 == 0) {
        //     ESP_LOGI(TAG, "--- 演示：手动触发电源协商 ---");
        //     bsp_trigger_power_chip_negotiation();
        // }
        
        // 每5秒检查一次状态
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/**
 * @brief 启动BSP电源芯片测试
 * 调用此函数来启动基于电压变化触发的电源芯片协商测试
 */
void bsp_power_test_start(void) {
    ESP_LOGI(TAG, "启动BSP电源芯片协商测试 - 基于电压变化触发模式");
    
    // 创建测试任务
    BaseType_t ret = xTaskCreate(bsp_power_test_task, 
                                "bsp_power_test", 
                                4096,               // 栈大小
                                NULL,              // 任务参数
                                3,                 // 任务优先级
                                NULL);             // 任务句柄
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建BSP电源芯片测试任务失败");
    } else {
        ESP_LOGI(TAG, "BSP电源芯片测试任务已启动");
    }
}

/**
 * @brief 显示BSP电源系统状态
 * 综合显示主电源、辅助电源和电源芯片的状态信息
 */
void bsp_power_system_status_show(void) {
    ESP_LOGI(TAG, "=== BSP电源系统状态 ===");
    
    // 获取主电源和辅助电源状态
    float main_voltage, aux_voltage;
    esp_err_t ret = bsp_get_power_status(&main_voltage, &aux_voltage);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "主电源电压: %.2fV", main_voltage);
        ESP_LOGI(TAG, "辅助电源电压: %.2fV", aux_voltage);
    } else {
        ESP_LOGE(TAG, "获取电源状态失败");
    }
    
    // 获取电源芯片协商数据
    const bsp_power_chip_data_t* chip_data = bsp_get_latest_power_chip_data();
    if (chip_data != NULL) {
        uint32_t data_age_ms = esp_log_timestamp() - chip_data->timestamp;
        ESP_LOGI(TAG, "电源芯片协商数据:");
        ESP_LOGI(TAG, "  电压: %.2fV", chip_data->voltage);
        ESP_LOGI(TAG, "  电流: %.3fA", chip_data->current);
        ESP_LOGI(TAG, "  功率: %.2fW", chip_data->power);
        ESP_LOGI(TAG, "  协商时刻: 系统运行第%lu毫秒", chip_data->timestamp);
        ESP_LOGI(TAG, "  数据年龄: %lu毫秒前", data_age_ms);
    } else {
        ESP_LOGW(TAG, "电源芯片协商数据无效");
        
        // 显示详细的数据状态信息
        bool is_valid;
        uint32_t age_seconds;
        esp_err_t status_ret = bsp_get_power_chip_data_status(&is_valid, &age_seconds);
        if (status_ret == ESP_OK) {
            if (!is_valid) {
                ESP_LOGI(TAG, "  状态: 数据无效（尚未进行协商）");
            } else {
                ESP_LOGI(TAG, "  状态: 数据有效，年龄: %lu 秒", age_seconds);
            }
        }
    }
    
    ESP_LOGI(TAG, "======================");
}
