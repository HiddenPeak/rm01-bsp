#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_board.h"
#include "bsp_ws2812.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "led_matrix.h"
#include "bsp_power_test.h"
#include "bsp_network_animation.h"
#include "bsp_system_state.h"

void app_main(void) {
    ESP_LOGI("MAIN", "应用程序启动");
    
    // BSP板级初始化（包含硬件初始化、电源管理、网络控制器、LED矩阵、Web服务器、网络监控等）
    bsp_board_init();
    ESP_LOGI("MAIN", "ESP32-S3 BSP初始化完成！");
    
    // 初始化系统状态控制器（新的扩展状态分类系统）
    ESP_LOGI("MAIN", "初始化系统状态控制器");
    esp_err_t ret = bsp_system_state_init();
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "系统状态控制器初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    
    // 保持向后兼容的网络动画控制器初始化
    ESP_LOGI("MAIN", "初始化网络动画控制器（兼容模式）");
    bsp_network_animation_init();
    
    // 设置启动动画，等待网络监控系统启动
    bsp_network_animation_set_startup();
    ESP_LOGI("MAIN", "已设置启动动画，等待网络监控系统启动");
      // 启动BSP电源芯片UART通信测试
    bsp_power_test_start();
    
    // 等待网络监控系统收集初始数据
    ESP_LOGI("MAIN", "等待网络监控系统收集初始数据...");
    vTaskDelay(pdMS_TO_TICKS(8000)); // 等待8秒让网络监控系统完成初始化
      // 开始网络状态与动画联动监控
    ESP_LOGI("MAIN", "开始网络状态与动画联动监控");
    bsp_network_animation_start_monitoring();
    
    // 启动系统状态监控
    ESP_LOGI("MAIN", "启动系统状态监控");
    bsp_system_state_start_monitoring();
      // 显示BSP电源系统状态
    bsp_power_system_status_show();
      // 应用主循环
    ESP_LOGI("MAIN", "进入应用主循环");
    int loop_count = 0;
    int status_report_count = 0;
    int system_state_report_count = 0;
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS); // 每秒休眠，保持系统运行
        
        loop_count++;
        status_report_count++;
        system_state_report_count++;
        
        // 每10秒更新一次系统状态
        if (system_state_report_count >= 10) {
            bsp_system_state_update_and_report();
            system_state_report_count = 0;
        }
          // 每30秒显示一次BSP电源系统状态
        if (loop_count >= 30) {
            bsp_power_system_status_show();
            loop_count = 0;
        }
        
        // 每60秒显示一次网络动画控制器状态
        if (status_report_count >= 60) {
            bsp_network_animation_print_status();
            status_report_count = 0;
        }
    }
}