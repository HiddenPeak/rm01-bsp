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

// 获取网络事件组句柄，可用于等待特定网络事件
EventGroupHandle_t nm_get_event_group(void);

#endif // NETWORK_MONITOR_H
