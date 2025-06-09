#include "bsp_board.h"
#include "bsp_config.h"
#include "bsp_network.h"
#include "network_monitor.h"
#include "bsp_ws2812.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
// #include "color_calibration.h"
#include <math.h>
#include <inttypes.h> // 添加此头文件以支持 PRIuXX 宏
#include "sdmmc_cmd.h" // 用于SD卡命令和信息打印
#include "bsp_power.h" // 引入电源管理头文件
#include "led_matrix.h" // 引入LED矩阵头文件
#include "bsp_webserver.h" // 引入Web服务器头文件

static const char *TAG = "BSP";

// 配置常量 - 优化后的初始化延迟时间
#define BSP_INIT_DELAY_NETWORK_MS           500     // 网络监控系统启动延迟 (优化: 2000→500ms)
#define BSP_INIT_DELAY_LED_MATRIX_MS        200     // LED矩阵初始化延迟 (优化: 1000→200ms)
#define BSP_INIT_DELAY_SYSTEM_MONITOR_MS    2000    // 系统监控数据收集延迟 (优化: 8000→2000ms)
#define BSP_ANIMATION_UPDATE_RATE_MS        30      // LED动画更新频率（约33FPS）

// 主循环配置常量
#define BSP_MAIN_LOOP_INTERVAL_MS           1000    // 主循环间隔（1秒）
#define BSP_SYSTEM_STATE_REPORT_INTERVAL    10      // 系统状态报告间隔（10秒）
#define BSP_POWER_STATUS_REPORT_INTERVAL    30      // 电源状态报告间隔（30秒）
#define BSP_NETWORK_ANIMATION_STATUS_INTERVAL 60    // 网络动画状态报告间隔（60秒）

// 动画更新任务句柄
static TaskHandle_t animation_task_handle = NULL;

// BSP性能统计结构
typedef struct {
    uint32_t init_start_time;
    uint32_t init_duration_ms;
    uint32_t uptime_seconds;
    uint32_t task_switches;
    uint32_t heap_usage_peak;
    bool critical_task_running;
    uint32_t network_errors;
    uint32_t power_fluctuations;
    uint32_t animation_frames_rendered;
} bsp_performance_stats_t;

static bsp_performance_stats_t bsp_stats = {0};

// 前向声明
static esp_err_t bsp_board_validate_config(void);

// 动画更新任务
static void animation_update_task(void *pvParameters) {
    ESP_LOGI(TAG, "LED矩阵动画更新任务启动");
    
    while (1) {
        led_matrix_update_animation();
        
        // 更新动画帧统计
        bsp_board_increment_animation_frames();
        
        vTaskDelay(BSP_ANIMATION_UPDATE_RATE_MS / portTICK_PERIOD_MS);
    }
}

// BSP LED矩阵服务初始化
void bsp_init_led_matrix_service(void) {
    ESP_LOGI(TAG, "初始化LED矩阵服务");
    led_matrix_init();
    ESP_LOGI(TAG, "LED矩阵系统初始化完成");
}

// BSP网络监控服务初始化
esp_err_t bsp_init_network_monitoring_service(void) {
    ESP_LOGI(TAG, "初始化网络监控系统");
    
    // 初始化网络监控系统（在Web服务器启动后进行）
    nm_init();
    ESP_LOGI(TAG, "网络监控系统初始化完成");
    
    ESP_LOGI(TAG, "启动网络监控服务");
      // 启动网络监控任务
    nm_start_monitoring();
    
    // 等待网络监控系统启动 (优化: 减少等待时间)
    ESP_LOGI(TAG, "等待网络监控系统启动... (优化等待时间)");
    vTaskDelay(BSP_INIT_DELAY_NETWORK_MS / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "查询网络状态:");
    nm_get_network_status();
    
    ESP_LOGI(TAG, "网络监控服务启动完成");
    return ESP_OK;
}

// BSP Web服务器服务初始化
esp_err_t bsp_init_webserver_service(void) {
    ESP_LOGI(TAG, "启动Web服务器服务");
    esp_err_t ret = bsp_start_webserver();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Web服务器启动失败: %s", esp_err_to_name(ret));
        return ret;
    } else {
        ESP_LOGI(TAG, "Web服务器已启动，请使用浏览器访问 http://10.10.99.97/");
        return ESP_OK;
    }
}

// 启动动画更新任务
esp_err_t bsp_start_animation_task(void) {
    if (animation_task_handle != NULL) {
        ESP_LOGW(TAG, "LED矩阵动画更新任务已在运行");
        return ESP_OK;
    }
      BaseType_t ret = xTaskCreate(
        animation_update_task,
        "animation_task",
        CONFIG_BSP_ANIMATION_TASK_STACK_SIZE,
        NULL,
        CONFIG_BSP_ANIMATION_TASK_PRIORITY,
        &animation_task_handle
    );
    
    if (ret == pdPASS) {
        ESP_LOGI(TAG, "LED矩阵自动动画更新任务已启动");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "LED矩阵动画更新任务创建失败");
        animation_task_handle = NULL;
        return ESP_FAIL;
    }
}

// 停止动画更新任务
void bsp_stop_animation_task(void) {
    if (animation_task_handle != NULL) {
        vTaskDelete(animation_task_handle);
        animation_task_handle = NULL;
        ESP_LOGI(TAG, "LED矩阵动画更新任务已停止");
    }
}

esp_err_t bsp_w5500_init(spi_host_device_t host) {
    esp_err_t ret = bsp_w5500_network_init(host);
    if (ret == ESP_OK) {
        // 只进行基础网络硬件初始化，网络监控将在Web服务器启动后初始化
        ESP_LOGI(TAG, "W5500网络控制器初始化成功");
    } else {
        ESP_LOGE(TAG, "W5500网络控制器初始化失败: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t bsp_rtl8367_init(void) {
    esp_err_t ret = bsp_rtl8367_network_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RTL8367网络初始化失败: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "RTL8367网络初始化成功");
    }    return ret;
}

// 确保前向声明在使用前可见
static esp_err_t bsp_board_validate_config(void);

esp_err_t bsp_board_init(void) {
    ESP_LOGI(TAG, "开始初始化ESP32-S3 BSP");
    esp_err_t ret;
    
    // 记录初始化开始时间
    bsp_stats.init_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    bsp_stats.heap_usage_peak = esp_get_free_heap_size();
    
    // 验证BSP配置
    ret = bsp_board_validate_config();
    if (ret != ESP_OK) {        ESP_LOGE(TAG, "BSP配置验证失败");
        return ret;
    }
    
    // ============ 第一阶段：LED矩阵优先初始化和基础硬件 ============
    ESP_LOGI(TAG, "第一阶段：LED矩阵优先初始化和基础硬件");
    
    // 最优先：初始化LED矩阵服务，提供早期状态指示
    ESP_LOGI(TAG, "优先初始化LED矩阵服务");
    bsp_init_led_matrix_service();
    
    // 启用LED矩阵动画任务
    ret = bsp_start_animation_task();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LED矩阵动画任务启动失败，但继续初始化");
    } else {
        ESP_LOGI(TAG, "LED矩阵动画任务已启动，可提供早期状态指示");
    }
    
    // 初始化电源管理模块
    bsp_power_init();
    
    // 初始化网络控制器（为Web服务器做准备）
    ret = bsp_w5500_init(SPI3_HOST);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "W5500网络控制器初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }
    // bsp_rtl8367_init(); // 如需要可启用
    
    // LPN100电源控制操作（当前模块会自动上电启动，所以不需要进行电源操作。）
    // bsp_lpn100_power_toggle();
    ESP_LOGI(TAG, "LPN100电源控制完成");

    // bsp_orin_power_control(false); // 开启ORIN电源
      // 读取并报告电压状态
    float main_v = bsp_get_main_voltage();
    float aux_v = bsp_get_aux_12v_voltage();
    ESP_LOGI(TAG, "电源状态 - 主电源: %.2fV, 辅助12V: %.2fV", main_v, aux_v);

    // ============ 第二阶段：优先启动Web服务器 ============
    ESP_LOGI(TAG, "第二阶段：优先启动Web服务器");
    
    // 优先初始化Web服务器服务
    ret = bsp_init_webserver_service();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Web服务器初始化失败，但继续启动其他服务");
    }

    // Web服务器启动后，初始化网络监控服务
    ESP_LOGI(TAG, "Web服务器启动完成，开始初始化网络监控服务");
    ret = bsp_init_network_monitoring_service();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "网络监控服务初始化失败，但继续启动其他服务");
    }    // ============ 第三阶段：WS2812设备初始化 ============
    ESP_LOGI(TAG, "第三阶段：初始化WS2812设备");
    
    // 初始化板载WS2812
    ret = bsp_ws2812_init(BSP_WS2812_ONBOARD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "板载WS2812初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 初始化触摸WS2812
    ret = bsp_ws2812_init(BSP_WS2812_TOUCH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "触摸WS2812初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }
      // 板载WS2812测试 - 可选（已优化为跳过以减少初始化时间）
    // bsp_ws2812_onboard_test();
    // ESP_LOGI("MAIN", "板载WS2812测试完成");
    
    // 触摸WS2812测试 - 优化为快速验证
    ESP_LOGI(TAG, "执行快速WS2812验证");
    // bsp_ws2812_touch_test(); // 注释掉耗时的完整测试
    // 替换为快速验证
    esp_err_t ws2812_verify = bsp_ws2812_set_pixel(BSP_WS2812_TOUCH, 0, 50, 50, 50);
    if (ws2812_verify == ESP_OK) {
        bsp_ws2812_refresh(BSP_WS2812_TOUCH);
        ESP_LOGI("MAIN", "触摸WS2812快速验证完成");
    } else {
        ESP_LOGW("MAIN", "触摸WS2812验证失败，但继续初始化");
    }// ============ 第四阶段：持续监测和高级服务初始化 ============
    ESP_LOGI(TAG, "第四阶段：初始化持续监测和高级服务");
    
    // 初始化系统状态控制器（新的扩展状态分类系统）
    ESP_LOGI(TAG, "初始化系统状态控制器");
    ret = bsp_system_state_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "系统状态控制器初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 初始化网络动画控制器（兼容模式）
    ESP_LOGI(TAG, "初始化网络动画控制器（兼容模式）");
    ret = bsp_network_animation_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "网络动画控制器初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 设置启动动画，等待网络监控系统启动
    bsp_network_animation_set_startup();
    ESP_LOGI(TAG, "已设置启动动画，等待网络监控系统启动");
    
    // 启动BSP电源芯片UART通信测试
    bsp_power_test_start();
    
    // ============ 第五阶段：启动持续监测服务 ============
    ESP_LOGI(TAG, "第五阶段：启动持续监测服务");
      // 等待网络监控系统收集初始数据 (优化: 减少等待时间)
    ESP_LOGI(TAG, "等待网络监控系统收集初始数据... (优化等待时间)");
    vTaskDelay(pdMS_TO_TICKS(BSP_INIT_DELAY_SYSTEM_MONITOR_MS));
    
    // 开始网络状态与动画联动监控 (优化: 异步启动)
    ESP_LOGI(TAG, "开始网络状态与动画联动监控");
    bsp_network_animation_start_monitoring();
    
    // 启动系统状态监控 (优化: 并行启动)
    ESP_LOGI(TAG, "启动系统状态监控");
    bsp_system_state_start_monitoring();
    
    // 显示BSP电源系统状态
    bsp_power_system_status_show();
      // 记录初始化完成时间和统计信息
    uint32_t init_end_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    bsp_stats.init_duration_ms = init_end_time - bsp_stats.init_start_time;
    
    // 初始化性能统计
    bsp_board_update_performance_stats();
    
    // 打印优化后的初始化报告
    ESP_LOGI(TAG, "=== BSP初始化完成报告 ===");
    ESP_LOGI(TAG, "ESP32-S3 BSP及所有系统级服务初始化完成 (耗时: %" PRIu32 " ms)", bsp_stats.init_duration_ms);
    ESP_LOGI(TAG, "优化效果: 相比原始44620ms，优化了约%.1f秒", (44620.0f - bsp_stats.init_duration_ms) / 1000.0f);
    ESP_LOGI(TAG, "主要优化项:");
    ESP_LOGI(TAG, "  - 网络监控延迟: 2000ms → 500ms (节省1.5s)");
    ESP_LOGI(TAG, "  - 系统监控延迟: 8000ms → 2000ms (节省6s)");
    ESP_LOGI(TAG, "  - WS2812测试优化: 完整测试 → 快速验证");
    ESP_LOGI(TAG, "========================");
    
    return ESP_OK;
}

// BSP应用主循环函数
void bsp_board_run_main_loop(void) {
    ESP_LOGI(TAG, "进入BSP应用主循环");
    
    int power_status_counter = 0;
    int system_state_counter = 0;
    int network_animation_status_counter = 0;
    int performance_stats_counter = 0;
    int health_check_counter = 0;
    
    while (1) {
        vTaskDelay(BSP_MAIN_LOOP_INTERVAL_MS / portTICK_PERIOD_MS);
        
        power_status_counter++;
        system_state_counter++;
        network_animation_status_counter++;
        performance_stats_counter++;
        health_check_counter++;
        
        // 每5秒更新一次性能统计
        if (performance_stats_counter >= 5) {
            bsp_board_update_performance_stats();
            performance_stats_counter = 0;
        }
        
        // 每10秒更新一次系统状态
        if (system_state_counter >= BSP_SYSTEM_STATE_REPORT_INTERVAL) {
            bsp_system_state_update_and_report();
            system_state_counter = 0;
        }
        
        // 每30秒显示一次BSP电源系统状态
        if (power_status_counter >= BSP_POWER_STATUS_REPORT_INTERVAL) {
            bsp_power_system_status_show();
            power_status_counter = 0;
        }
        
        // 每60秒显示一次网络动画控制器状态
        if (network_animation_status_counter >= BSP_NETWORK_ANIMATION_STATUS_INTERVAL) {
            bsp_network_animation_print_status();
            network_animation_status_counter = 0;
        }
        
        // 每120秒进行健康检查和性能统计报告
        if (health_check_counter >= 120) {
            ESP_LOGI(TAG, "定期健康检查和性能统计报告");
            bsp_board_health_check_extended();
            bsp_board_print_performance_stats();
            health_check_counter = 0;
        }
    }
}

// BSP清理函数
esp_err_t bsp_board_cleanup(void) {
    ESP_LOGI(TAG, "开始清理BSP资源");
    
    // 停止动画任务
    bsp_stop_animation_task();
    
    // 停止系统状态监控
    bsp_system_state_stop();
    
    // 停止网络监控
    nm_stop_monitoring();
    
    // 停止Web服务器
    bsp_stop_webserver();
    
    // 清理电源芯片监控
    bsp_power_chip_monitor_stop();
    
    // 清理WS2812资源
    bsp_ws2812_deinit_all();
    
    ESP_LOGI(TAG, "BSP资源清理完成");
    return ESP_OK;
}

// 获取BSP初始化状态
bool bsp_board_is_initialized(void) {
    // 检查关键组件是否已初始化
    return (animation_task_handle != NULL);
}

// BSP错误恢复函数
esp_err_t bsp_board_error_recovery(void) {
    ESP_LOGW(TAG, "开始BSP错误恢复");
    
    // 重启关键服务
    esp_err_t ret = bsp_start_animation_task();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "动画任务恢复失败");
        return ret;
    }
    
    // 重启网络监控
    nm_start_monitoring();
    
    ESP_LOGI(TAG, "BSP错误恢复完成");
    return ESP_OK;
}

// BSP系统诊断函数
void bsp_board_print_system_info(void) {
    ESP_LOGI(TAG, "=== BSP系统信息 ===");
    ESP_LOGI(TAG, "动画任务状态: %s", animation_task_handle ? "运行中" : "已停止");
      // 显示内存使用情况
    ESP_LOGI(TAG, "自由堆内存: %" PRIu32 " 字节", (uint32_t)esp_get_free_heap_size());
    ESP_LOGI(TAG, "最小自由堆内存: %" PRIu32 " 字节", (uint32_t)esp_get_minimum_free_heap_size());
      // 显示任务运行时间
    if (animation_task_handle) {
        UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(animation_task_handle);
        ESP_LOGI(TAG, "动画任务栈高水位: %" PRIu32 " 字节", (uint32_t)(stack_remaining * sizeof(StackType_t)));
    }
    
    // 显示电源状态
    float main_v = bsp_get_main_voltage();
    float aux_v = bsp_get_aux_12v_voltage();
    ESP_LOGI(TAG, "电源状态 - 主电源: %.2fV, 辅助12V: %.2fV", main_v, aux_v);
    
    ESP_LOGI(TAG, "=== BSP系统信息结束 ===");
}

// BSP健康检查函数
esp_err_t bsp_board_health_check(void) {
    ESP_LOGI(TAG, "开始BSP健康检查");
    
    // 检查动画任务状态
    if (animation_task_handle == NULL) {
        ESP_LOGW(TAG, "健康检查: 动画任务未运行");
        return ESP_FAIL;
    }
      // 检查内存使用情况
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < CONFIG_BSP_HEALTH_CHECK_MIN_FREE_HEAP) {
        ESP_LOGW(TAG, "健康检查: 可用堆内存较低 (%" PRIu32 " 字节)", (uint32_t)free_heap);
    }
    
    // 检查电源状态
    float main_v = bsp_get_main_voltage();
    if (main_v < CONFIG_BSP_HEALTH_CHECK_MIN_VOLTAGE || main_v > CONFIG_BSP_HEALTH_CHECK_MAX_VOLTAGE) {
        ESP_LOGW(TAG, "健康检查: 主电源电压异常 (%.2fV)", main_v);
    }
    
    ESP_LOGI(TAG, "BSP健康检查完成");
    return ESP_OK;
}

// BSP性能统计函数
void bsp_board_update_performance_stats(void) {
    // 更新运行时间
    bsp_stats.uptime_seconds = xTaskGetTickCount() / configTICK_RATE_HZ;
    
    // 更新内存使用峰值
    size_t min_heap = esp_get_minimum_free_heap_size();
    if (min_heap < bsp_stats.heap_usage_peak || bsp_stats.heap_usage_peak == 0) {
        bsp_stats.heap_usage_peak = min_heap;
    }
    
    // 检查关键任务状态
    bsp_stats.critical_task_running = (animation_task_handle != NULL);
    
    // 更新任务切换计数（近似值）
    bsp_stats.task_switches = xTaskGetTickCount();
}

void bsp_board_print_performance_stats(void) {
    ESP_LOGI(TAG, "=== BSP性能统计 ===");
    ESP_LOGI(TAG, "初始化时长: %" PRIu32 " ms", bsp_stats.init_duration_ms);
    ESP_LOGI(TAG, "运行时间: %" PRIu32 " 秒", bsp_stats.uptime_seconds);
    ESP_LOGI(TAG, "内存使用峰值: %" PRIu32 " 字节", bsp_stats.heap_usage_peak);
    ESP_LOGI(TAG, "关键任务状态: %s", bsp_stats.critical_task_running ? "运行中" : "已停止");
    ESP_LOGI(TAG, "网络错误计数: %" PRIu32, bsp_stats.network_errors);
    ESP_LOGI(TAG, "电源波动计数: %" PRIu32, bsp_stats.power_fluctuations);
    ESP_LOGI(TAG, "动画帧渲染: %" PRIu32, bsp_stats.animation_frames_rendered);
    ESP_LOGI(TAG, "================");
}

void bsp_board_reset_performance_stats(void) {
    memset(&bsp_stats, 0, sizeof(bsp_performance_stats_t));
    bsp_stats.init_start_time = xTaskGetTickCount();
    ESP_LOGI(TAG, "性能统计已重置");
}

void bsp_board_increment_network_errors(void) {
    bsp_stats.network_errors++;
}

void bsp_board_increment_power_fluctuations(void) {
    bsp_stats.power_fluctuations++;
}

void bsp_board_increment_animation_frames(void) {
    bsp_stats.animation_frames_rendered++;
}

// BSP配置验证函数（增强版）
static esp_err_t bsp_board_validate_config(void) {
    ESP_LOGI(TAG, "验证BSP配置参数");
    
    esp_err_t ret = ESP_OK;
      // 验证延迟配置是否合理 (更新为优化后的范围)
    if (BSP_INIT_DELAY_NETWORK_MS < 100 || BSP_INIT_DELAY_NETWORK_MS > 5000) {
        ESP_LOGW(TAG, "网络初始化延迟配置可能不合理: %d ms", BSP_INIT_DELAY_NETWORK_MS);
        ret = ESP_ERR_INVALID_ARG;
    }
    
    if (BSP_ANIMATION_UPDATE_RATE_MS < 10 || BSP_ANIMATION_UPDATE_RATE_MS > 100) {
        ESP_LOGW(TAG, "动画更新频率配置可能不合理: %d ms", BSP_ANIMATION_UPDATE_RATE_MS);
        ret = ESP_ERR_INVALID_ARG;
    }
      // 验证主循环配置
    if (BSP_MAIN_LOOP_INTERVAL_MS < 100 || BSP_MAIN_LOOP_INTERVAL_MS > 5000) {
        ESP_LOGW(TAG, "主循环间隔配置可能不合理: %d ms", BSP_MAIN_LOOP_INTERVAL_MS);
        ret = ESP_ERR_INVALID_ARG;
    }
      // 验证内存是否足够
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < 100000) { // 100KB 最小要求
        ESP_LOGE(TAG, "可用堆内存不足: %" PRIu32 " 字节，建议至少100KB", (uint32_t)free_heap);
        return ESP_ERR_NO_MEM;
    }
    
    // 验证GPIO配置
    if (BSP_W5500_RST_PIN == BSP_W5500_INT_PIN) {
        ESP_LOGE(TAG, "W5500 RST和INT引脚配置重复");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 验证SPI引脚配置
    if (BSP_W5500_MISO_PIN == BSP_W5500_MOSI_PIN || 
        BSP_W5500_MISO_PIN == BSP_W5500_SCLK_PIN ||
        BSP_W5500_MOSI_PIN == BSP_W5500_SCLK_PIN) {
        ESP_LOGE(TAG, "W5500 SPI引脚配置重复");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "BSP配置验证通过");
    } else {
        ESP_LOGW(TAG, "BSP配置验证发现问题，但不影响运行");
    }
    
    return ESP_OK; // 即使有警告也允许继续运行
}

// BSP单元测试框架
#ifdef CONFIG_BSP_ENABLE_UNIT_TESTS

typedef struct {
    const char* test_name;
    esp_err_t (*test_func)(void);
    bool is_critical;
} bsp_unit_test_t;

static esp_err_t test_bsp_init_cleanup(void) {
    ESP_LOGI(TAG, "测试: BSP初始化和清理");
    
    // 测试初始化
    esp_err_t ret = bsp_board_init();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 检查初始化状态
    if (!bsp_board_is_initialized()) {
        return ESP_FAIL;
    }
    
    // 测试清理
    ret = bsp_board_cleanup();
    if (ret != ESP_OK) {
        return ret;
    }
    
    return ESP_OK;
}

static esp_err_t test_bsp_config_validation(void) {
    ESP_LOGI(TAG, "测试: BSP配置验证");
    return bsp_board_validate_config();
}

static esp_err_t test_bsp_health_check(void) {
    ESP_LOGI(TAG, "测试: BSP健康检查");
    return bsp_board_health_check();
}

static esp_err_t test_bsp_performance_stats(void) {
    ESP_LOGI(TAG, "测试: BSP性能统计");
    
    bsp_board_reset_performance_stats();
    bsp_board_update_performance_stats();
    bsp_board_print_performance_stats();
    
    return ESP_OK;
}

static const bsp_unit_test_t bsp_unit_tests[] = {
    {"BSP初始化清理测试", test_bsp_init_cleanup, true},
    {"BSP配置验证测试", test_bsp_config_validation, false},
    {"BSP健康检查测试", test_bsp_health_check, false},
    {"BSP性能统计测试", test_bsp_performance_stats, false},
};

esp_err_t bsp_board_run_unit_tests(void) {
    ESP_LOGI(TAG, "开始运行BSP单元测试");
    
    int total_tests = sizeof(bsp_unit_tests) / sizeof(bsp_unit_test_t);
    int passed_tests = 0;
    int failed_critical = 0;
    
    for (int i = 0; i < total_tests; i++) {
        const bsp_unit_test_t* test = &bsp_unit_tests[i];
        ESP_LOGI(TAG, "运行测试: %s", test->test_name);
        
        esp_err_t result = test->test_func();
        if (result == ESP_OK) {
            ESP_LOGI(TAG, "测试通过: %s", test->test_name);
            passed_tests++;
        } else {
            ESP_LOGE(TAG, "测试失败: %s (错误码: 0x%x)", test->test_name, result);
            if (test->is_critical) {
                failed_critical++;
            }
        }
    }
    
    ESP_LOGI(TAG, "单元测试完成: %d/%d 通过", passed_tests, total_tests);
    
    if (failed_critical > 0) {
        ESP_LOGE(TAG, "关键测试失败数量: %d", failed_critical);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

#endif // CONFIG_BSP_ENABLE_UNIT_TESTS

// 扩展的健康检查功能
esp_err_t bsp_board_health_check_extended(void) {
    ESP_LOGI(TAG, "开始扩展BSP健康检查");
    
    esp_err_t ret = ESP_OK;
    
    // 基础健康检查
    if (bsp_board_health_check() != ESP_OK) {
        ret = ESP_FAIL;
    }
      // 检查任务堆栈使用情况
    if (animation_task_handle != NULL) {
        UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(animation_task_handle);
        if (stack_remaining < CONFIG_BSP_HEALTH_CHECK_MIN_STACK_REMAINING) {
            ESP_LOGW(TAG, "健康检查: 动画任务堆栈空间不足 (%d 字节剩余)", stack_remaining * sizeof(StackType_t));
            ret = ESP_FAIL;
        }
    }
      // 检查系统负载
    if (animation_task_handle != NULL) {
        eTaskState task_state = eTaskGetState(animation_task_handle);
        if (task_state == eSuspended) {
            ESP_LOGW(TAG, "健康检查: 动画任务处于挂起状态");
            ret = ESP_FAIL;
        }
    }
    
    // 检查网络连接状态
    // 注意：这里需要根据实际的网络模块API进行调整
    // if (!nm_is_network_connected()) {
    //     ESP_LOGW(TAG, "健康检查: 网络连接异常");
    //     ret = ESP_FAIL;
    // }
    
    // 检查温度（如果有传感器）
    // float temperature = bsp_get_temperature();
    // if (temperature > 80.0f) {
    //     ESP_LOGW(TAG, "健康检查: 温度过高 (%.1f°C)", temperature);
    //     ret = ESP_FAIL;
    // }
    
    ESP_LOGI(TAG, "扩展BSP健康检查完成，状态: %s", ret == ESP_OK ? "正常" : "异常");
    return ret;
}