/**
 * @file bsp_status_interface.h
 * @brief BSP统一状态接口 - 单一入口的状态获取和控制
 * 
 * 提供统一的状态查询和控制接口，隐藏内部架构复杂性
 * 替代分散在多个模块中的状态接口，简化用户使用
 */

#ifndef BSP_STATUS_INTERFACE_H
#define BSP_STATUS_INTERFACE_H

#include "esp_err.h"
#include "bsp_state_manager.h"
#include "bsp_display_controller.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========== 统一状态数据结构 ==========

/**
 * @brief 网络连接状态详情
 */
typedef struct {
    bool computing_module_connected;     // 算力模组连接状态
    bool application_module_connected;   // 应用模组连接状态
    bool user_host_connected;            // 用户主机连接状态
    bool internet_connected;             // 互联网连接状态
    uint32_t network_check_count;        // 网络检查次数
    uint32_t last_network_change_time;   // 上次网络状态变化时间
} network_connection_status_t;

/**
 * @brief 系统性能状态详情
 */
typedef struct {
    float current_temperature;           // 当前温度 (°C)
    bool high_compute_load;              // 高计算负载状态
    float current_power_consumption;     // 当前功耗 (W)
    float cpu_usage_percent;             // CPU使用率 (%)
    float memory_usage_percent;          // 内存使用率 (%)
    uint32_t active_task_count;          // 活跃任务数量
} system_performance_status_t;

/**
 * @brief 显示控制状态详情
 */
typedef struct {
    int current_animation_index;         // 当前动画索引
    uint32_t total_animation_switches;   // 总动画切换次数
    uint32_t last_animation_switch_time; // 上次动画切换时间
    bool display_controller_active;      // 显示控制器激活状态
    bool manual_display_mode;            // 手动显示模式
    bool auto_switch_enabled;            // 自动切换启用状态
} display_control_status_t;

/**
 * @brief 统一的系统状态信息
 * 
 * 包含所有子系统的状态信息，一次查询获取完整状态
 */
typedef struct {
    // 基础状态信息
    system_state_t current_state;        // 当前系统状态
    system_state_t previous_state;       // 前一个系统状态
    uint32_t state_change_count;         // 状态变化次数
    uint32_t time_in_current_state;      // 在当前状态的时间（秒）
    uint32_t system_uptime_seconds;      // 系统运行时间（秒）
    
    // 详细状态信息
    network_connection_status_t network;    // 网络连接状态
    system_performance_status_t performance; // 系统性能状态
    display_control_status_t display;       // 显示控制状态
    
    // 元数据
    uint32_t status_timestamp;           // 状态快照时间戳
    bool status_valid;                   // 状态数据有效性
    char status_source[32];              // 状态数据来源标识
} unified_system_status_t;

// ========== 状态变化通知 ==========

/**
 * @brief 系统事件类型
 */
typedef enum {
    BSP_EVENT_STATE_CHANGED = 0,        // 系统状态变化
    BSP_EVENT_NETWORK_CHANGED,          // 网络状态变化
    BSP_EVENT_DISPLAY_CHANGED,          // 显示状态变化
    BSP_EVENT_PERFORMANCE_CHANGED,      // 性能状态变化
    BSP_EVENT_ERROR_OCCURRED,           // 错误发生
    BSP_EVENT_COUNT                     // 事件类型数量
} bsp_event_type_t;

/**
 * @brief 系统事件数据
 */
typedef struct {
    bsp_event_type_t type;              // 事件类型
    void* data;                         // 事件数据指针
    uint32_t data_size;                 // 事件数据大小
    uint32_t timestamp;                 // 事件时间戳
    char source_component[32];          // 事件源组件名称
} bsp_system_event_t;

/**
 * @brief 状态变化回调函数类型
 * 
 * @param event 系统事件信息
 * @param user_data 用户数据指针
 */
typedef void (*bsp_status_change_callback_t)(const bsp_system_event_t* event, void* user_data);

// ========== 状态缓存配置 ==========

/**
 * @brief 状态缓存配置
 */
typedef struct {
    uint32_t cache_ttl_ms;              // 缓存生存时间（毫秒）
    bool enable_auto_refresh;           // 启用自动刷新
    uint32_t auto_refresh_interval_ms;  // 自动刷新间隔（毫秒）
} status_cache_config_t;

/**
 * @brief 状态监听配置
 */
typedef struct {
    bsp_event_type_t event_mask;        // 监听的事件类型掩码
    uint32_t min_change_interval_ms;    // 最小变化间隔（防抖）
    float numeric_change_threshold;     // 数值变化阈值（百分比）
    bool batch_events;                  // 是否批量处理事件
} status_watch_config_t;

// ========== 核心接口 ==========

/**
 * @brief 初始化BSP统一状态接口
 * 
 * 初始化所有相关子系统并建立统一接口
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_status_interface_init(void);

/**
 * @brief 启动BSP状态监控服务
 * 
 * 启动所有状态监控任务和事件处理
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_status_interface_start(void);

/**
 * @brief 停止BSP状态监控服务
 */
void bsp_status_interface_stop(void);

/**
 * @brief 获取统一的系统状态（实时）
 * 
 * 立即查询所有子系统状态并返回完整信息
 * 
 * @param status 状态信息结构指针
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_get_system_status(unified_system_status_t* status);

/**
 * @brief 获取缓存的系统状态（快速）
 * 
 * 返回缓存的状态信息，如果缓存过期则自动刷新
 * 
 * @param status 状态信息结构指针
 * @param max_age_ms 可接受的最大缓存年龄（毫秒）
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_get_system_status_cached(unified_system_status_t* status, uint32_t max_age_ms);

/**
 * @brief 异步请求状态更新
 * 
 * 请求异步更新系统状态，完成后通过回调通知
 * 
 * @param callback 完成回调函数
 * @param user_data 用户数据
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_request_status_update_async(bsp_status_change_callback_t callback, void* user_data);

// ========== 状态控制接口 ==========

/**
 * @brief 设置显示模式
 * 
 * @param manual_mode true为手动模式，false为自动模式
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_set_display_mode(bool manual_mode);

/**
 * @brief 手动设置动画
 * 
 * 仅在手动模式下有效
 * 
 * @param animation_index 动画索引
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_set_animation(int animation_index);

/**
 * @brief 强制刷新系统状态
 * 
 * 立即重新检测所有状态并更新缓存
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_force_status_refresh(void);

// ========== 事件订阅接口 ==========

/**
 * @brief 注册状态变化监听器
 * 
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_register_status_listener(bsp_status_change_callback_t callback, void* user_data);

/**
 * @brief 注册条件状态监听器
 * 
 * 只有满足条件的状态变化才会触发回调
 * 
 * @param config 监听配置
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_register_conditional_listener(const status_watch_config_t* config, 
                                           bsp_status_change_callback_t callback, 
                                           void* user_data);

/**
 * @brief 注销状态变化监听器
 * 
 * @param callback 要注销的回调函数
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_unregister_status_listener(bsp_status_change_callback_t callback);

// ========== 缓存和性能配置 ==========

/**
 * @brief 配置状态缓存
 * 
 * @param config 缓存配置
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_configure_status_cache(const status_cache_config_t* config);

/**
 * @brief 清除状态缓存
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_clear_status_cache(void);

// ========== 调试和监控接口 ==========

/**
 * @brief 打印完整的系统状态报告
 */
void bsp_print_system_status_report(void);

/**
 * @brief 打印状态接口统计信息
 */
void bsp_print_status_interface_stats(void);

/**
 * @brief 获取状态接口性能统计
 * 
 * @param total_queries 总查询次数
 * @param cache_hits 缓存命中次数
 * @param avg_query_time_us 平均查询时间（微秒）
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_get_status_interface_stats(uint32_t* total_queries, 
                                        uint32_t* cache_hits, 
                                        uint32_t* avg_query_time_us);

// ========== 便利接口 ==========

/**
 * @brief 获取默认状态缓存配置
 * 
 * @return status_cache_config_t 默认配置
 */
status_cache_config_t bsp_get_default_cache_config(void);

/**
 * @brief 获取默认状态监听配置
 * 
 * @return status_watch_config_t 默认配置
 */
status_watch_config_t bsp_get_default_watch_config(void);

/**
 * @brief 检查系统是否处于健康状态
 * 
 * 快速检查关键系统状态
 * 
 * @return bool true表示健康，false表示存在问题
 */
bool bsp_is_system_healthy(void);

/**
 * @brief 获取系统状态摘要
 * 
 * 返回简化的状态信息字符串，适用于日志或显示
 * 
 * @param buffer 缓冲区
 * @param buffer_size 缓冲区大小
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_get_status_summary(char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // BSP_STATUS_INTERFACE_H
