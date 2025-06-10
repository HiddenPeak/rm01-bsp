/**
 * @file bsp_network_adapter.h
 * @brief BSP网络状态适配器 - 简化的网络状态输入模块
 * 
 * 替代复杂的网络动画控制器，专注于提供网络状态输入
 * 与统一状态接口集成，简化架构
 */

#ifndef BSP_NETWORK_ADAPTER_H
#define BSP_NETWORK_ADAPTER_H

#include "esp_err.h"
#include "network_monitor.h"

#ifdef __cplusplus
extern "C" {
#endif

// 网络状态变化回调类型
typedef void (*network_state_change_cb_t)(const char* ip, nm_status_t old_status, nm_status_t new_status);

/**
 * @brief 初始化BSP网络状态适配器
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_network_adapter_init(void);

/**
 * @brief 启动网络状态监控
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_network_adapter_start(void);

/**
 * @brief 停止网络状态监控
 */
void bsp_network_adapter_stop(void);

/**
 * @brief 注册网络状态变化回调
 * 
 * @param callback 回调函数
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_network_adapter_register_callback(network_state_change_cb_t callback);

/**
 * @brief 获取网络连接摘要
 * 
 * @param connected_count 连接的设备数量
 * @param total_count 总设备数量
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_network_adapter_get_summary(uint8_t* connected_count, uint8_t* total_count);

/**
 * @brief 打印网络状态摘要
 */
void bsp_network_adapter_print_status(void);

#ifdef __cplusplus
}
#endif

#endif // BSP_NETWORK_ADAPTER_H
