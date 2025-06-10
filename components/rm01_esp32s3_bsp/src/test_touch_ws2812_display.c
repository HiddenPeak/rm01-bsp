/**
 * @file test_touch_ws2812_display.c
 * @brief Touch WS2812 显示功能测试程序
 */

#include "bsp_touch_ws2812_display.h"
#include "bsp_display_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TOUCH_WS2812_TEST";

/**
 * @brief 测试Touch WS2812基本颜色显示
 */
static void test_basic_colors(void) {
    ESP_LOGI(TAG, "========== 测试基本颜色显示 ==========");
    
    // 测试各种基本颜色
    ESP_LOGI(TAG, "显示红色");
    bsp_touch_ws2812_display_set_color(255, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "显示绿色");
    bsp_touch_ws2812_display_set_color(0, 255, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "显示蓝色");
    bsp_touch_ws2812_display_set_color(0, 0, 255);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "显示白色");
    bsp_touch_ws2812_display_set_color(255, 255, 255);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "显示橙色");
    bsp_touch_ws2812_display_set_color(255, 165, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "显示黄色");
    bsp_touch_ws2812_display_set_color(255, 255, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "关闭LED");
    bsp_touch_ws2812_display_off();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief 测试Touch WS2812闪烁效果
 */
static void test_blink_effects(void) {
    ESP_LOGI(TAG, "========== 测试闪烁效果 ==========");
    
    ESP_LOGI(TAG, "测试蓝色快速闪烁（N305错误模拟）");
    bsp_touch_ws2812_display_set_blink(0, 0, 255, BLINK_SPEED_FAST);
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    ESP_LOGI(TAG, "测试黄色正常闪烁（Jetson错误模拟）");
    bsp_touch_ws2812_display_set_blink(255, 255, 0, BLINK_SPEED_NORMAL);
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    ESP_LOGI(TAG, "测试绿色慢速闪烁（用户主机警告模拟）");
    bsp_touch_ws2812_display_set_blink(0, 255, 0, BLINK_SPEED_SLOW);
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    bsp_touch_ws2812_display_off();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief 测试Touch WS2812呼吸灯效果
 */
static void test_breath_effects(void) {
    ESP_LOGI(TAG, "========== 测试呼吸灯效果 ==========");
    
    ESP_LOGI(TAG, "测试白色快速呼吸（启动模式模拟）");
    bsp_touch_ws2812_display_set_breath(255, 255, 255, BREATH_SPEED_FAST);
    vTaskDelay(pdMS_TO_TICKS(6000));
    
    ESP_LOGI(TAG, "测试白色慢速呼吸（无互联网待机模拟）");
    bsp_touch_ws2812_display_set_breath(255, 255, 255, BREATH_SPEED_SLOW);
    vTaskDelay(pdMS_TO_TICKS(6000));
    
    ESP_LOGI(TAG, "测试橙色慢速呼吸（有互联网待机模拟）");
    bsp_touch_ws2812_display_set_breath(255, 165, 0, BREATH_SPEED_SLOW);
    vTaskDelay(pdMS_TO_TICKS(6000));
    
    bsp_touch_ws2812_display_off();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief 测试Touch WS2812显示模式
 */
static void test_display_modes(void) {
    ESP_LOGI(TAG, "========== 测试预定义显示模式 ==========");
    
    ESP_LOGI(TAG, "测试初始化模式（白色常亮）");
    bsp_touch_ws2812_display_set_mode(TOUCH_DISPLAY_MODE_INIT);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "测试N305错误模式（蓝色闪烁）");
    bsp_touch_ws2812_display_set_mode(TOUCH_DISPLAY_MODE_N305_ERROR);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "测试Jetson错误模式（黄色闪烁）");
    bsp_touch_ws2812_display_set_mode(TOUCH_DISPLAY_MODE_JETSON_ERROR);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "测试用户主机警告模式（绿色闪烁）");
    bsp_touch_ws2812_display_set_mode(TOUCH_DISPLAY_MODE_USER_HOST_WARNING);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "测试启动模式（白色快速呼吸）");
    bsp_touch_ws2812_display_set_mode(TOUCH_DISPLAY_MODE_STARTUP);
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    ESP_LOGI(TAG, "测试无互联网待机模式（白色慢速呼吸）");
    bsp_touch_ws2812_display_set_mode(TOUCH_DISPLAY_MODE_STANDBY_NO_INTERNET);
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    ESP_LOGI(TAG, "测试有互联网待机模式（橙色慢速呼吸）");
    bsp_touch_ws2812_display_set_mode(TOUCH_DISPLAY_MODE_STANDBY_WITH_INTERNET);
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    ESP_LOGI(TAG, "测试多重错误模式（多色闪烁切换）");
    bsp_touch_ws2812_display_set_mode(TOUCH_DISPLAY_MODE_MULTI_ERROR);
    vTaskDelay(pdMS_TO_TICKS(8000));
    
    bsp_touch_ws2812_display_off();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief 测试Touch WS2812亮度调节
 */
static void test_brightness_control(void) {
    ESP_LOGI(TAG, "========== 测试亮度调节 ==========");
    
    uint8_t brightness_levels[] = {255, 192, 128, 64, 32, 16};
    size_t num_levels = sizeof(brightness_levels) / sizeof(brightness_levels[0]);
    
    for (size_t i = 0; i < num_levels; i++) {
        ESP_LOGI(TAG, "设置亮度为: %d", brightness_levels[i]);
        bsp_touch_ws2812_display_set_brightness(brightness_levels[i]);
        bsp_touch_ws2812_display_set_color(255, 255, 255);  // 白色
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    // 恢复默认亮度
    bsp_touch_ws2812_display_set_brightness(128);
    bsp_touch_ws2812_display_off();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief 测试状态信息获取
 */
static void test_status_info(void) {
    ESP_LOGI(TAG, "========== 测试状态信息获取 ==========");
    
    // 打印详细状态信息
    bsp_touch_ws2812_display_print_status();
    
    // 获取状态结构
    touch_display_status_t status;
    esp_err_t ret = bsp_touch_ws2812_display_get_status(&status);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "状态获取成功:");
        ESP_LOGI(TAG, "  当前模式: %s", bsp_touch_ws2812_display_get_mode_name(status.current_mode));
        ESP_LOGI(TAG, "  激活状态: %s", status.is_active ? "是" : "否");
        ESP_LOGI(TAG, "  模式变化次数: %ld", status.mode_change_count);
        ESP_LOGI(TAG, "  在当前模式时间: %ld ms", status.time_in_current_mode);
    } else {
        ESP_LOGE(TAG, "获取状态失败: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief Touch WS2812显示测试主函数
 */
void test_touch_ws2812_display_main(void) {
    ESP_LOGI(TAG, "========== Touch WS2812显示功能测试开始 ==========");
    
    // 注意：此测试假设Touch WS2812已经通过BSP初始化流程完成初始化
    // 如果需要单独测试，需要先调用 bsp_ws2812_init(BSP_WS2812_TOUCH)
    
    // 初始化Touch WS2812显示控制器
    ESP_LOGI(TAG, "初始化Touch WS2812显示控制器");
    touch_display_config_t config = bsp_touch_ws2812_display_get_default_config();
    config.debug_mode = true;  // 启用调试模式以查看详细日志
    
    esp_err_t ret = bsp_touch_ws2812_display_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch WS2812显示控制器初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    
    // 启动显示控制器
    ret = bsp_touch_ws2812_display_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch WS2812显示控制器启动失败: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "Touch WS2812显示控制器启动成功，开始测试");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 暂停自动模式进行手动测试
    bsp_touch_ws2812_display_set_auto_mode(false);
    
    // 执行各项测试
    test_basic_colors();
    test_blink_effects();
    test_breath_effects();
    test_display_modes();
    test_brightness_control();
    test_status_info();
    
    ESP_LOGI(TAG, "========== 所有测试完成 ==========");
    
    // 恢复自动模式
    ESP_LOGI(TAG, "恢复自动模式，Touch WS2812将根据系统状态自动显示");
    bsp_touch_ws2812_display_set_auto_mode(true);
    bsp_touch_ws2812_display_resume_auto();
    
    ESP_LOGI(TAG, "Touch WS2812显示功能测试结束");
}

/**
 * @brief 演示Touch WS2812自动模式
 */
void demo_touch_ws2812_auto_mode(void) {
    ESP_LOGI(TAG, "========== Touch WS2812自动模式演示 ==========");
    
    ESP_LOGI(TAG, "在自动模式下，Touch WS2812会根据以下规则显示:");
    ESP_LOGI(TAG, "  初始化阶段（前30秒）: 白色常亮");
    ESP_LOGI(TAG, "  N305错误（超过60秒未ping到）: 蓝色闪烁");
    ESP_LOGI(TAG, "  Jetson错误（超过60秒未ping到）: 黄色闪烁");
    ESP_LOGI(TAG, "  用户主机警告（未连接）: 绿色闪烁");
    ESP_LOGI(TAG, "  启动中（30秒后各模组可ping）: 白色快速呼吸");
    ESP_LOGI(TAG, "  无互联网待机（240秒后）: 白色慢速呼吸");
    ESP_LOGI(TAG, "  有互联网待机: 橙色慢速呼吸");
    ESP_LOGI(TAG, "  多重错误: 多种颜色闪烁切换");
    
    // 打印当前状态
    bsp_touch_ws2812_display_print_status();
    
    ESP_LOGI(TAG, "请观察Touch WS2812的显示变化...");
}
