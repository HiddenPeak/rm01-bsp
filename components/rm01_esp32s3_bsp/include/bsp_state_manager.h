/**
 * @file bsp_state_manager.h
 * @brief BSP系统状态管理器 - 纯状态检测与管理
 * 
 * 专注于系统状态的检测、分析和管理，不涉及显示逻辑
 * 提供状态变化通知机制，供显示控制器使用
 */

#ifndef BSP_STATE_MANAGER_H
#define BSP_STATE_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========== 系统配置常量 ==========

// 温度阈值定义
#define TEMP_THRESHOLD_HIGH_1    85.0f  // 高温状态1阈值（85°C）
#define TEMP_THRESHOLD_HIGH_2    95.0f  // 高温状态2阈值（95°C）
#define TEMP_THRESHOLD_NORMAL    80.0f  // 恢复正常状态阈值（80°C，有5°C滞回）

// 高计算负载判断阈值
#define COMPUTE_CPU_THRESHOLD       80.0f   // CPU使用率阈值 (%)
#define COMPUTE_MEMORY_THRESHOLD    85.0f   // 内存使用率阈值 (%)
#define COMPUTE_POWER_THRESHOLD     50.0f   // 功耗阈值 (W)
#define COMPUTE_TASK_THRESHOLD      10      // 活跃任务数阈值

// 网络模组API超时设置
#define MODULE_API_TIMEOUT_MS       5000    // API请求超时时间 (ms)
#define MODULE_API_RETRY_COUNT      3       // API请求重试次数
#define MODULE_API_CACHE_DURATION   30      // 数据缓存时间 (秒)

// 系统状态枚举
typedef enum {
    SYSTEM_STATE_STANDBY = 0,           // 待机状态（默认）
    SYSTEM_STATE_STARTUP_0,             // 启动状态0（所有模组断开）
    SYSTEM_STATE_STARTUP_1,             // 启动状态1（仅算力模组连接）
    SYSTEM_STATE_STARTUP_2,             // 启动状态2（仅应用模组连接）
    SYSTEM_STATE_STARTUP_3,             // 启动状态3（算力+应用模组连接）
    SYSTEM_STATE_HIGH_TEMP_1,           // 高温状态1（85-95°C）
    SYSTEM_STATE_HIGH_TEMP_2,           // 高温状态2（>95°C）
    SYSTEM_STATE_USER_HOST_DISCONNECTED, // 用户主机未连接
    SYSTEM_STATE_HIGH_COMPUTE_LOAD,     // 高负荷计算状态
    SYSTEM_STATE_GPU_HIGH_USAGE,        // GPU高使用率状态
    SYSTEM_STATE_MEMORY_HIGH_USAGE,     // 内存高使用率状态
    SYSTEM_STATE_COUNT                  // 状态总数
} system_state_t;

// 系统状态信息结构
typedef struct {
    system_state_t current_state;       // 当前状态
    system_state_t previous_state;      // 前一个状态
    uint32_t state_change_count;        // 状态变化次数
    uint32_t time_in_current_state;     // 在当前状态的时间（秒）
    float current_temperature;          // 当前温度
    bool computing_module_connected;    // 算力模组连接状态
    bool application_module_connected;  // 应用模组连接状态
    bool user_host_connected;           // 用户主机连接状态
    bool high_compute_load;             // 高负荷计算状态
} system_state_info_t;

// 状态变化回调函数类型
typedef void (*state_change_callback_t)(system_state_t old_state, system_state_t new_state, void* user_data);

// ========== 核心接口 ==========

/**
 * @brief 初始化系统状态管理器
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_state_manager_init(void);

/**
 * @brief 开始系统状态监控
 * 
 * 启动状态检测任务，定期检测系统状态变化
 */
void bsp_state_manager_start_monitoring(void);

/**
 * @brief 停止系统状态监控
 */
void bsp_state_manager_stop_monitoring(void);

/**
 * @brief 获取当前系统状态
 * 
 * @return system_state_t 当前系统状态
 */
system_state_t bsp_state_manager_get_current_state(void);

/**
 * @brief 获取系统状态名称
 * 
 * @param state 状态枚举值
 * @return const char* 状态名称字符串
 */
const char* bsp_state_manager_get_state_name(system_state_t state);

/**
 * @brief 获取详细的系统状态信息
 * 
 * @param info 状态信息结构指针
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_state_manager_get_info(system_state_info_t* info);

/**
 * @brief 强制设置系统状态（用于测试）
 * 
 * @param state 要设置的状态
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_state_manager_force_set_state(system_state_t state);

/**
 * @brief 手动触发状态检测和更新
 * 
 * 立即检测当前系统状态并更新，不等待定时器
 */
void bsp_state_manager_update_now(void);

/**
 * @brief 打印系统状态统计信息
 */
void bsp_state_manager_print_status(void);

// ========== 回调管理接口 ==========

/**
 * @brief 注册状态变化回调函数
 * 
 * @param callback 回调函数指针
 * @param user_data 用户数据指针
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_state_manager_register_callback(state_change_callback_t callback, void* user_data);

/**
 * @brief 注销状态变化回调函数
 * 
 * @param callback 要注销的回调函数指针
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_state_manager_unregister_callback(state_change_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // BSP_STATE_MANAGER_H
