/**
 * @file system_state_controller.h
 * @brief 系统状态控制器 - 扩展状态分类系统
 * 
 * 支持基于多种条件的复杂状态联动：
 * - 模组启动状态（基于网络连接状态组合）
 * - 高温状态（基于电源芯片温度读取）
 * - 用户主机连接状态
 * - 高负荷计算状态（基于算力模块状态读取）
 * - 待机状态（默认状态）
 */

#ifndef SYSTEM_STATE_CONTROLLER_H
#define SYSTEM_STATE_CONTROLLER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include "network_module_api.h"  // 包含网络模组API定义

#ifdef __cplusplus
extern "C" {
#endif

// ========== 系统配置常量 ==========

// 温度阈值定义
#define TEMP_THRESHOLD_HIGH_1    85.0f  // 高温状态1阈值（85°C）
#define TEMP_THRESHOLD_HIGH_2    95.0f  // 高温状态2阈值（95°C）
#define TEMP_THRESHOLD_NORMAL    80.0f  // 恢复正常状态阈值（80°C，有5°C滞回）

// ========== TODO: 需要等待设备API启用后调整的配置 ==========
// 高计算负载判断阈值
#define COMPUTE_CPU_THRESHOLD       80.0f   // CPU使用率阈值 (%)
#define COMPUTE_MEMORY_THRESHOLD    85.0f   // 内存使用率阈值 (%)
#define COMPUTE_POWER_THRESHOLD     50.0f   // 功耗阈值 (W)
#define COMPUTE_TASK_THRESHOLD      10      // 活跃任务数阈值

// 网络模组API超时设置
#define MODULE_API_TIMEOUT_MS       5000    // API请求超时时间 (ms)
#define MODULE_API_RETRY_COUNT      3       // API请求重试次数
#define MODULE_API_CACHE_DURATION   30      // 数据缓存时间 (秒)
// ============================================================

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
    SYSTEM_STATE_COUNT                  // 状态总数
} system_state_t;

// 动画索引映射（对应JSON文件中的动画）
typedef enum {
    ANIM_EXAMPLE = 0,       // 示例动画（待机状态）
    ANIM_STARTUP,           // 启动中动画
    ANIM_LINK_ERROR,        // 链接错误动画
    ANIM_HIGH_TEMP,         // 高温警告动画（已在example_animation.json中定义）
    ANIM_COMPUTING          // 计算中动画（已在example_animation.json中定义）
} system_animation_index_t;

// 系统状态控制器初始化
esp_err_t system_state_controller_init(void);

// 开始系统状态监控
void system_state_controller_start_monitoring(void);

// 停止系统状态监控
void system_state_controller_stop(void);

// 更新状态并生成报告
void system_state_controller_update_and_report(void);

// 获取当前系统状态
system_state_t system_state_get_current(void);

// 获取系统状态名称
const char* system_state_get_name(system_state_t state);

// 强制设置系统状态（用于测试）
esp_err_t system_state_force_set(system_state_t state);

// 打印系统状态信息
void system_state_print_status(void);

// 获取系统状态统计信息
typedef struct {
    system_state_t current_state;       // 当前状态
    system_state_t previous_state;      // 前一个状态
    uint32_t state_change_count;        // 状态变化次数
    uint32_t time_in_current_state;     // 在当前状态的时间（秒）
    float current_temperature;          // 当前温度
    bool computing_module_connected;    // 算力模组连接状态
    bool application_module_connected;  // 应用模组连接状态
    bool user_host_connected;           // 用户主机连接状态
    bool high_compute_load;             // 高负荷计算标志
} system_state_info_t;

// 获取系统状态信息
esp_err_t system_state_get_info(system_state_info_t* info);

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_STATE_CONTROLLER_H
