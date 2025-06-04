#ifndef NETWORK_ANIMATION_CONTROLLER_H
#define NETWORK_ANIMATION_CONTROLLER_H

#include "esp_err.h"
#include "network_monitor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 网络状态与动画联动控制器
 * 
 * 此模块负责监控网络状态变化并自动切换LED动画：
 * - 当用户主机(10.10.99.100)连接时：显示"示例动画"
 * - 当用户主机断开连接时：显示"链接错误"动画 
 * - 系统启动阶段：显示"启动中"动画
 */

// 动画索引定义（与JSON文件中的动画顺序对应）
#define ANIMATION_INDEX_DEMO        0  // 示例动画
#define ANIMATION_INDEX_STARTUP     1  // 启动中 
#define ANIMATION_INDEX_ERROR       2  // 链接错误

/**
 * @brief 初始化网络动画控制器
 * 
 * 注册网络状态变化回调，设置初始动画状态
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t network_animation_controller_init(void);

/**
 * @brief 网络状态变化回调函数
 * 
 * 当网络状态发生变化时被网络监控系统调用
 * 
 * @param index 目标索引
 * @param ip 目标IP地址
 * @param status 新的网络状态
 * @param arg 用户参数（未使用）
 */
void network_status_change_callback(uint8_t index, const char* ip, int status, void* arg);

/**
 * @brief 手动设置启动动画
 * 
 * 在系统启动时显示启动动画，等待网络监控系统开始工作
 */
void network_animation_set_startup(void);

/**
 * @brief 开始网络状态监控
 * 
 * 启动网络监控并开始根据状态切换动画
 */
void network_animation_start_monitoring(void);

/**
 * @brief 获取当前网络动画状态信息
 * 
 * 用于调试和状态查询
 */
void network_animation_print_status(void);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_ANIMATION_CONTROLLER_H
