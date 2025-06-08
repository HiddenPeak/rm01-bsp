/*
 * 网络监控实时性优化示例
 * 展示如何使用优化后的网络监控系统获得更好的实时性
 */

#include "network_monitor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

static const char *TAG = "NM_EXAMPLE";

// 网络状态变化回调示例
void network_status_callback(uint8_t index, const char* ip, int status, void* arg) {
    const char* target_names[] = {"算力模块", "应用模块", "用户主机", "互联网"};
    const char* status_str = (status == NM_STATUS_UP) ? "连接" : "断开";
    
    ESP_LOGI(TAG, "回调通知: %s (%s) 状态变为 %s", 
             target_names[index], ip, status_str);
}

void network_monitor_realtime_example(void) {
    ESP_LOGI(TAG, "=== 网络监控实时性优化示例 ===");
    
    // 1. 初始化网络监控系统
    nm_init();
    
    // 2. 注册状态变化回调，实现即时通知
    nm_register_status_change_callback(network_status_callback, NULL);
    
    // 3. 启用快速监控模式，提高检测频率
    nm_enable_fast_monitoring(true);
    ESP_LOGI(TAG, "已启用快速监控模式");
    
    // 4. 自定义监控间隔（可选）
    nm_set_monitoring_interval(800);  // 设置为800ms间隔
    ESP_LOGI(TAG, "设置监控间隔为800ms");
    
    // 5. 启动监控
    nm_start_monitoring();
    ESP_LOGI(TAG, "网络监控已启动");
    
    // 6. 使用事件组等待特定网络事件
    EventGroupHandle_t event_group = nm_get_event_group();
    
    ESP_LOGI(TAG, "等待网络状态变化事件...");
    
    // 示例：等待用户主机连接事件
    while (1) {
        EventBits_t events = xEventGroupWaitBits(
            event_group,
            NM_EVENT_USER_HOST_UP | NM_EVENT_USER_HOST_DOWN |
            NM_EVENT_COMPUTING_UP | NM_EVENT_COMPUTING_DOWN,
            pdTRUE,  // 清除事件位
            pdFALSE, // 等待任意一个事件
            pdMS_TO_TICKS(5000)  // 5秒超时
        );
        
        if (events & NM_EVENT_USER_HOST_UP) {
            ESP_LOGI(TAG, "检测到用户主机连接事件！");
        }
        if (events & NM_EVENT_USER_HOST_DOWN) {
            ESP_LOGI(TAG, "检测到用户主机断开事件！");
        }
        if (events & NM_EVENT_COMPUTING_UP) {
            ESP_LOGI(TAG, "检测到算力模块连接事件！");
        }
        if (events & NM_EVENT_COMPUTING_DOWN) {
            ESP_LOGI(TAG, "检测到算力模块断开事件！");
        }
        
        // 展示当前所有目标状态
        nm_get_all_status();
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void network_monitor_performance_comparison(void) {
    ESP_LOGI(TAG, "=== 网络监控性能对比测试 ===");
    
    nm_init();
    nm_register_status_change_callback(network_status_callback, NULL);
    
    // 测试普通模式
    ESP_LOGI(TAG, "开始普通模式测试（3秒间隔）...");
    nm_enable_fast_monitoring(false);
    nm_set_monitoring_interval(3000);
    nm_start_monitoring();
    
    vTaskDelay(pdMS_TO_TICKS(15000));  // 运行15秒
    
    nm_stop_monitoring();
    
    // 测试快速模式
    ESP_LOGI(TAG, "开始快速模式测试（1秒间隔）...");
    nm_enable_fast_monitoring(true);
    nm_set_monitoring_interval(1000);
    nm_start_monitoring();
    
    vTaskDelay(pdMS_TO_TICKS(15000));  // 运行15秒
    
    nm_stop_monitoring();
    
    ESP_LOGI(TAG, "性能对比测试完成");
}
