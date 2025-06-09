#include "bsp_board.h"
#include "unity.h"
#include "esp_log.h"

static const char *TAG = "BSP_TESTS";

// 基础功能测试
TEST_CASE("BSP初始化和清理测试", "[bsp_board]")
{
    ESP_LOGI(TAG, "开始BSP初始化和清理测试");
    
    // 测试初始化
    esp_err_t ret = bsp_board_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // 检查初始化状态
    TEST_ASSERT_TRUE(bsp_board_is_initialized());
    
    // 等待一段时间让系统稳定
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 测试清理
    ret = bsp_board_cleanup();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    ESP_LOGI(TAG, "BSP初始化和清理测试通过");
}

// 性能统计测试
TEST_CASE("BSP性能统计测试", "[bsp_board][performance]")
{
    ESP_LOGI(TAG, "开始BSP性能统计测试");
    
    // 重置性能统计
    bsp_board_reset_performance_stats();
    
    // 更新性能统计
    bsp_board_update_performance_stats();
    
    // 增加一些计数器
    for (int i = 0; i < 10; i++) {
        bsp_board_increment_animation_frames();
        bsp_board_increment_network_errors();
        bsp_board_increment_power_fluctuations();
    }
    
    // 打印性能统计
    bsp_board_print_performance_stats();
    
    ESP_LOGI(TAG, "BSP性能统计测试通过");
}

// 健康检查测试
TEST_CASE("BSP健康检查测试", "[bsp_board][health]")
{
    ESP_LOGI(TAG, "开始BSP健康检查测试");
    
    // 先初始化BSP
    esp_err_t ret = bsp_board_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // 等待系统稳定
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 基础健康检查
    ret = bsp_board_health_check();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // 扩展健康检查
    ret = bsp_board_health_check_extended();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // 打印系统信息
    bsp_board_print_system_info();
    
    // 清理资源
    bsp_board_cleanup();
    
    ESP_LOGI(TAG, "BSP健康检查测试通过");
}

// 错误恢复测试
TEST_CASE("BSP错误恢复测试", "[bsp_board][recovery]")
{
    ESP_LOGI(TAG, "开始BSP错误恢复测试");
    
    // 初始化BSP
    esp_err_t ret = bsp_board_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // 模拟错误情况（停止动画任务）
    bsp_stop_animation_task();
    
    // 验证系统检测到错误
    TEST_ASSERT_FALSE(bsp_board_is_initialized());
    
    // 执行错误恢复
    ret = bsp_board_error_recovery();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // 验证恢复成功
    TEST_ASSERT_TRUE(bsp_board_is_initialized());
    
    // 清理资源
    bsp_board_cleanup();
    
    ESP_LOGI(TAG, "BSP错误恢复测试通过");
}

// 压力测试
TEST_CASE("BSP压力测试", "[bsp_board][stress]")
{
    ESP_LOGI(TAG, "开始BSP压力测试");
    
    // 初始化BSP
    esp_err_t ret = bsp_board_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // 运行一段时间的主循环模拟
    int test_cycles = 10;
    for (int i = 0; i < test_cycles; i++) {
        // 更新性能统计
        bsp_board_update_performance_stats();
        
        // 模拟一些活动
        bsp_board_increment_animation_frames();
        
        // 进行健康检查
        if (i % 5 == 0) {
            ret = bsp_board_health_check();
            TEST_ASSERT_EQUAL(ESP_OK, ret);
        }
        
        // 短暂延迟
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // 打印最终统计
    bsp_board_print_performance_stats();
    
    // 清理资源
    bsp_board_cleanup();
    
    ESP_LOGI(TAG, "BSP压力测试通过");
}

// 内存泄漏测试
TEST_CASE("BSP内存泄漏测试", "[bsp_board][memory]")
{
    ESP_LOGI(TAG, "开始BSP内存泄漏测试");
    
    size_t initial_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "初始堆内存: %d 字节", initial_heap);
    
    // 多次初始化和清理循环
    for (int cycle = 0; cycle < 3; cycle++) {
        ESP_LOGI(TAG, "内存测试循环 %d/3", cycle + 1);
        
        // 初始化
        esp_err_t ret = bsp_board_init();
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        
        // 运行一段时间
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // 更新统计
        bsp_board_update_performance_stats();
        
        // 清理
        ret = bsp_board_cleanup();
        TEST_ASSERT_EQUAL(ESP_OK, ret);
          // 检查内存
        size_t current_heap = esp_get_free_heap_size();
        ESP_LOGI(TAG, "循环 %d 后堆内存: %zu 字节", cycle + 1, current_heap);
        
        // 允许一些内存波动，但不应该有显著泄漏
        TEST_ASSERT_GREATER_THAN(initial_heap - 5000, current_heap);
    }
    
    size_t final_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "最终堆内存: %d 字节", final_heap);
    ESP_LOGI(TAG, "内存差异: %d 字节", (int)(initial_heap - final_heap));
    
    ESP_LOGI(TAG, "BSP内存泄漏测试通过");
}

void run_bsp_board_tests(void)
{
    ESP_LOGI(TAG, "开始运行BSP板级测试套件");
    
    UNITY_BEGIN();
    
    RUN_TEST(BSP初始化和清理测试);
    RUN_TEST(BSP性能统计测试);
    RUN_TEST(BSP健康检查测试);
    RUN_TEST(BSP错误恢复测试);
    RUN_TEST(BSP压力测试);
    RUN_TEST(BSP内存泄漏测试);
    
    UNITY_END();
    
    ESP_LOGI(TAG, "BSP板级测试套件执行完成");
}
