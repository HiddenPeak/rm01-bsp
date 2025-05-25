#include "network_monitor.h"
#include "ping/ping_sock.h"  // 使用官方ping库
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

// 当前正在ping的目标索引
static volatile int current_ping_target = -1;

// 静态函数前置声明
static void nm_ping_success_callback(esp_ping_handle_t hdl, void *args);
static void nm_ping_timeout_callback(esp_ping_handle_t hdl, void *args);
static void nm_ping_end_callback(esp_ping_handle_t hdl, void *args);
static void nm_task(void *pvParameters);
static esp_err_t nm_start_simple_ping(int target_index);
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

static void nm_ping_success_callback(esp_ping_handle_t hdl, void *args) {
    uint32_t elapsed_time;
    int target_index = current_ping_target;
    
    // 获取ping响应时间
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    
    // 获取互斥锁
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // 更新目标状态
        if (target_index >= 0 && target_index < NM_TARGET_COUNT) {
            nm_targets[target_index].last_response_time = elapsed_time;
            nm_targets[target_index].packets_received++;
            
            // 简单计算平均响应时间
            if (nm_targets[target_index].average_response_time == 0) {
                nm_targets[target_index].average_response_time = elapsed_time;
            } else {
                nm_targets[target_index].average_response_time = 
                    (nm_targets[target_index].average_response_time + elapsed_time) / 2;
            }
                
            // 更新状态为连接
            nm_update_status(&nm_targets[target_index], NM_STATUS_UP);
            
            ESP_LOGD(TAG, "Ping成功: %s, 时间=%" PRIu32 "ms", 
                     nm_targets[target_index].ip, elapsed_time);
        }
        xSemaphoreGive(nm_mutex);
    }
}

static void nm_ping_timeout_callback(esp_ping_handle_t hdl, void *args) {
    int target_index = current_ping_target;
    
    // 获取互斥锁
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // 更新目标状态
        if (target_index >= 0 && target_index < NM_TARGET_COUNT) {
            nm_targets[target_index].packets_sent++;
            
            // 计算丢包率
            float loss_rate = 100.0f * (1.0f - (float)nm_targets[target_index].packets_received / 
                                      (float)nm_targets[target_index].packets_sent);
            nm_targets[target_index].loss_rate = loss_rate;
            
            // 如果丢包率高于70%，认为断开
            if (loss_rate > 70.0f) {
                nm_update_status(&nm_targets[target_index], NM_STATUS_DOWN);
            }
            
            ESP_LOGD(TAG, "Ping超时: %s, 丢包率=%.1f%%", 
                     nm_targets[target_index].ip, loss_rate);
        }
        xSemaphoreGive(nm_mutex);
    }
}

static void nm_ping_end_callback(esp_ping_handle_t hdl, void *args) {
    uint32_t transmitted, received;
    int target_index = current_ping_target;
    
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    
    // 获取互斥锁
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (target_index >= 0 && target_index < NM_TARGET_COUNT) {
            nm_targets[target_index].packets_sent = transmitted;
            nm_targets[target_index].packets_received = received;
            
            if (transmitted > 0) {
                nm_targets[target_index].loss_rate = 
                    (transmitted - received) * 100.0f / transmitted;
            }
            
            ESP_LOGD(TAG, "Ping结束: %s, 发送=%" PRIu32 ", 接收=%" PRIu32,
                   nm_targets[target_index].ip, transmitted, received);
        }
        xSemaphoreGive(nm_mutex);
    }
}

static esp_err_t nm_start_simple_ping(int target_index) {
    if (target_index < 0 || target_index >= NM_TARGET_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    current_ping_target = target_index;
    
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = nm_targets[target_index].addr;
    ping_config.count = 3;                          // 发送3个ping包
    ping_config.interval_ms = 1000;                 // 每秒一个
    ping_config.timeout_ms = 2000;                  // 2秒超时
    ping_config.task_stack_size = 4096;             // 增加栈空间
    ping_config.task_prio = 2;                      // 中等优先级
    ping_config.data_size = 32;                     // 小数据包
    
    esp_ping_callbacks_t cbs = {
        .on_ping_success = nm_ping_success_callback,
        .on_ping_timeout = nm_ping_timeout_callback,
        .on_ping_end = nm_ping_end_callback,
        .cb_args = NULL  // 使用全局变量，不传递参数
    };
    
    esp_ping_handle_t ping_handle;
    esp_err_t ret = esp_ping_new_session(&ping_config, &cbs, &ping_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建ping会话失败: %s, 目标=%s", 
                esp_err_to_name(ret), nm_targets[target_index].ip);
        current_ping_target = -1;
        return ret;
    }
    
    ESP_LOGD(TAG, "开始ping测试: %s", nm_targets[target_index].ip);
    ret = esp_ping_start(ping_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动ping失败: %s, 目标=%s", 
                esp_err_to_name(ret), nm_targets[target_index].ip);
        esp_ping_delete_session(ping_handle);
        current_ping_target = -1;
        return ret;
    }
    
    // 等待ping完成
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // 停止并删除ping会话
    esp_ping_stop(ping_handle);
    esp_ping_delete_session(ping_handle);
    current_ping_target = -1;
    
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

// ===== BSP兼容层适配器 =====
// 这部分代码为了兼容原有的BSP接口而保留

// 网络监控目标适配器
static network_target_t network_targets[NETWORK_TARGET_COUNT];

// 将 nm_target_t 转换为 network_target_t
static void convert_nm_to_network_target(const nm_target_t* nm_target, network_target_t* net_target, int index) {
    if (!nm_target || !net_target) return;
    
    strncpy(net_target->ip, nm_target->ip, sizeof(net_target->ip));
    net_target->status = (network_status_t)nm_target->status;
    net_target->prev_status = (network_status_t)nm_target->prev_status;
    net_target->last_response_time = nm_target->last_response_time;
    net_target->average_response_time = nm_target->average_response_time;
    net_target->packets_sent = nm_target->packets_sent;
    net_target->packets_received = nm_target->packets_received;
    net_target->loss_rate = nm_target->loss_rate;
    net_target->ping_handle = nm_target->ping_handle;
    net_target->index = nm_target->index;
    
    // 设置目标名称
    const char* names[] = {"算力模块", "应用模块", "用户主机", "互联网"};
    if (index >= 0 && index < 4) {
        strncpy(net_target->name, names[index], sizeof(net_target->name));
    }
}

// 更新适配器数组
static void update_network_targets_adapter(void) {
    for (int i = 0; i < NETWORK_TARGET_COUNT; i++) {
        nm_target_t nm_info;
        const char* ips[] = {NM_COMPUTING_MODULE_IP, NM_APPLICATION_MODULE_IP, NM_USER_HOST_IP, NM_INTERNET_IP};
        
        if (nm_get_target_info(ips[i], &nm_info)) {
            convert_nm_to_network_target(&nm_info, &network_targets[i], i);
        }
    }
}

// ===== BSP兼容接口实现 =====

// 获取网络监控目标数组（BSP兼容API）
const network_target_t* nm_get_network_targets(void) {
    update_network_targets_adapter();
    return network_targets;
}

// 启动网络监控（BSP兼容API）
void nm_start_network_monitor(void) {
    ESP_LOGI(TAG, "启动网络监控系统");
    
    // 初始化并启动网络监控
    nm_init();
    nm_start_monitoring();
}

// 停止网络监控（BSP兼容API）
void nm_stop_network_monitor(void) {
    ESP_LOGI(TAG, "停止网络监控系统");
    nm_stop_monitoring();
}

// 获取网络状态（BSP兼容API）
void nm_get_network_status(void) {
    ESP_LOGI(TAG, "网络状态报告:");
    
    // 更新适配器数组
    update_network_targets_adapter();
    
    for (int i = 0; i < NETWORK_TARGET_COUNT; i++) {
        const char *status_str = "未知";
        if (network_targets[i].status == NETWORK_STATUS_UP) {
            status_str = "连接";
        } else if (network_targets[i].status == NETWORK_STATUS_DOWN) {
            status_str = "断开";
        }
        
        ESP_LOGI(TAG, "目标 %s (%s): 状态=%s, 响应时间=%" PRIu32 "ms, 丢包率=%.1f%%",
                 network_targets[i].name, network_targets[i].ip, status_str, 
                 network_targets[i].last_response_time,
                 network_targets[i].loss_rate);
    }
}

static void nm_task(void *pvParameters) {
    ESP_LOGI(TAG, "网络监控任务开始运行");
    
    // 任务主循环 - 顺序ping每个目标
    while (1) {
        for (int i = 0; i < NM_TARGET_COUNT; i++) {
            ESP_LOGI(TAG, "开始ping测试目标 %d: %s", i, nm_targets[i].ip);
            
            // 执行单次ping测试 - 使用简化的方法
            esp_err_t ret = nm_start_simple_ping(i);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "ping测试失败，目标: %s", nm_targets[i].ip);
                // 如果ping失败，标记为断开状态
                if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    nm_update_status(&nm_targets[i], NM_STATUS_DOWN);
                    xSemaphoreGive(nm_mutex);
                }
            }
            
            // 等待一段时间再测试下一个目标
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        
        // 所有目标ping完成后，等待一段时间再开始下一轮
        vTaskDelay(pdMS_TO_TICKS(10000));
        
        // 定期打印汇总信息
        nm_get_all_status();
    }
}
