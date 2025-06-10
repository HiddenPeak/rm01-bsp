/**
 * @file test_board_ws2812_display.c
 * @brief Board WS2812 显示功能测试程序
 */

#include "bsp_board_ws2812_display.h"
#include "bsp_display_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BOARD_WS2812_TEST";

/**
 * @brief 测试Board WS2812基本颜色显示
 */
static void test_basic_colors(void) {
    ESP_LOGI(TAG, "========== 测试基本颜色显示 ==========");
    
    // 测试各种基本颜色
    ESP_LOGI(TAG, "显示红色");
    bsp_board_ws2812_display_set_color(255, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "显示紫色");
    bsp_board_ws2812_display_set_color(128, 0, 128);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "显示蓝色");
    bsp_board_ws2812_display_set_color(0, 0, 255);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "显示绿色");
    bsp_board_ws2812_display_set_color(0, 255, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "显示白色");
    bsp_board_ws2812_display_set_color(255, 255, 255);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "关闭LED");
    bsp_board_ws2812_display_off();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief 测试Board WS2812呼吸灯效果
 */
static void test_breath_effects(void) {
    ESP_LOGI(TAG, "========== 测试呼吸灯效果 ==========");
    
    ESP_LOGI(TAG, "红色快速呼吸（3秒）");
    bsp_board_ws2812_display_set_breath(255, 0, 0, BOARD_BREATH_SPEED_FAST);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "紫色正常呼吸（3秒）");
    bsp_board_ws2812_display_set_breath(128, 0, 128, BOARD_BREATH_SPEED_NORMAL);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "红色慢速呼吸（3秒）");
    bsp_board_ws2812_display_set_breath(255, 0, 0, BOARD_BREATH_SPEED_SLOW);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "紫色慢速呼吸（3秒）");
    bsp_board_ws2812_display_set_breath(128, 0, 128, BOARD_BREATH_SPEED_SLOW);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "关闭LED");
    bsp_board_ws2812_display_off();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief 测试Board WS2812预定义显示模式
 */
static void test_predefined_modes(void) {
    ESP_LOGI(TAG, "========== 测试预定义显示模式 ==========");
    
    ESP_LOGI(TAG, "测试高温警告模式（红色慢速呼吸）");
    bsp_board_ws2812_display_set_mode(BOARD_DISPLAY_MODE_HIGH_TEMP);
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    ESP_LOGI(TAG, "测试GPU高使用率模式（紫色快速呼吸）");
    bsp_board_ws2812_display_set_mode(BOARD_DISPLAY_MODE_HIGH_POWER);
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    ESP_LOGI(TAG, "测试内存高使用率模式（紫色慢速呼吸）");
    bsp_board_ws2812_display_set_mode(BOARD_DISPLAY_MODE_MEMORY_HIGH_USAGE);
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    ESP_LOGI(TAG, "测试关闭模式");
    bsp_board_ws2812_display_set_mode(BOARD_DISPLAY_MODE_OFF);
    vTaskDelay(pdMS_TO_TICKS(2000));
}

/**
 * @brief 测试亮度调节功能
 */
static void test_brightness_control(void) {
    ESP_LOGI(TAG, "========== 测试亮度调节功能 ==========");
    
    ESP_LOGI(TAG, "设置白色，测试不同亮度级别");
    
    uint8_t brightness_levels[] = {255, 200, 150, 100, 50, 25, 10, 255};
    size_t num_levels = sizeof(brightness_levels) / sizeof(brightness_levels[0]);
    
    for (size_t i = 0; i < num_levels; i++) {
        ESP_LOGI(TAG, "设置亮度为: %d", brightness_levels[i]);
        bsp_board_ws2812_display_set_brightness(brightness_levels[i]);
        bsp_board_ws2812_display_set_color(255, 255, 255);
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
    
    ESP_LOGI(TAG, "关闭LED");
    bsp_board_ws2812_display_off();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief 获取并显示状态信息
 */
static void print_status_info(void) {
    ESP_LOGI(TAG, "========== 获取显示状态信息 ==========");
    
    board_display_status_t status;
    esp_err_t ret = bsp_board_ws2812_display_get_status(&status);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Board WS2812显示状态:");
        ESP_LOGI(TAG, "  当前模式: %s", bsp_board_ws2812_display_get_mode_name(status.current_mode));
        ESP_LOGI(TAG, "  前一个模式: %s", bsp_board_ws2812_display_get_mode_name(status.previous_mode));
        ESP_LOGI(TAG, "  激活状态: %s", status.is_active ? "是" : "否");
        ESP_LOGI(TAG, "  模式变化次数: %ld", status.mode_change_count);
        ESP_LOGI(TAG, "  在当前模式时间: %ld ms", status.time_in_current_mode);
        ESP_LOGI(TAG, "  系统运行时间: %ld ms", status.system_uptime_ms);
        
        ESP_LOGI(TAG, "监控数据状态:");
        ESP_LOGI(TAG, "  N305数据有效: %s", status.metrics.n305_data_valid ? "是" : "否");
        ESP_LOGI(TAG, "  Jetson数据有效: %s", status.metrics.jetson_data_valid ? "是" : "否");
        if (status.metrics.n305_data_valid) {
            ESP_LOGI(TAG, "  N305 CPU温度: %.1f°C", status.metrics.n305_cpu_temp);
        }
        if (status.metrics.jetson_data_valid) {
            ESP_LOGI(TAG, "  Jetson CPU温度: %.1f°C", status.metrics.jetson_cpu_temp);
            ESP_LOGI(TAG, "  Jetson GPU温度: %.1f°C", status.metrics.jetson_gpu_temp);
            ESP_LOGI(TAG, "  Jetson功率: %.1f mW (%.2f W)", status.metrics.jetson_power_mw, status.metrics.jetson_power_mw/1000.0f);
            ESP_LOGI(TAG, "  Jetson内存使用率: %.1f%%", status.metrics.jetson_memory_usage);
        }
    } else {
        ESP_LOGE(TAG, "获取状态失败: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief 测试监控数据更新
 */
static void test_metrics_update(void) {
    ESP_LOGI(TAG, "========== 测试监控数据更新 ==========");
    
    ESP_LOGI(TAG, "手动更新监控数据...");
    esp_err_t ret = bsp_board_ws2812_display_update_metrics();
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "监控数据更新成功");
        
        system_metrics_t metrics;
        ret = bsp_board_ws2812_display_get_metrics(&metrics);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "当前监控数据:");
            if (metrics.n305_data_valid) {
                ESP_LOGI(TAG, "  N305 CPU温度: %.1f°C", metrics.n305_cpu_temp);
            } else {
                ESP_LOGI(TAG, "  N305数据无效");
            }
            
            if (metrics.jetson_data_valid) {
                ESP_LOGI(TAG, "  Jetson CPU温度: %.1f°C", metrics.jetson_cpu_temp);
                ESP_LOGI(TAG, "  Jetson GPU温度: %.1f°C", metrics.jetson_gpu_temp);
                ESP_LOGI(TAG, "  Jetson功率: %.1f mW (%.2f W)", metrics.jetson_power_mw, metrics.jetson_power_mw/1000.0f);
                ESP_LOGI(TAG, "  Jetson内存使用情况: %.1f MB / %.1f MB (%.1f%%)", 
                         metrics.jetson_memory_used, metrics.jetson_memory_total, metrics.jetson_memory_usage);
            } else {
                ESP_LOGI(TAG, "  Jetson数据无效");
            }
        }
    } else {
        ESP_LOGI(TAG, "监控数据更新失败: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief Board WS2812显示测试主函数
 */
void test_board_ws2812_display_main(void) {
    ESP_LOGI(TAG, "========== Board WS2812显示功能测试开始 ==========");
    
    // 注意：此测试假设Board WS2812已经通过BSP初始化流程完成初始化
    // 如果需要单独测试，需要先调用 bsp_ws2812_init(BSP_WS2812_ONBOARD)
    
    // 初始化Board WS2812显示控制器
    ESP_LOGI(TAG, "初始化Board WS2812显示控制器");
    board_display_config_t config = bsp_board_ws2812_display_get_default_config();
    config.debug_mode = true;  // 启用调试模式以查看详细日志
    
    esp_err_t ret = bsp_board_ws2812_display_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Board WS2812显示控制器初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    
    // 启动显示控制器
    ret = bsp_board_ws2812_display_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Board WS2812显示控制器启动失败: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "Board WS2812显示控制器启动成功，开始测试");
    
    // 等待系统稳定
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 执行各项测试
    test_basic_colors();
    test_breath_effects();
    test_predefined_modes();
    test_brightness_control();
    test_metrics_update();
    print_status_info();
    
    ESP_LOGI(TAG, "========== 测试完成，恢复自动模式 ==========");
    
    // 恢复自动模式
    bsp_board_ws2812_display_resume_auto();
    
    ESP_LOGI(TAG, "Board WS2812显示控制器已恢复自动模式");
    ESP_LOGI(TAG, "========== Board WS2812显示功能测试结束 ==========");
}

/**
 * @brief 演示Board WS2812自动模式
 */
void demo_board_ws2812_auto_mode(void) {
    ESP_LOGI(TAG, "========== Board WS2812自动模式演示 ==========");
      ESP_LOGI(TAG, "在自动模式下，Board WS2812会根据以下规则显示:");
    ESP_LOGI(TAG, "  高温警告（N305 >= 95°C 或 Jetson >= 80°C）: 红色慢速呼吸 (高优先级)");
    ESP_LOGI(TAG, "  功率过高（Jetson >= 45W）: 紫色快速呼吸 (中优先级)");
    ESP_LOGI(TAG, "  内存高使用率（>= 90%%）: 白色慢速呼吸 (低优先级)");ESP_LOGI(TAG, "  正常状态: 关闭显示");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "监控数据来源:");
    ESP_LOGI(TAG, "  统一API: %s", BOARD_PROMETHEUS_API);
    ESP_LOGI(TAG, "  更新间隔: %d秒", BOARD_METRICS_UPDATE_INTERVAL / 1000);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "系统将持续监控这些指标并自动调整显示状态");
    ESP_LOGI(TAG, "========================================");
}
