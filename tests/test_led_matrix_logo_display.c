#include "led_matrix_logo_display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LED_LOGO_TEST";

/**
 * @brief LED Matrix Logo Display Controller 功能测试
 */
void test_led_matrix_logo_display(void) {
    ESP_LOGI(TAG, "========== LED Matrix Logo Display Controller 测试开始 ==========");
    
    // 测试1: 初始化
    ESP_LOGI(TAG, "测试1: 初始化LED Matrix Logo Display Controller");
    esp_err_t ret = led_matrix_logo_display_init(NULL);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ 初始化成功");
    } else {
        ESP_LOGE(TAG, "✗ 初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    
    // 测试2: 检查初始化状态
    ESP_LOGI(TAG, "测试2: 检查初始化状态");
    bool is_initialized = led_matrix_logo_display_is_initialized();
    ESP_LOGI(TAG, "初始化状态: %s", is_initialized ? "✓ 已初始化" : "✗ 未初始化");
    
    // 测试3: 启动服务
    ESP_LOGI(TAG, "测试3: 启动Logo显示服务");
    ret = led_matrix_logo_display_start();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ 服务启动成功");
    } else {
        ESP_LOGE(TAG, "✗ 服务启动失败: %s", esp_err_to_name(ret));
    }
    
    // 测试4: 检查运行状态
    ESP_LOGI(TAG, "测试4: 检查运行状态");
    bool is_running = led_matrix_logo_display_is_running();
    ESP_LOGI(TAG, "运行状态: %s", is_running ? "✓ 正在运行" : "✗ 未运行");
    
    // 测试5: 显示模式设置
    ESP_LOGI(TAG, "测试5: 设置显示模式");
    
    // 测试关闭模式
    ret = led_matrix_logo_display_set_mode(LOGO_DISPLAY_MODE_OFF);
    ESP_LOGI(TAG, "设置关闭模式: %s", ret == ESP_OK ? "✓ 成功" : "✗ 失败");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 测试单一显示模式
    ret = led_matrix_logo_display_set_mode(LOGO_DISPLAY_MODE_SINGLE);
    ESP_LOGI(TAG, "设置单一显示模式: %s", ret == ESP_OK ? "✓ 成功" : "✗ 失败");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 测试序列显示模式
    ret = led_matrix_logo_display_set_mode(LOGO_DISPLAY_MODE_SEQUENCE);
    ESP_LOGI(TAG, "设置序列显示模式: %s", ret == ESP_OK ? "✓ 成功" : "✗ 失败");
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 测试6: 获取当前模式
    ESP_LOGI(TAG, "测试6: 获取当前显示模式");
    logo_display_mode_t current_mode = led_matrix_logo_display_get_mode();
    ESP_LOGI(TAG, "当前模式: %d", current_mode);
    
    // 测试7: 亮度设置
    ESP_LOGI(TAG, "测试7: 设置亮度");
    ret = led_matrix_logo_display_set_brightness(128);
    ESP_LOGI(TAG, "设置亮度到128: %s", ret == ESP_OK ? "✓ 成功" : "✗ 失败");
    
    uint8_t brightness = led_matrix_logo_display_get_brightness();
    ESP_LOGI(TAG, "当前亮度: %d", brightness);
    
    // 测试8: 状态查询
    ESP_LOGI(TAG, "测试8: 查询状态信息");
    logo_display_status_t status;
    ret = led_matrix_logo_display_get_status(&status);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ 状态查询成功");
        ESP_LOGI(TAG, "  当前模式: %d", status.current_mode);
        ESP_LOGI(TAG, "  当前Logo: %s", status.current_logo_name[0] ? status.current_logo_name : "无");
        ESP_LOGI(TAG, "  运行时间: %lu ms", status.uptime_ms);
        ESP_LOGI(TAG, "  显示次数: %lu", status.display_count);
        ESP_LOGI(TAG, "  切换次数: %lu", status.mode_switch_count);
        ESP_LOGI(TAG, "  Logo数量: %d", status.logo_count);
    } else {
        ESP_LOGE(TAG, "✗ 状态查询失败: %s", esp_err_to_name(ret));
    }
    
    // 测试9: 打印详细状态
    ESP_LOGI(TAG, "测试9: 打印详细状态");
    led_matrix_logo_display_print_status();
    
    // 测试10: 配置文件加载
    ESP_LOGI(TAG, "测试10: 尝试加载配置文件");
    ret = led_matrix_logo_display_load_config("/sdcard/logo_config.json");
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ 配置文件加载成功");
    } else {
        ESP_LOGW(TAG, "⚠ 配置文件加载失败: %s (可能是文件不存在)", esp_err_to_name(ret));
    }
    
    // 测试11: Logo重新加载
    ESP_LOGI(TAG, "测试11: 尝试重新加载Logo文件");
    ret = led_matrix_logo_display_reload_logos();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Logo文件重新加载成功");
    } else {
        ESP_LOGW(TAG, "⚠ Logo文件重新加载失败: %s", esp_err_to_name(ret));
    }
    
    // 测试12: 显示指定Logo
    ESP_LOGI(TAG, "测试12: 尝试显示指定Logo");
    ret = led_matrix_logo_display_show_logo("startup_logo");
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Logo显示命令成功");
    } else {
        ESP_LOGW(TAG, "⚠ Logo显示失败: %s", esp_err_to_name(ret));
    }
    
    // 等待一段时间让用户观察显示效果
    ESP_LOGI(TAG, "等待5秒让用户观察显示效果...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // 测试13: 调试模式
    ESP_LOGI(TAG, "测试13: 启用调试模式");
    led_matrix_logo_display_set_debug(true);
    ESP_LOGI(TAG, "✓ 调试模式已启用");
    
    // 再次打印状态以查看调试信息
    led_matrix_logo_display_print_status();
    
    // 测试14: 停止服务
    ESP_LOGI(TAG, "测试14: 停止Logo显示服务");
    led_matrix_logo_display_stop();
    ESP_LOGI(TAG, "✓ 服务已停止");
    
    // 检查停止后的运行状态
    is_running = led_matrix_logo_display_is_running();
    ESP_LOGI(TAG, "停止后运行状态: %s", is_running ? "✗ 仍在运行" : "✓ 已停止");
    
    ESP_LOGI(TAG, "========== LED Matrix Logo Display Controller 测试完成 ==========");
}

/**
 * @brief 演示LED Matrix Logo Display Controller的各种功能
 */
void demo_led_matrix_logo_display(void) {
    ESP_LOGI(TAG, "========== LED Matrix Logo Display Controller 演示开始 ==========");
    
    // 初始化
    esp_err_t ret = led_matrix_logo_display_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化失败，演示终止");
        return;
    }
    
    // 启动服务
    led_matrix_logo_display_start();
    
    ESP_LOGI(TAG, "演示1: 关闭模式 (5秒)");
    led_matrix_logo_display_set_mode(LOGO_DISPLAY_MODE_OFF);
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    ESP_LOGI(TAG, "演示2: 单一显示模式 (10秒)");
    led_matrix_logo_display_set_mode(LOGO_DISPLAY_MODE_SINGLE);
    vTaskDelay(pdMS_TO_TICKS(10000));
    
    ESP_LOGI(TAG, "演示3: 序列显示模式 (15秒)");
    led_matrix_logo_display_set_mode(LOGO_DISPLAY_MODE_SEQUENCE);
    vTaskDelay(pdMS_TO_TICKS(15000));
    
    ESP_LOGI(TAG, "演示4: 定时切换模式 (15秒)");
    led_matrix_logo_display_set_mode(LOGO_DISPLAY_MODE_TIMED_SWITCH);
    vTaskDelay(pdMS_TO_TICKS(15000));
    
    ESP_LOGI(TAG, "演示5: 随机显示模式 (15秒)");
    led_matrix_logo_display_set_mode(LOGO_DISPLAY_MODE_RANDOM);
    vTaskDelay(pdMS_TO_TICKS(15000));
    
    ESP_LOGI(TAG, "演示6: 亮度调节演示");
    led_matrix_logo_display_set_mode(LOGO_DISPLAY_MODE_SINGLE);
    
    uint8_t brightness_levels[] = {50, 100, 150, 200, 255};
    for (int i = 0; i < 5; i++) {
        ESP_LOGI(TAG, "设置亮度为: %d", brightness_levels[i]);
        led_matrix_logo_display_set_brightness(brightness_levels[i]);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    
    // 恢复正常亮度
    led_matrix_logo_display_set_brightness(128);
    
    ESP_LOGI(TAG, "演示完成，设置为单一显示模式");
    led_matrix_logo_display_set_mode(LOGO_DISPLAY_MODE_SINGLE);
    
    ESP_LOGI(TAG, "========== LED Matrix Logo Display Controller 演示完成 ==========");
}
