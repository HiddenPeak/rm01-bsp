/**
 * @file bsp_network_adapter.c
 * @brief BSP网络状态适配器实现 - 简化的网络状态输入模块
 * 
 * 替代复杂的网络动画控制器，专注于提供网络状态输入
 * 与统一状态接口集成，简化架构
 */

#include "bsp_network_adapter.h"
#include "bsp_status_interface.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "BSP_NET_ADAPTER";

// 网络适配器状态
typedef struct {
    bool initialized;
    bool monitoring_active;
    network_state_change_cb_t status_callback;
    
    // 状态缓存
    struct {
        nm_status_t computing_status;
        nm_status_t application_status;
        nm_status_t user_host_status;
        nm_status_t internet_status;
    } last_status;
} bsp_network_adapter_t;

static bsp_network_adapter_t s_adapter = {0};

// 内部回调函数
static void internal_network_callback(uint8_t index, const char* ip, int status, void* arg);

esp_err_t bsp_network_adapter_init(void) {
    ESP_LOGI(TAG, "初始化BSP网络状态适配器");
    
    if (s_adapter.initialized) {
        ESP_LOGW(TAG, "网络适配器已初始化");
        return ESP_OK;
    }
    
    // 初始化状态缓存
    s_adapter.last_status.computing_status = NM_STATUS_UNKNOWN;
    s_adapter.last_status.application_status = NM_STATUS_UNKNOWN;
    s_adapter.last_status.user_host_status = NM_STATUS_UNKNOWN;
    s_adapter.last_status.internet_status = NM_STATUS_UNKNOWN;
    
    s_adapter.initialized = true;
    
    ESP_LOGI(TAG, "BSP网络状态适配器初始化完成");
    return ESP_OK;
}

esp_err_t bsp_network_adapter_start(void) {
    if (!s_adapter.initialized) {
        ESP_LOGE(TAG, "网络适配器未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "启动BSP网络状态监控");
    
    // 注册网络监控回调
    nm_register_status_change_callback(internal_network_callback, NULL);
    
    s_adapter.monitoring_active = true;
    
    ESP_LOGI(TAG, "BSP网络状态监控已启动");
    return ESP_OK;
}

void bsp_network_adapter_stop(void) {
    if (!s_adapter.monitoring_active) {
        ESP_LOGW(TAG, "网络状态监控未运行");
        return;
    }
    
    ESP_LOGI(TAG, "停止BSP网络状态监控");
    
    // 注销网络监控回调
    nm_register_status_change_callback(NULL, NULL);
    
    s_adapter.monitoring_active = false;
    
    ESP_LOGI(TAG, "BSP网络状态监控已停止");
}

esp_err_t bsp_network_adapter_register_callback(network_state_change_cb_t callback) {
    s_adapter.status_callback = callback;
    ESP_LOGI(TAG, "已注册网络状态变化回调");
    return ESP_OK;
}

esp_err_t bsp_network_adapter_get_summary(uint8_t* connected_count, uint8_t* total_count) {
    if (connected_count == NULL || total_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *total_count = 4;  // 算力模组、应用模组、用户主机、互联网
    *connected_count = 0;
    
    if (nm_get_status(NM_COMPUTING_MODULE_IP) == NM_STATUS_UP) (*connected_count)++;
    if (nm_get_status(NM_APPLICATION_MODULE_IP) == NM_STATUS_UP) (*connected_count)++;
    if (nm_get_status(NM_USER_HOST_IP) == NM_STATUS_UP) (*connected_count)++;
    if (nm_get_status(NM_INTERNET_IP) == NM_STATUS_UP) (*connected_count)++;
    
    return ESP_OK;
}

void bsp_network_adapter_print_status(void) {
    uint8_t connected, total;
    esp_err_t ret = bsp_network_adapter_get_summary(&connected, &total);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取网络状态摘要失败");
        return;
    }
    
    ESP_LOGI(TAG, "=== BSP网络状态适配器 ===");
    ESP_LOGI(TAG, "监控状态: %s", s_adapter.monitoring_active ? "运行中" : "已停止");
    ESP_LOGI(TAG, "网络连接: %d/%d", connected, total);
    
    const char* ip_list[] = {NM_COMPUTING_MODULE_IP, NM_APPLICATION_MODULE_IP, NM_USER_HOST_IP, NM_INTERNET_IP};
    const char* name_list[] = {"算力模组", "应用模组", "用户主机", "互联网"};
    
    for (int i = 0; i < 4; i++) {
        nm_status_t status = nm_get_status(ip_list[i]);
        ESP_LOGI(TAG, "  %s(%s): %s", name_list[i], ip_list[i], 
                 status == NM_STATUS_UP ? "连接" : 
                 status == NM_STATUS_DOWN ? "断开" : "未知");
    }
    
    ESP_LOGI(TAG, "========================");
}

// 内部回调函数实现
static void internal_network_callback(uint8_t index, const char* ip, int status, void* arg) {
    ESP_LOGI(TAG, "网络状态变化: %s -> %s", ip, 
             (status == NM_STATUS_UP) ? "连接" : 
             (status == NM_STATUS_DOWN) ? "断开" : "未知");
    
    // 确定是哪个网络目标
    nm_status_t old_status = NM_STATUS_UNKNOWN;
    nm_status_t new_status = (nm_status_t)status;
    
    if (strcmp(ip, NM_COMPUTING_MODULE_IP) == 0) {
        old_status = s_adapter.last_status.computing_status;
        s_adapter.last_status.computing_status = new_status;
    } else if (strcmp(ip, NM_APPLICATION_MODULE_IP) == 0) {
        old_status = s_adapter.last_status.application_status;
        s_adapter.last_status.application_status = new_status;
    } else if (strcmp(ip, NM_USER_HOST_IP) == 0) {
        old_status = s_adapter.last_status.user_host_status;
        s_adapter.last_status.user_host_status = new_status;
    } else if (strcmp(ip, NM_INTERNET_IP) == 0) {
        old_status = s_adapter.last_status.internet_status;
        s_adapter.last_status.internet_status = new_status;
    }
    
    // 调用用户回调
    if (s_adapter.status_callback != NULL) {
        s_adapter.status_callback(ip, old_status, new_status);
    }
    
    // 通知统一状态接口（如果已初始化）
    // 这里可以发布网络变化事件到统一状态接口
}
