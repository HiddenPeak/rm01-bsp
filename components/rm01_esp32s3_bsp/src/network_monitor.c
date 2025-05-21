#include "network_monitor.h"

// ESP-IDF ping API函数声明
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ESP ping函数声明
 * 确保esp_ping API函数被正确声明
 */
esp_err_t esp_ping_new_session(const void* config, const void* callback, void** handle);
esp_err_t esp_ping_delete_session(void* handle);
esp_err_t esp_ping_start(void* handle);
esp_err_t esp_ping_stop(void* handle);
esp_err_t esp_ping_get_profile(void* handle, int profile_type, void* data, uint32_t size);

#ifdef __cplusplus
}
#endif

// 定义ESP Ping的常量
#define ESP_PING_PROF_SEQNO    0
#define ESP_PING_PROF_TTL      1
#define ESP_PING_PROF_IPADDR   2
#define ESP_PING_PROF_SIZE     3
#define ESP_PING_PROF_TIMEGAP  4
#define ESP_PING_PROF_DURATION 5
#define ESP_PING_PROF_REQUEST  6
#define ESP_PING_PROF_REPLY    7

// 定义默认ping配置
#define ESP_PING_DEFAULT_CONFIG()     \
    {                                 \
        .count = 5,                   \
        .interval_ms = 1000,          \
        .timeout_ms = 1000,           \
        .data_size = 56,              \
        .tos = 0,                     \
        .task_stack_size = 2048,      \
        .task_prio = 2,               \
    }

// ESP ping配置结构体
typedef struct {
    uint32_t count;                 // ping的次数，0表示永久ping
    uint32_t interval_ms;           // ping间隔(ms)
    uint32_t timeout_ms;            // ping超时时间(ms)
    uint32_t data_size;             // ping数据大小
    uint8_t tos;                    // 服务类型
    uint32_t task_stack_size;       // ping任务的栈大小
    uint32_t task_prio;             // ping任务的优先级
    ip_addr_t target_addr;          // 目标IP地址
} esp_ping_config_t;

// ESP ping回调结构体
typedef struct {
    void (*on_ping_success)(void *hdl, void *args); // ping成功回调
    void (*on_ping_timeout)(void *hdl, void *args); // ping超时回调
    void (*on_ping_end)(void *hdl, void *args);     // ping结束回调
    void *cb_args;                                 // 回调函数参数
} esp_ping_callbacks_t;
#include "esp_netif_ip_addr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "NET_MON";

// 全局网络监控目标数组
static nm_target_t nm_targets[NM_TARGET_COUNT];

// 互斥锁，保护数据访问
static SemaphoreHandle_t nm_mutex;

// 事件组，用于同步和控制
static TaskHandle_t nm_task_handle = NULL;

// 网络事件组
static EventGroupHandle_t nm_event_group = NULL;

// 状态变化回调函数
static nm_status_change_cb_t nm_status_change_callback = NULL;
static void* nm_callback_arg = NULL;

// 静态函数前置声明
static void nm_ping_success_callback(void *hdl, void *args);
static void nm_ping_timeout_callback(void *hdl, void *args);
static void nm_ping_end_callback(void *hdl, void *args);
static void nm_task(void *pvParameters);
static esp_err_t nm_start_ping(nm_target_t *target);
static void nm_update_status(nm_target_t *target, nm_status_t new_status);

void nm_init(void) {
    // 创建互斥锁
    nm_mutex = xSemaphoreCreateMutex();
    
    // 初始化目标数组
    memset(nm_targets, 0, sizeof(nm_targets));
    
    // 配置目标IP
    strncpy(nm_targets[0].ip, NM_COMPUTING_MODULE_IP, sizeof(nm_targets[0].ip));
    strncpy(nm_targets[1].ip, NM_APPLICATION_MODULE_IP, sizeof(nm_targets[1].ip));
    strncpy(nm_targets[2].ip, NM_USER_HOST_IP, sizeof(nm_targets[2].ip));
    strncpy(nm_targets[3].ip, NM_INTERNET_IP, sizeof(nm_targets[3].ip));
    
    // 设置索引和初始状态
    for (int i = 0; i < NM_TARGET_COUNT; i++) {
        nm_targets[i].index = i;
        nm_targets[i].status = NM_STATUS_UNKNOWN;
        nm_targets[i].prev_status = NM_STATUS_UNKNOWN;
        ipaddr_aton(nm_targets[i].ip, &nm_targets[i].addr);
    }
    
    ESP_LOGI(TAG, "网络监控系统初始化完成, 监控 %d 个目标", NM_TARGET_COUNT);
}

void nm_start_monitoring(void) {
    // 创建监控任务
    if (nm_task_handle == NULL) {
        xTaskCreate(nm_task, "network_monitor", 4096, NULL, 5, &nm_task_handle);
        ESP_LOGI(TAG, "网络监控任务已启动");
    } else {
        ESP_LOGW(TAG, "网络监控任务已在运行");
    }
}

void nm_stop_monitoring(void) {
    // 停止任务
    if (nm_task_handle != NULL) {
        // 获取互斥锁
        if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // 停止所有ping会话
            for (int i = 0; i < NM_TARGET_COUNT; i++) {
                if (nm_targets[i].ping_handle != NULL) {
                    esp_ping_stop(nm_targets[i].ping_handle);
                    esp_ping_delete_session(nm_targets[i].ping_handle);
                    nm_targets[i].ping_handle = NULL;
                }
            }
            xSemaphoreGive(nm_mutex);
        }
        
        // 删除任务
        vTaskDelete(nm_task_handle);
        nm_task_handle = NULL;
        ESP_LOGI(TAG, "网络监控任务已停止");
    }
}

nm_status_t nm_get_status(const char* ip) {
    nm_status_t status = NM_STATUS_UNKNOWN;
    
    // 获取互斥锁
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // 查找匹配的目标
        for (int i = 0; i < NM_TARGET_COUNT; i++) {
            if (strcmp(nm_targets[i].ip, ip) == 0) {
                status = nm_targets[i].status;
                break;
            }
        }
        xSemaphoreGive(nm_mutex);
    }
    
    return status;
}

void nm_get_all_status(void) {
    // 获取互斥锁
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ESP_LOGI(TAG, "网络状态报告:");
        for (int i = 0; i < NM_TARGET_COUNT; i++) {
            const char *status_str = "未知";
            if (nm_targets[i].status == NM_STATUS_UP) {
                status_str = "连接";
            } else if (nm_targets[i].status == NM_STATUS_DOWN) {
                status_str = "断开";
            }
            
            ESP_LOGI(TAG, "目标 %s: 状态=%s, 响应时间=%" PRIu32 "ms, 丢包率=%.1f%%",
                     nm_targets[i].ip, status_str, 
                     nm_targets[i].last_response_time,
                     nm_targets[i].loss_rate);
        }
        xSemaphoreGive(nm_mutex);
    }
}

void nm_print_status_all(void) {
    nm_get_all_status();
}

static void nm_ping_success_callback(void *hdl, void *args) {
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    uint8_t index = *((uint8_t*)args);
    
    // 获取ping结果信息
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    
    // 获取互斥锁
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // 更新目标状态
        if (index < NM_TARGET_COUNT) {
            nm_targets[index].last_response_time = elapsed_time;
            nm_targets[index].packets_received++;
            
            // 简单计算平均响应时间
            nm_targets[index].average_response_time = 
                (nm_targets[index].average_response_time + elapsed_time) / 2;
                
            // 更新状态为连接
            nm_update_status(&nm_targets[index], NM_STATUS_UP);
            
            ESP_LOGD(TAG, "Ping成功: %s, 序列号=%" PRIu16 ", 时间=%" PRIu32 "ms", 
                     nm_targets[index].ip, seqno, elapsed_time);
        }
        xSemaphoreGive(nm_mutex);
    }
}

static void nm_ping_timeout_callback(void *hdl, void *args) {
    uint16_t seqno;
    ip_addr_t target_addr;
    uint8_t index = *((uint8_t*)args);
    
    // 获取ping超时信息
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    
    // 获取互斥锁
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // 更新目标状态
        if (index < NM_TARGET_COUNT) {
            // 连续超时几次后才认为断开，这里简单判断如果收包率低于30%就认为断开
            nm_targets[index].packets_sent++;
            float loss_rate = 100.0f * (1.0f - (float)nm_targets[index].packets_received / 
                                      (float)nm_targets[index].packets_sent);
            nm_targets[index].loss_rate = loss_rate;
            
            if (loss_rate > 70.0f) {
                nm_update_status(&nm_targets[index], NM_STATUS_DOWN);
            }
            
            ESP_LOGD(TAG, "Ping超时: %s, 序列号=%" PRIu16 ", 丢包率=%.1f%%", 
                     nm_targets[index].ip, seqno, loss_rate);
        }
        xSemaphoreGive(nm_mutex);
    }
}

static void nm_ping_end_callback(esp_ping_handle_t hdl, void *args) {
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;
    uint8_t index = *((uint8_t*)args);
    
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    float loss_rate = 0;
    
    if (transmitted > 0) {
        loss_rate = (transmitted - received) * 100.0f / transmitted;
    }
    
    // 获取互斥锁
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (index < NM_TARGET_COUNT) {
            nm_targets[index].packets_sent = transmitted;
            nm_targets[index].packets_received = received;
            nm_targets[index].loss_rate = loss_rate;
            
            ESP_LOGI(TAG, "Ping会话结束: %s, 发送=%" PRIu32 ", 接收=%" PRIu32 ", 丢包率=%.1f%%, 总时间=%" PRIu32 "ms",
                   nm_targets[index].ip, transmitted, received, loss_rate, total_time_ms);
                
            // 重启ping会话，实现持续监控
            esp_ping_stop(nm_targets[index].ping_handle);
            esp_ping_delete_session(nm_targets[index].ping_handle);
            nm_targets[index].ping_handle = NULL;
            
            // 稍微延迟一点再重启，避免过于频繁
            vTaskDelay(pdMS_TO_TICKS(1000));
            nm_start_ping(&nm_targets[index]);
        }
        xSemaphoreGive(nm_mutex);
    }
}

static esp_err_t nm_start_ping(nm_target_t *target) {
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    
    ping_config.target_addr = target->addr;      // 目标IP地址
    ping_config.count = 10;                      // 发送10个ping包
    ping_config.interval_ms = 1000;              // 每秒发送一次
    ping_config.timeout_ms = 1000;               // 1秒超时
    ping_config.task_stack_size = 4096;          // ping任务的栈大小
    ping_config.task_prio = 1;                   // 任务优先级
    ping_config.tos = 0;                         // 服务类型
    ping_config.data_size = 64;                  // ICMP包数据大小
    
    esp_ping_callbacks_t cbs = {
        .on_ping_success = nm_ping_success_callback,
        .on_ping_timeout = nm_ping_timeout_callback,
        .on_ping_end = nm_ping_end_callback,
        .cb_args = &target->index  // 传递目标索引作为回调参数
    };
    
    esp_err_t ret = esp_ping_new_session(&ping_config, &cbs, &target->ping_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建ping会话失败: %s, 目标=%s", esp_err_to_name(ret), target->ip);
        return ret;
    }
    
    ESP_LOGI(TAG, "开始ping测试，目标: %s", target->ip);
    ret = esp_ping_start(target->ping_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动ping会话失败: %s, 目标=%s", esp_err_to_name(ret), target->ip);
        esp_ping_delete_session(target->ping_handle);
        target->ping_handle = NULL;
        return ret;
    }
    
    return ESP_OK;
}

static void nm_update_status(nm_target_t *target, nm_status_t new_status) {
    // 保存旧状态
    target->prev_status = target->status;
    
    // 设置新状态
    target->status = new_status;
    
    // 如果状态发生变化，打印日志
    if (target->status != target->prev_status) {
        const char *status_str = "未知";
        const char *prev_status_str = "未知";
        
        if (target->status == NM_STATUS_UP) {
            status_str = "连接";
        } else if (target->status == NM_STATUS_DOWN) {
            status_str = "断开";
        }
        
        if (target->prev_status == NM_STATUS_UP) {
            prev_status_str = "连接";
        } else if (target->prev_status == NM_STATUS_DOWN) {
            prev_status_str = "断开";
        }
        
        ESP_LOGI(TAG, "网络状态变化: %s 从 [%s] 变为 [%s], 响应时间=%" PRIu32 "ms, 丢包率=%.1f%%",
                 target->ip, prev_status_str, status_str, 
                 target->last_response_time, target->loss_rate);
    }
}

// 获取事件组句柄
EventGroupHandle_t nm_get_event_group(void) {
    return nm_event_group;
}

// 注册状态变化回调
void nm_register_status_change_callback(nm_status_change_cb_t callback, void* arg) {
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        nm_status_change_callback = callback;
        nm_callback_arg = arg;
        xSemaphoreGive(nm_mutex);
        ESP_LOGI(TAG, "网络状态变化回调已注册");
    }
}

// 获取指定IP的详细信息
bool nm_get_target_info(const char* ip, nm_target_t* target_info) {
    bool found = false;
    
    if (!ip || !target_info) {
        return false;
    }
    
    // 获取互斥锁
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // 查找匹配的目标
        for (int i = 0; i < NM_TARGET_COUNT; i++) {
            if (strcmp(nm_targets[i].ip, ip) == 0) {
                // 复制数据
                memcpy(target_info, &nm_targets[i], sizeof(nm_target_t));
                found = true;
                break;
            }
        }
        xSemaphoreGive(nm_mutex);
    }
    
    return found;
}

static void nm_task(void *pvParameters) {
    ESP_LOGI(TAG, "网络监控任务开始运行");
    
    // 启动对所有目标的ping
    for (int i = 0; i < NM_TARGET_COUNT; i++) {
        // 获取互斥锁
        if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            nm_start_ping(&nm_targets[i]);
            xSemaphoreGive(nm_mutex);
        }
        
        // 稍微间隔一下，避免同时发起多个ping导致网络压力
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // 任务主循环
    while (1) {
        // 定期打印汇总信息
        vTaskDelay(pdMS_TO_TICKS(30000)); // 每30秒打印一次汇总信息
        nm_get_all_status();
    }
}
