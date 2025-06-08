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

// 高实时性监控接口
void nm_enable_fast_monitoring(bool enable);  // 启用/禁用快速监控模式
void nm_set_monitoring_interval(uint32_t interval_ms);  // 设置监控间隔
bool nm_is_fast_mode_enabled(void);  // 检查是否启用快速模式

// 自适应监控接口
void nm_enable_adaptive_monitoring(bool enable);  // 启用/禁用自适应监控
void nm_enable_network_quality_monitoring(bool enable);  // 启用/禁用网络质量监控
bool nm_is_adaptive_mode_enabled(void);  // 检查是否启用自适应监控

// 性能统计接口
typedef struct {
    uint32_t total_pings;
    uint32_t successful_pings; 
    uint32_t failed_pings;
    uint32_t avg_response_time;
    uint32_t monitoring_cycles;
    uint32_t state_changes;
} nm_performance_stats_t;

void nm_get_performance_stats(nm_performance_stats_t* stats);  // 获取性能统计
void nm_reset_performance_stats(void);  // 重置性能统计

// BSP兼容接口函数
const network_target_t* nm_get_network_targets(void);
void nm_start_network_monitor(void);
void nm_stop_network_monitor(void);
void nm_get_network_status(void);

// 获取网络事件组句柄，可用于等待特定网络事件
EventGroupHandle_t nm_get_event_group(void);

// 并发监控优化接口
void nm_enable_concurrent_monitoring(bool enable);  // 启用/禁用并发监控模式
bool nm_is_concurrent_mode_enabled(void);          // 检查是否启用并发模式

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

void nm_set_advanced_config(const nm_advanced_config_t* config);
void nm_get_advanced_config(nm_advanced_config_t* config);

// 性能监控接口
typedef struct {
    uint32_t avg_ping_time;             // 平均ping时间
    uint32_t lock_contention_count;     // 锁竞争次数
    uint32_t queue_overflow_count;      // 队列溢出次数
    uint32_t concurrent_ping_count;     // 当前并发ping数
    uint32_t prediction_accuracy;       // 预测准确率(%)
} nm_performance_metrics_t;

void nm_get_performance_metrics(nm_performance_metrics_t* metrics);
void nm_reset_performance_metrics(void);

#endif // NETWORK_MONITOR_H
