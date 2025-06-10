/**
 * @file test_integration.c
 * @brief BSP状态接口集成测试
 * 
 * 简单的集成测试，验证新的统一状态接口是否正确编译和链接
 */

#include <stdio.h>
#include "esp_log.h"
#include "bsp_status_interface.h"

static const char *TAG = "BSP_INTEGRATION_TEST";

void app_main(void) {
    ESP_LOGI(TAG, "BSP状态接口集成测试开始");
    
    // 初始化统一状态接口
    esp_err_t ret = bsp_status_interface_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ BSP统一状态接口初始化成功");
    } else {
        ESP_LOGE(TAG, "❌ BSP统一状态接口初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    
    // 测试基本状态查询
    unified_system_status_t status;
    ret = bsp_get_system_status(&status);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 基本状态查询成功");
        ESP_LOGI(TAG, "   系统运行时间: %lu 秒", status.system_uptime_seconds);
        ESP_LOGI(TAG, "   当前系统状态: %d", status.current_state);
        ESP_LOGI(TAG, "   状态变化次数: %lu", status.state_change_count);
    } else {
        ESP_LOGE(TAG, "❌ 基本状态查询失败: %s", esp_err_to_name(ret));
    }
    
    // 测试缓存状态查询
    ret = bsp_get_system_status_cached(&status, 5000);  // 5秒缓存
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 缓存状态查询成功");
    } else {
        ESP_LOGE(TAG, "❌ 缓存状态查询失败: %s", esp_err_to_name(ret));
    }
    
    // 测试性能统计
    bsp_status_performance_stats_t perf_stats;
    ret = bsp_get_performance_stats(&perf_stats);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 性能统计查询成功");
        ESP_LOGI(TAG, "   查询次数: %lu", perf_stats.total_queries);
        ESP_LOGI(TAG, "   缓存命中率: %.1f%%", perf_stats.cache_hit_rate);
        ESP_LOGI(TAG, "   平均查询时间: %lu us", perf_stats.average_query_time_us);
    } else {
        ESP_LOGE(TAG, "❌ 性能统计查询失败: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "🎉 BSP状态接口集成测试完成");
    
    // 清理资源
    bsp_status_interface_deinit();
    ESP_LOGI(TAG, "✅ BSP统一状态接口清理完成");
}
