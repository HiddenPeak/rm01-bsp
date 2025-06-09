/**
 * @file bsp_network_animation.c
 * @brief BSP网络状态动画控制器实现
 * 
 * 此模块是BSP初始化服务的一部分，提供设备状态可视化反馈
 */

#include "bsp_network_animation.h"
#include "led_animation.h"
#include "led_animation_loader.h"
#include "network_monitor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "BSP_NET_ANIM";

// BSP网络动画控制器状态
static bool bsp_controller_initialized = false;
static bool bsp_monitoring_started = false;
static int current_bsp_animation = BSP_ANIMATION_INDEX_STARTUP;

/**
 * @brief 初始化BSP网络动画控制器
 */
esp_err_t bsp_network_animation_init(void) {
    ESP_LOGI(TAG, "初始化BSP网络状态动画控制器");
    
    // 检查LED动画系统是否有足够的动画
    int animation_count = led_animation_get_count();
    ESP_LOGI(TAG, "当前加载的动画数量: %d", animation_count);
    
    if (animation_count < 3) {
        ESP_LOGW(TAG, "动画数量不足，需要至少3个动画（正常运行、启动中、连接错误）");
        ESP_LOGW(TAG, "当前动画数量: %d，将继续运行但功能可能受限", animation_count);
    }
    
    // 打印所有可用动画
    for (int i = 0; i < animation_count; i++) {
        const char* name = led_animation_get_name(i);
        ESP_LOGI(TAG, "BSP动画 %d: %s", i, name ? name : "未命名");
    }
    
    // 注册BSP网络状态变化回调
    nm_register_status_change_callback(bsp_network_status_change_callback, NULL);
    ESP_LOGI(TAG, "已注册BSP网络状态变化回调");
    
    bsp_controller_initialized = true;
    ESP_LOGI(TAG, "BSP网络状态动画控制器初始化完成");
    return ESP_OK;
}

/**
 * @brief BSP网络状态变化回调函数
 */
void bsp_network_status_change_callback(uint8_t index, const char* ip, int status, void* arg) {
    // 检查是否是用户主机(10.10.99.100)的状态变化
    if (strcmp(ip, NM_USER_HOST_IP) != 0) {
        return; // 只关心用户主机的状态
    }
    
    ESP_LOGI(TAG, "BSP检测到用户主机(%s)网络状态变化: %s", ip, 
             (status == NM_STATUS_UP) ? "连接" : 
             (status == NM_STATUS_DOWN) ? "断开" : "未知");
    
    // 根据网络状态切换BSP状态指示动画
    int target_animation = -1;
    
    if (status == NM_STATUS_UP) {
        // 用户主机连接 - 显示正常运行指示
        target_animation = BSP_ANIMATION_INDEX_DEMO;
        ESP_LOGI(TAG, "用户主机已连接，切换到正常运行指示");
    } else if (status == NM_STATUS_DOWN) {
        // 用户主机断开 - 显示连接错误指示
        target_animation = BSP_ANIMATION_INDEX_ERROR;
        ESP_LOGI(TAG, "用户主机已断开，切换到连接错误指示");
    }
    
    // 执行BSP状态指示动画切换
    if (target_animation >= 0) {
        esp_err_t result = led_animation_select(target_animation);
        if (result == ESP_OK) {
            current_bsp_animation = target_animation;
            ESP_LOGI(TAG, "BSP成功切换到状态指示动画索引: %d", target_animation);
        } else {
            ESP_LOGE(TAG, "BSP切换状态指示动画失败，索引: %d, 错误: %s", 
                     target_animation, esp_err_to_name(result));
        }
    }
}

/**
 * @brief 设置设备启动状态指示
 */
void bsp_network_animation_set_startup(void) {
    ESP_LOGI(TAG, "设置BSP设备启动状态指示");
    
    esp_err_t result = led_animation_select(BSP_ANIMATION_INDEX_STARTUP);
    if (result == ESP_OK) {
        current_bsp_animation = BSP_ANIMATION_INDEX_STARTUP;
        ESP_LOGI(TAG, "BSP成功切换到设备启动指示");
    } else {
        ESP_LOGE(TAG, "BSP切换到设备启动指示失败: %s", esp_err_to_name(result));
    }
}

/**
 * @brief 启动BSP网络状态监控
 */
void bsp_network_animation_start_monitoring(void) {
    if (!bsp_controller_initialized) {
        ESP_LOGE(TAG, "BSP控制器未初始化，无法开始监控");
        return;
    }
    
    ESP_LOGI(TAG, "启动BSP网络状态监控");
    
    // 初始检查用户主机状态
    nm_status_t initial_status = nm_get_status(NM_USER_HOST_IP);
    ESP_LOGI(TAG, "用户主机(%s)初始状态: %s", NM_USER_HOST_IP, 
             (initial_status == NM_STATUS_UP) ? "连接" : 
             (initial_status == NM_STATUS_DOWN) ? "断开" : "未知");
    
    // 根据初始状态设置BSP状态指示
    if (initial_status == NM_STATUS_UP) {
        led_animation_select(BSP_ANIMATION_INDEX_DEMO);
        current_bsp_animation = BSP_ANIMATION_INDEX_DEMO;
        ESP_LOGI(TAG, "用户主机已连接，显示正常运行指示");
    } else if (initial_status == NM_STATUS_DOWN) {
        led_animation_select(BSP_ANIMATION_INDEX_ERROR);
        current_bsp_animation = BSP_ANIMATION_INDEX_ERROR;
        ESP_LOGI(TAG, "用户主机未连接，显示连接错误指示");
    } else {
        // 状态未知，保持启动指示
        ESP_LOGI(TAG, "用户主机状态未知，保持设备启动指示");
    }
    
    bsp_monitoring_started = true;
    ESP_LOGI(TAG, "BSP网络状态监控已开始");
}

/**
 * @brief 获取BSP网络动画状态
 */
void bsp_network_animation_print_status(void) {
    ESP_LOGI(TAG, "=== BSP网络状态动画控制器状态 ===");
    ESP_LOGI(TAG, "BSP控制器已初始化: %s", bsp_controller_initialized ? "是" : "否");
    ESP_LOGI(TAG, "BSP监控已开始: %s", bsp_monitoring_started ? "是" : "否");
    
    // 当前BSP状态指示动画信息
    const char* current_anim_name = led_animation_get_name(current_bsp_animation);
    ESP_LOGI(TAG, "当前BSP状态指示: %d (%s)", current_bsp_animation, 
             current_anim_name ? current_anim_name : "未知");
    
    // 实际LED动画系统状态
    int current_led_anim = led_animation_get_current_index();
    const char* current_led_name = led_animation_get_name(current_led_anim);
    ESP_LOGI(TAG, "实际LED动画: %d (%s)", current_led_anim,
             current_led_name ? current_led_name : "未知");
    
    // 用户主机网络状态
    nm_status_t user_host_status = nm_get_status(NM_USER_HOST_IP);
    ESP_LOGI(TAG, "用户主机(%s)状态: %s", NM_USER_HOST_IP,
             (user_host_status == NM_STATUS_UP) ? "连接" : 
             (user_host_status == NM_STATUS_DOWN) ? "断开" : "未知");
             
    // 获取用户主机详细信息
    nm_target_t target_info;
    if (nm_get_target_info(NM_USER_HOST_IP, &target_info)) {
        ESP_LOGI(TAG, "用户主机详细信息:");
        ESP_LOGI(TAG, "  响应时间: %" PRIu32 " ms", target_info.last_response_time);
        ESP_LOGI(TAG, "  平均响应时间: %" PRIu32 " ms", target_info.average_response_time);
        ESP_LOGI(TAG, "  丢包率: %.1f%%", target_info.loss_rate);
        ESP_LOGI(TAG, "  发送包数: %" PRIu32, target_info.packets_sent);
        ESP_LOGI(TAG, "  接收包数: %" PRIu32, target_info.packets_received);
    }
    
    ESP_LOGI(TAG, "===============================");
}
