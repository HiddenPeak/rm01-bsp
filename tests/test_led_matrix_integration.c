// LED Matrix Logo Display 测试验证程序
// 这个文件用于验证重构后的LED Matrix Logo Display Controller功能

#include <stdio.h>
#include "led_matrix_logo_display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TEST_LED_LOGO";

void test_led_matrix_logo_display(void) {
    ESP_LOGI(TAG, "开始测试LED Matrix Logo Display Controller");
    
    // 1. 测试默认配置获取
    logo_display_config_t default_config = led_matrix_logo_display_get_default_config();
    ESP_LOGI(TAG, "默认配置: mode=%d, interval=%lu ms, brightness=%d", 
             default_config.mode, default_config.switch_interval_ms, default_config.brightness);
    
    // 2. 测试初始化
    esp_err_t ret = led_matrix_logo_display_init(&default_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ 初始化成功");
    } else {
        ESP_LOGE(TAG, "✗ 初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    
    // 3. 测试状态查询
    if (led_matrix_logo_display_is_initialized()) {
        ESP_LOGI(TAG, "✓ 初始化状态检查通过");
    } else {
        ESP_LOGE(TAG, "✗ 初始化状态检查失败");
    }
    
    // 4. 测试启动
    ret = led_matrix_logo_display_start();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ 启动成功");
    } else {
        ESP_LOGE(TAG, "✗ 启动失败: %s", esp_err_to_name(ret));
    }
    
    // 5. 测试运行状态查询
    if (led_matrix_logo_display_is_running()) {
        ESP_LOGI(TAG, "✓ 运行状态检查通过");
    } else {
        ESP_LOGW(TAG, "⚠ 运行状态检查：未运行（可能由于SD卡或JSON文件问题）");
    }
    
    // 6. 测试状态打印
    led_matrix_logo_display_print_status();
    
    // 7. 测试配置更改
    ESP_LOGI(TAG, "测试配置更改...");
    led_matrix_logo_display_set_switch_interval(3000);
    led_matrix_logo_display_set_animation_speed(100);
    led_matrix_logo_display_set_brightness(200);
    
    // 8. 测试模式切换
    ESP_LOGI(TAG, "测试模式切换...");
    led_matrix_logo_display_set_mode(LOGO_DISPLAY_MODE_SINGLE);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    led_matrix_logo_display_set_mode(LOGO_DISPLAY_MODE_SEQUENCE);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 9. 测试暂停和恢复
    ESP_LOGI(TAG, "测试暂停和恢复...");
    led_matrix_logo_display_pause(true);
    vTaskDelay(pdMS_TO_TICKS(1000));
    led_matrix_logo_display_pause(false);
    
    // 10. 获取详细状态
    logo_display_status_t status;
    ret = led_matrix_logo_display_get_status(&status);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "状态详情: 运行=%d, 模式=%d, 当前Logo=%lu/%lu", 
                 status.is_running, status.current_mode, 
                 status.current_logo_index, status.total_logos);
        ESP_LOGI(TAG, "总切换次数: %lu, 当前Logo名称: %s", 
                 status.total_switches, status.current_logo_name);
    }
    
    ESP_LOGI(TAG, "LED Matrix Logo Display Controller测试完成");
}

// 测试任务
void led_matrix_test_task(void *pvParameters) {
    // 等待系统初始化完成
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    ESP_LOGI(TAG, "=== LED Matrix Logo Display 功能测试 ===");
    test_led_matrix_logo_display();
    
    // 持续监控状态
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "=== 状态报告 ===");
        led_matrix_logo_display_print_status();
    }
    
    vTaskDelete(NULL);
}

// 创建测试任务函数
void create_led_matrix_test_task(void) {
    xTaskCreate(led_matrix_test_task, "led_matrix_test", 4096, NULL, 5, NULL);
}
