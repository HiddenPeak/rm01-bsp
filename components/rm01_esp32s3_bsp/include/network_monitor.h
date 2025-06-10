#ifndef NETWORK_MONITOR_H
#define NETWORK_MONITOR_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "lwip/ip_addr.h"
#include "esp_log.h"

// 确保esp_ping_handle_t被正确定义为void*类型
#ifndef ESP_PING_HANDLE_T_DEFINED
#define ESP_PING_HANDLE_T_DEFINED
typedef void* esp_ping_handle_t;
#endif

// 定义网络监控的目标IP地址
#define NM_TARGET_COUNT 4
#define NM_COMPUTING_MODULE_IP "10.10.99.98"   // 算力模块
#define NM_APPLICATION_MODULE_IP "10.10.99.99" // 应用模块
#define NM_USER_HOST_IP "10.10.99.100"        // 用户主机
#define NM_INTERNET_IP "8.8.8.8"              // 外网访问测试

// 网络状态事件位
#define NM_EVENT_COMPUTING_UP     (1 << 0)
#define NM_EVENT_COMPUTING_DOWN   (1 << 1)
#define NM_EVENT_APPLICATION_UP   (1 << 2)
#define NM_EVENT_APPLICATION_DOWN (1 << 3)
#define NM_EVENT_USER_HOST_UP     (1 << 4)
#define NM_EVENT_USER_HOST_DOWN   (1 << 5)
#define NM_EVENT_INTERNET_UP      (1 << 6)
#define NM_EVENT_INTERNET_DOWN    (1 << 7)

// 网络状态变化回调函数类型
typedef void (*nm_status_change_cb_t)(uint8_t index, const char* ip, int status, void* arg);

// 网络状态枚举
typedef enum {
    NM_STATUS_UNKNOWN = 0,  // 未知状态
    NM_STATUS_UP,           // 网络连通
    NM_STATUS_DOWN,         // 网络断开
} nm_status_t;

// 网络监控目标结构体
typedef struct {
    char ip[16];            // IP地址字符串
    char name[32];          // 目标名称
    ip_addr_t addr;         // IP地址结构
    nm_status_t status;     // 当前状态
    nm_status_t prev_status; // 上一次的状态
    uint32_t last_response_time; // 最后一次响应时间(ms)
    uint32_t average_response_time; // 平均响应时间(ms)
    uint32_t packets_sent;   // 发送的包数
    uint32_t packets_received; // 收到的包数
    float loss_rate;         // 丢包率(%)
    esp_ping_handle_t ping_handle; // ping会话句柄
    uint8_t index;           // 目标索引，用于回调函数识别
} nm_target_t;

// BSP兼容层 - 网络监控适配器接口
typedef enum {
    NETWORK_STATUS_UNKNOWN = 0,  // 未知状态
    NETWORK_STATUS_UP,           // 网络连通
    NETWORK_STATUS_DOWN,         // 网络断开
} network_status_t;

typedef struct {
    char ip[16];            // IP地址字符串
    char name[32];          // 目标名称
    network_status_t status;     // 当前状态
    network_status_t prev_status; // 上一次的状态
    uint32_t last_response_time; // 最后一次响应时间(ms)
    uint32_t average_response_time; // 平均响应时间(ms)
    uint32_t packets_sent;   // 发送的包数
    uint32_t packets_received; // 收到的包数
    float loss_rate;         // 丢包率(%)
    void *ping_handle; // ping会话句柄，类型改为void*以避免包含过多头文件
    uint8_t index;           // 目标索引，用于回调函数识别
} network_target_t;

#define NETWORK_TARGET_COUNT 4

// 初始化网络监控
void nm_init(void);

// 开始监控
void nm_start_monitoring(void);

// 停止监控
void nm_stop_monitoring(void);

// 查询指定IP的状态
nm_status_t nm_get_status(const char* ip);

// 获取指定IP的详细信息
bool nm_get_target_info(const char* ip, nm_target_t* target_info);

// 查询所有监控目标的状态
void nm_get_all_status(void);

// 输出当前所有IP的状态信息
void nm_print_status_all(void);

// 注册网络状态变化回调
void nm_register_status_change_callback(nm_status_change_cb_t callback, void* arg);

// BSP兼容接口函数
const network_target_t* nm_get_network_targets(void);
void nm_start_network_monitor(void);
void nm_stop_network_monitor(void);
void nm_get_network_status(void);

// 获取网络事件组句柄，可用于等待特定网络事件
EventGroupHandle_t nm_get_event_group(void);

// 无锁状态查询接口（高性能）
nm_status_t nm_get_status_lockfree(const char* ip);
const nm_target_t* nm_get_targets_readonly(void);  // 只读访问，避免拷贝

// 高级监控配置
typedef struct {
    uint32_t ping_timeout_ms;           // ping超时时间
    uint32_t max_concurrent_pings;      // 最大并发ping数
    uint32_t result_queue_size;         // 结果队列大小
    bool enable_prediction;             // 启用预测性监控
    bool enable_smart_scheduling;       // 启用智能调度
} nm_advanced_config_t;

// 性能统计接口
typedef struct {
    uint32_t total_pings;
    uint32_t successful_pings; 
    uint32_t failed_pings;
    uint32_t avg_response_time;
    uint32_t monitoring_cycles;
    uint32_t state_changes;
} nm_performance_stats_t;

// 性能指标接口
typedef struct {
    uint32_t avg_ping_time;             // 平均ping时间
    uint32_t lock_contention_count;     // 锁竞争次数
    uint32_t queue_overflow_count;      // 队列溢出次数
    uint32_t concurrent_ping_count;     // 当前并发ping数
    uint32_t prediction_accuracy;       // 预测准确率(%)
} nm_performance_metrics_t;

// ============================================================================
// 标准化接口 (BSP重构第二阶段) - 2025年6月10日
// ============================================================================

// 配置接口 (nm_config_*) - 统一配置管理
// 监控模式配置
void nm_config_set_fast_mode(bool enable);                 // 设置快速监控模式
void nm_config_set_adaptive_mode(bool enable);             // 设置自适应监控模式
void nm_config_set_concurrent_mode(bool enable);           // 设置并发监控模式
void nm_config_set_quality_monitor(bool enable);           // 设置质量监控模式

// 参数配置
void nm_config_set_interval(uint32_t interval_ms);         // 设置监控间隔
esp_err_t nm_config_set_advanced(const nm_advanced_config_t* config);  // 设置高级配置

// 状态查询
bool nm_config_is_fast_mode_enabled(void);                 // 检查快速模式状态
bool nm_config_is_adaptive_mode_enabled(void);             // 检查自适应模式状态
bool nm_config_is_concurrent_mode_enabled(void);           // 检查并发模式状态
void nm_config_get_advanced(nm_advanced_config_t* config); // 获取高级配置

// 性能接口 (nm_perf_*) - 统一性能监控
// 统计数据管理
void nm_perf_get_stats(nm_performance_stats_t* stats);     // 获取性能统计
void nm_perf_reset_stats(void);                            // 重置性能统计

// 指标数据管理
void nm_perf_get_metrics(nm_performance_metrics_t* metrics); // 获取性能指标
void nm_perf_reset_metrics(void);                          // 重置性能指标

// 实时性能监控
uint32_t nm_perf_get_current_latency(const char* ip);      // 获取指定IP当前延迟
float nm_perf_get_packet_loss_rate(const char* ip);        // 获取指定IP丢包率
uint32_t nm_perf_get_uptime_percent(const char* ip);       // 获取指定IP可用性百分比

#endif // NETWORK_MONITOR_H
