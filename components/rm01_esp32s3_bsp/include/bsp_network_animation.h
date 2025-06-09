/**
 * @file bsp_network_animation.h
 * @brief BSP网络状态动画控制器
 * 
 * 此模块是BSP初始化服务的一部分，负责：
 * - 设备启动时的状态指示
 * - 网络连接状态的可视化反馈
 * - 系统运行状态的LED指示
 */

#ifndef BSP_NETWORK_ANIMATION_H
#define BSP_NETWORK_ANIMATION_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BSP网络状态动画控制器
 * 
 * 作为BSP服务的一部分，提供：
 * - 设备启动状态指示
 * - 网络连接状态可视化
 * - 系统运行状态反馈
 */

// BSP动画索引定义（与LED动画系统集成）
#define BSP_ANIMATION_INDEX_DEMO        0  // 正常运行指示
#define BSP_ANIMATION_INDEX_STARTUP     1  // 启动中指示
#define BSP_ANIMATION_INDEX_ERROR       2  // 连接错误指示

/**
 * @brief 初始化BSP网络动画控制器
 * 
 * 作为BSP初始化序列的一部分被调用
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_network_animation_init(void);

/**
 * @brief 启动BSP网络状态监控
 * 
 * 开始监控网络状态并自动更新LED指示
 */
void bsp_network_animation_start_monitoring(void);

/**
 * @brief 设置设备启动状态指示
 * 
 * 在设备启动过程中显示启动指示动画
 */
void bsp_network_animation_set_startup(void);

/**
 * @brief 获取BSP网络动画状态
 * 
 * 用于系统状态查询和调试
 */
void bsp_network_animation_print_status(void);

/**
 * @brief BSP网络状态变化回调函数
 * 
 * 内部使用，当网络状态发生变化时被网络监控系统调用
 * 
 * @param index 目标索引
 * @param ip 目标IP地址
 * @param status 新的网络状态
 * @param arg 用户参数（未使用）
 */
void bsp_network_status_change_callback(uint8_t index, const char* ip, int status, void* arg);

#ifdef __cplusplus
}
#endif

#endif // BSP_NETWORK_ANIMATION_H
