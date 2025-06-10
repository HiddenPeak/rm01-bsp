#include "network_monitor.h"
#include "ping/ping_sock.h"  // 使用官方ping库
#include "esp_netif_ip_addr.h"
#include "esp_netif.h"
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

// 高实时性监控控制变量
static bool fast_monitoring_enabled = false;
static uint32_t monitoring_interval_ms = 1000;  // 默认3秒间隔

// 自适应监控功能
static bool adaptive_monitoring_enabled = false;
static uint32_t consecutive_success_count = 0;
static uint32_t consecutive_failure_count = 0;
static bool network_quality_monitoring = false;

// 性能统计
static nm_performance_stats_t performance_stats = {0};

// 并发监控优化相关变量
static bool concurrent_monitoring_enabled = false;
static QueueHandle_t ping_result_queue = NULL;
static esp_ping_handle_t persistent_ping_handles[NM_TARGET_COUNT];
static uint32_t ping_start_times[NM_TARGET_COUNT];
static volatile uint32_t active_ping_count = 0;
static SemaphoreHandle_t active_ping_semaphore = NULL;

// 高级配置 - 优化初始化时间
static nm_advanced_config_t advanced_config = {
    .ping_timeout_ms = 500,             // 优化: 1000→500ms ping超时
    .max_concurrent_pings = NM_TARGET_COUNT,
    .result_queue_size = 16,
    .enable_prediction = false,
    .enable_smart_scheduling = false
};

// 性能指标
static nm_performance_metrics_t performance_metrics = {0};

// ping结果结构体
typedef struct {
    uint8_t target_index;
    bool success;
    uint32_t response_time;
    uint32_t timestamp;
    esp_ping_handle_t handle;
} ping_result_t;

// 静态函数前置声明
static void nm_ping_success_callback(esp_ping_handle_t hdl, void *args);
static void nm_ping_timeout_callback(esp_ping_handle_t hdl, void *args);
static void nm_ping_end_callback(esp_ping_handle_t hdl, void *args);
static void nm_task(void *pvParameters);
static esp_err_t nm_start_simple_ping(int target_index);
static void nm_update_status(nm_target_t *target, nm_status_t new_status);

// 并发监控相关函数声明
static esp_err_t nm_create_persistent_ping_session(int target_index);
static void nm_concurrent_ping_success_callback(esp_ping_handle_t hdl, void *args);
static void nm_concurrent_ping_timeout_callback(esp_ping_handle_t hdl, void *args);
static void nm_concurrent_ping_end_callback(esp_ping_handle_t hdl, void *args);
static esp_err_t nm_start_concurrent_ping_all(void);
static void nm_process_ping_results_task(void *pvParameters);
static TaskHandle_t ping_result_processor_handle = NULL;

// 网络诊断函数声明
static void nm_diagnose_network_connectivity(void);
static void nm_show_network_interfaces(void);
static esp_err_t nm_test_connectivity(const char* ip, const char* name);

// 无锁状态查询实现
nm_status_t nm_get_status_lockfree(const char* ip) {
    if (!ip) return NM_STATUS_UNKNOWN;
    
    // 原子读取，无需锁保护
    for (int i = 0; i < NM_TARGET_COUNT; i++) {
        if (strcmp(nm_targets[i].ip, ip) == 0) {
            return nm_targets[i].status;  // 简单数据类型读取是原子的
        }
    }
    return NM_STATUS_UNKNOWN;
}

// 只读访问目标数组
const nm_target_t* nm_get_targets_readonly(void) {
    return nm_targets;  // 调用者承诺只读访问
}

// 检查网络监控系统是否已初始化
static bool is_network_monitor_initialized(void) {
    return (nm_mutex != NULL);
}

void nm_init(void) {
    // 创建互斥锁
    nm_mutex = xSemaphoreCreateMutex();
    
    // 创建事件组
    nm_event_group = xEventGroupCreate();
    
    // 创建信号量用于并发控制
    active_ping_semaphore = xSemaphoreCreateCounting(NM_TARGET_COUNT, NM_TARGET_COUNT);
    
    // 创建ping结果队列
    ping_result_queue = xQueueCreate(advanced_config.result_queue_size, sizeof(ping_result_t));
    
    // 初始化目标数组
    memset(nm_targets, 0, sizeof(nm_targets));
    memset(persistent_ping_handles, 0, sizeof(persistent_ping_handles));
    memset(ping_start_times, 0, sizeof(ping_start_times));
    
    // 配置目标IP和名称
    strncpy(nm_targets[0].ip, NM_COMPUTING_MODULE_IP, sizeof(nm_targets[0].ip));
    strncpy(nm_targets[0].name, "算力模块", sizeof(nm_targets[0].name));
    
    strncpy(nm_targets[1].ip, NM_APPLICATION_MODULE_IP, sizeof(nm_targets[1].ip));
    strncpy(nm_targets[1].name, "应用模块", sizeof(nm_targets[1].name));
    
    strncpy(nm_targets[2].ip, NM_USER_HOST_IP, sizeof(nm_targets[2].ip));
    strncpy(nm_targets[2].name, "用户主机", sizeof(nm_targets[2].name));
    
    strncpy(nm_targets[3].ip, NM_INTERNET_IP, sizeof(nm_targets[3].ip));
    strncpy(nm_targets[3].name, "互联网", sizeof(nm_targets[3].name));    // 设置索引和初始状态
    for (int i = 0; i < NM_TARGET_COUNT; i++) {
        nm_targets[i].index = i;
        nm_targets[i].status = NM_STATUS_UNKNOWN;      // 初始状态设为未知，ping后会更新为连接或断开
        nm_targets[i].prev_status = NM_STATUS_UNKNOWN;
        ipaddr_aton(nm_targets[i].ip, &nm_targets[i].addr);
    }
    
    // 启动ping结果处理任务
    xTaskCreate(nm_process_ping_results_task, "ping_processor", 3072, NULL, 6, &ping_result_processor_handle);
    
    ESP_LOGI(TAG, "网络监控系统初始化完成, 监控 %d 个目标, 并发模式 %s", 
             NM_TARGET_COUNT, concurrent_monitoring_enabled ? "启用" : "禁用");
    
    // 执行网络诊断
    nm_diagnose_network_connectivity();
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
                
                // 停止持久化会话
                if (persistent_ping_handles[i] != NULL) {
                    esp_ping_stop(persistent_ping_handles[i]);
                    esp_ping_delete_session(persistent_ping_handles[i]);
                    persistent_ping_handles[i] = NULL;
                }
            }
            
            active_ping_count = 0;
            xSemaphoreGive(nm_mutex);
        }
        
        // 删除任务
        vTaskDelete(nm_task_handle);
        nm_task_handle = NULL;
        
        // 删除结果处理任务
        if (ping_result_processor_handle != NULL) {
            vTaskDelete(ping_result_processor_handle);
            ping_result_processor_handle = NULL;
        }
        
        ESP_LOGI(TAG, "网络监控任务已停止");
    }
}

nm_status_t nm_get_status(const char* ip) {
    nm_status_t status = NM_STATUS_UNKNOWN;
    
    // 检查网络监控是否已初始化
    if (!is_network_monitor_initialized()) {
        ESP_LOGW(TAG, "网络监控系统未初始化，返回未知状态");
        return NM_STATUS_UNKNOWN;
    }
    
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
    // 检查网络监控是否已初始化
    if (!is_network_monitor_initialized()) {
        ESP_LOGW(TAG, "网络监控系统未初始化，无法获取状态");
        return;
    }
    
    // 获取互斥锁
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ESP_LOGI(TAG, "=== 网络状态报告 ===");
        for (int i = 0; i < NM_TARGET_COUNT; i++) {
            const char *status_str = "未知";
            if (nm_targets[i].status == NM_STATUS_UP) {
                status_str = "已连接";
            } else if (nm_targets[i].status == NM_STATUS_DOWN) {
                status_str = "已断开";
            }
            
            ESP_LOGI(TAG, "[%d] %s (%s): 状态=%s, 响应时间=%" PRIu32 "ms, 丢包率=%.1f%%, 发送=%" PRIu32 ", 接收=%" PRIu32,
                     i, nm_targets[i].name, nm_targets[i].ip, status_str, 
                     nm_targets[i].last_response_time, nm_targets[i].loss_rate,
                     nm_targets[i].packets_sent, nm_targets[i].packets_received);
        }
        ESP_LOGI(TAG, "=== 状态报告结束 ===");
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
            
            // 更新性能统计
            performance_stats.total_pings++;
            performance_stats.successful_pings++;
            if (performance_stats.successful_pings > 0) {
                performance_stats.avg_response_time = 
                    (performance_stats.avg_response_time * (performance_stats.successful_pings - 1) + elapsed_time) / 
                    performance_stats.successful_pings;
            }
            
            // 自适应监控逻辑
            consecutive_success_count++;
            consecutive_failure_count = 0;
            
            // 更新状态为连接
            nm_update_status(&nm_targets[target_index], NM_STATUS_UP);
            
            ESP_LOGI(TAG, "Ping成功: %s (%s), 时间=%" PRIu32 "ms, 连续成功=%"PRIu32, 
                     nm_targets[target_index].name, nm_targets[target_index].ip, elapsed_time, consecutive_success_count);
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
            
            // 更新性能统计
            performance_stats.total_pings++;
            performance_stats.failed_pings++;
            
            // 自适应监控逻辑
            consecutive_failure_count++;
            consecutive_success_count = 0;
            
            // 超时就应该设置为断开状态，不管丢包率
            nm_update_status(&nm_targets[target_index], NM_STATUS_DOWN);
            
            ESP_LOGW(TAG, "Ping超时: %s (%s), 丢包率=%.1f%%, 连续失败=%"PRIu32,
                     nm_targets[target_index].name, nm_targets[target_index].ip, loss_rate, consecutive_failure_count);
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
    ping_config.count = 1;                          // 只发送1个ping包提高速度
    ping_config.interval_ms = 50;                   // 优化: 100→50ms 减少间隔
    ping_config.timeout_ms = 500;                   // 500ms超时时间
    ping_config.task_stack_size = 3072;             // 优化: 减少栈空间
    ping_config.task_prio = 3;                      // 提高优先级
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
        // 设置状态为断开
        nm_update_status(&nm_targets[target_index], NM_STATUS_DOWN);
        current_ping_target = -1;        return ret;
    }
    
    // 获取当前状态描述
    const char *current_status_str = "未知";
    if (nm_targets[target_index].status == NM_STATUS_UP) {
        current_status_str = "已连接";
    } else if (nm_targets[target_index].status == NM_STATUS_DOWN) {
        current_status_str = "已断开";
    }
    
    ESP_LOGI(TAG, "开始ping测试: %s (当前状态: %s)", nm_targets[target_index].ip, current_status_str);
    ret = esp_ping_start(ping_handle);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动ping失败: %s, 目标=%s", 
                esp_err_to_name(ret), nm_targets[target_index].ip);
        // 设置状态为断开
        nm_update_status(&nm_targets[target_index], NM_STATUS_DOWN);
        esp_ping_delete_session(ping_handle);
        current_ping_target = -1;
        return ret;
    }    // 等待ping完成 - 给足够时间让超时回调执行
    vTaskDelay(pdMS_TO_TICKS(800));  // 500ms超时 + 300ms缓冲时间
    
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
    
    // 如果状态发生变化，打印日志并触发回调
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
        
        // 更新状态变化统计
        performance_stats.state_changes++;
        
        ESP_LOGI(TAG, "网络状态变化: %s 从 [%s] 变为 [%s], 响应时间=%" PRIu32 "ms, 丢包率=%.1f%%, 变化次数=%"PRIu32,
                 target->ip, prev_status_str, status_str, 
                 target->last_response_time, target->loss_rate, performance_stats.state_changes);
        
        // 触发状态变化回调（如果已注册）
        if (nm_status_change_callback != NULL) {
            nm_status_change_callback(target->index, target->ip, target->status, nm_callback_arg);
        }
        
        // 设置相应的事件位（用于事件组通知）
        if (nm_event_group != NULL) {
            EventBits_t event_bit = 0;
            switch (target->index) {
                case 0: // 算力模块
                    event_bit = (target->status == NM_STATUS_UP) ? NM_EVENT_COMPUTING_UP : NM_EVENT_COMPUTING_DOWN;
                    break;
                case 1: // 应用模块
                    event_bit = (target->status == NM_STATUS_UP) ? NM_EVENT_APPLICATION_UP : NM_EVENT_APPLICATION_DOWN;
                    break;
                case 2: // 用户主机
                    event_bit = (target->status == NM_STATUS_UP) ? NM_EVENT_USER_HOST_UP : NM_EVENT_USER_HOST_DOWN;
                    break;
                case 3: // 互联网
                    event_bit = (target->status == NM_STATUS_UP) ? NM_EVENT_INTERNET_UP : NM_EVENT_INTERNET_DOWN;
                    break;
            }
            if (event_bit != 0) {
                xEventGroupSetBits(nm_event_group, event_bit);
            }
        }
    }
}

// 获取事件组句柄
EventGroupHandle_t nm_get_event_group(void) {
    return nm_event_group;
}

// 检查模式状态（保留的状态查询函数）
bool nm_is_fast_mode_enabled(void) {
    return fast_monitoring_enabled;
}

bool nm_is_adaptive_mode_enabled(void) {
    return adaptive_monitoring_enabled;
}

static void nm_task(void *pvParameters) {
    ESP_LOGI(TAG, "网络监控任务开始运行, 模式: %s", 
             concurrent_monitoring_enabled ? "并发" : "顺序");
    
    if (concurrent_monitoring_enabled) {
        // 并发监控模式
        while (1) {
            uint32_t loop_start_time = xTaskGetTickCount();
            performance_stats.monitoring_cycles++;
            
            // 启动所有目标的并发ping
            esp_err_t ret = nm_start_concurrent_ping_all();
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "启动并发ping失败");
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            
            // 等待所有ping完成或超时
            uint32_t wait_time = advanced_config.ping_timeout_ms + 200; // 额外500ms缓冲
            uint32_t waited = 0;
            
            while (active_ping_count > 0 && waited < wait_time) {
                vTaskDelay(pdMS_TO_TICKS(100));
                waited += 100;
            }
            
            // 自适应监控逻辑
            uint32_t current_interval = monitoring_interval_ms;
            if (adaptive_monitoring_enabled) {
                if (consecutive_failure_count > 3) {
                    current_interval = current_interval / 2;
                    if (current_interval < 500) current_interval = 500;
                } else if (consecutive_success_count > 10) {
                    current_interval = current_interval * 1.2;
                    if (current_interval > 10000) current_interval = 10000;
                }
            }
            
            // 计算延迟时间
            uint32_t loop_elapsed = (xTaskGetTickCount() - loop_start_time) * portTICK_PERIOD_MS;
            if (loop_elapsed < current_interval) {
                vTaskDelay(pdMS_TO_TICKS(current_interval - loop_elapsed));
            }
            
            // 定期状态报告
            static int status_report_counter = 0;
            status_report_counter++;
            int report_interval = fast_monitoring_enabled ? 10 : 5;
            
            if (status_report_counter >= report_interval) {
                nm_get_all_status();
                if (network_quality_monitoring && performance_stats.monitoring_cycles % 20 == 0) {
                    ESP_LOGI(TAG, "并发监控统计: 周期=%"PRIu32", 总ping=%"PRIu32", 成功=%"PRIu32", 失败=%"PRIu32", 平均响应=%"PRIu32"ms, 队列溢出=%"PRIu32,
                             performance_stats.monitoring_cycles,
                             performance_stats.total_pings,
                             performance_stats.successful_pings,
                             performance_stats.failed_pings,
                             performance_metrics.avg_ping_time,
                             performance_metrics.queue_overflow_count);
                }
                status_report_counter = 0;
            }
        }
    } else {
        // 传统顺序监控模式（保持兼容性）
        while (1) {
            uint32_t loop_start_time = xTaskGetTickCount();
            performance_stats.monitoring_cycles++;
              for (int i = 0; i < NM_TARGET_COUNT; i++) {
                ESP_LOGI(TAG, "开始ping测试目标 %d: %s", i, nm_targets[i].ip);
                
                // 执行单次ping测试
                esp_err_t ret = nm_start_simple_ping(i);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "ping测试失败，目标: %s, 错误: %s", nm_targets[i].ip, esp_err_to_name(ret));
                    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                        nm_update_status(&nm_targets[i], NM_STATUS_DOWN);
                        performance_stats.failed_pings++;
                        xSemaphoreGive(nm_mutex);
                    }
                }
                
                // 目标间延迟
                uint32_t target_delay = fast_monitoring_enabled ? 200 : 500;
                vTaskDelay(pdMS_TO_TICKS(target_delay));
            }
            
            // 自适应监控逻辑
            uint32_t current_interval = monitoring_interval_ms;
            if (adaptive_monitoring_enabled) {
                if (consecutive_failure_count > 3) {
                    current_interval = current_interval / 2;
                    if (current_interval < 500) current_interval = 500;
                } else if (consecutive_success_count > 10) {
                    current_interval = current_interval * 1.2;
                    if (current_interval > 10000) current_interval = 10000;
                }
            }
            
            // 计算延迟时间
            uint32_t loop_elapsed = (xTaskGetTickCount() - loop_start_time) * portTICK_PERIOD_MS;
            if (loop_elapsed < current_interval) {
                vTaskDelay(pdMS_TO_TICKS(current_interval - loop_elapsed));
            }
            
            // 定期状态报告
            static int status_report_counter = 0;
            status_report_counter++;
            int report_interval = fast_monitoring_enabled ? 5 : 3;
            
            if (status_report_counter >= report_interval) {
                nm_get_all_status();
                if (network_quality_monitoring && performance_stats.monitoring_cycles % 10 == 0) {
                    ESP_LOGI(TAG, "顺序监控统计: 周期=%"PRIu32", 总ping=%"PRIu32", 成功=%"PRIu32", 失败=%"PRIu32", 平均响应=%"PRIu32"ms",
                             performance_stats.monitoring_cycles,
                             performance_stats.total_pings,
                             performance_stats.successful_pings,
                             performance_stats.failed_pings,
                             performance_stats.avg_response_time);
                }
                status_report_counter = 0;
            }
        }
    }
}

// 注册网络状态变化回调
void nm_register_status_change_callback(nm_status_change_cb_t callback, void* arg) {
    // 检查网络监控是否已初始化
    if (!is_network_monitor_initialized()) {
        ESP_LOGW(TAG, "网络监控系统未初始化，无法注册回调");
        return;
    }
    
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        nm_status_change_callback = callback;
        nm_callback_arg = arg;
        ESP_LOGI(TAG, "已注册网络状态变化回调函数");
        xSemaphoreGive(nm_mutex);
    }
}

// 获取指定IP的详细信息
bool nm_get_target_info(const char* ip, nm_target_t* target_info) {
    if (!ip || !target_info) {
        return false;
    }
    
    // 检查网络监控是否已初始化
    if (!is_network_monitor_initialized()) {
        ESP_LOGW(TAG, "网络监控系统未初始化，无法获取目标信息");
        return false;
    }
    
    bool found = false;
    
    // 获取互斥锁
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // 查找匹配的目标
        for (int i = 0; i < NM_TARGET_COUNT; i++) {
            if (strcmp(nm_targets[i].ip, ip) == 0) {
                // 拷贝目标信息
                memcpy(target_info, &nm_targets[i], sizeof(nm_target_t));
                found = true;
                break;
            }
        }
        xSemaphoreGive(nm_mutex);
    }
    
    return found;
}

// BSP兼容接口函数实现
const network_target_t* nm_get_network_targets(void) {
    static network_target_t network_targets[NETWORK_TARGET_COUNT];
    
    // 获取互斥锁
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // 转换内部结构到BSP兼容结构
        for (int i = 0; i < NM_TARGET_COUNT; i++) {
            strncpy(network_targets[i].ip, nm_targets[i].ip, sizeof(network_targets[i].ip));
            strncpy(network_targets[i].name, nm_targets[i].name, sizeof(network_targets[i].name));
            
            // 状态转换
            switch (nm_targets[i].status) {
                case NM_STATUS_UP:
                    network_targets[i].status = NETWORK_STATUS_UP;
                    break;
                case NM_STATUS_DOWN:
                    network_targets[i].status = NETWORK_STATUS_DOWN;
                    break;
                default:
                    network_targets[i].status = NETWORK_STATUS_UNKNOWN;
                    break;
            }
            
            // 前一状态转换
            switch (nm_targets[i].prev_status) {
                case NM_STATUS_UP:
                    network_targets[i].prev_status = NETWORK_STATUS_UP;
                    break;
                case NM_STATUS_DOWN:
                    network_targets[i].prev_status = NETWORK_STATUS_DOWN;
                    break;
                default:
                    network_targets[i].prev_status = NETWORK_STATUS_UNKNOWN;
                    break;
            }
            
            // 复制其他字段
            network_targets[i].last_response_time = nm_targets[i].last_response_time;
            network_targets[i].average_response_time = nm_targets[i].average_response_time;
            network_targets[i].packets_sent = nm_targets[i].packets_sent;
            network_targets[i].packets_received = nm_targets[i].packets_received;
            network_targets[i].loss_rate = nm_targets[i].loss_rate;
            network_targets[i].ping_handle = nm_targets[i].ping_handle;
            network_targets[i].index = nm_targets[i].index;
        }
        xSemaphoreGive(nm_mutex);
    }
    
    return network_targets;
}

void nm_start_network_monitor(void) {
    // BSP兼容的启动接口，直接调用标准接口
    nm_start_monitoring();
}

void nm_stop_network_monitor(void) {
    // BSP兼容的停止接口，直接调用标准接口
    nm_stop_monitoring();
}

void nm_get_network_status(void) {
    // BSP兼容的状态获取接口，直接调用标准接口
    nm_get_all_status();
}

// 网络诊断功能
static void nm_diagnose_network_connectivity(void) {
    ESP_LOGI(TAG, "=== 网络连接诊断开始 ===");
    
    // 显示网络接口信息
    nm_show_network_interfaces();
    
    // 检查网络接口状态
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        ESP_LOGW(TAG, "无法获取WiFi STA网络接口");
        netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
        if (netif == NULL) {
            ESP_LOGE(TAG, "无法获取任何网络接口");
            return;
        } else {
            ESP_LOGI(TAG, "使用以太网接口");
        }
    } else {
        ESP_LOGI(TAG, "使用WiFi接口");
    }
    
    // 获取IP地址信息
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "本机IP: " IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI(TAG, "网关IP: " IPSTR, IP2STR(&ip_info.gw));
        ESP_LOGI(TAG, "子网掩码: " IPSTR, IP2STR(&ip_info.netmask));
    } else {
        ESP_LOGW(TAG, "无法获取IP信息");
    }
    
    // 检查DNS服务器
    esp_netif_dns_info_t dns_info;
    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK) {
        ESP_LOGI(TAG, "主DNS: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    }
    
    ESP_LOGI(TAG, "=== 网络连接诊断结束 ===");
    
    // 测试所有目标的连接性
    ESP_LOGI(TAG, "=== 开始连接性测试 ===");
    for (int i = 0; i < NM_TARGET_COUNT; i++) {
        nm_test_connectivity(nm_targets[i].ip, nm_targets[i].name);
        vTaskDelay(pdMS_TO_TICKS(1000)); // 测试间隔1秒
    }
    ESP_LOGI(TAG, "=== 连接性测试完成 ===");
}

// 测试特定IP的网络可达性
static esp_err_t nm_test_connectivity(const char* ip, const char* name) {
    if (!ip || !name) return ESP_ERR_INVALID_ARG;
    
    ESP_LOGI(TAG, "测试网络连接: %s (%s)", name, ip);
    
    ip_addr_t target_addr;
    if (!ipaddr_aton(ip, &target_addr)) {
        ESP_LOGE(TAG, "无效的IP地址: %s", ip);
        return ESP_ERR_INVALID_ARG;
    }
    
    // 配置快速ping测试
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;
    ping_config.count = 3;                    // 发送3个包测试
    ping_config.interval_ms = 500;            // 500ms间隔
    ping_config.timeout_ms = 2000;            // 2秒超时
    ping_config.task_stack_size = 4096;
    ping_config.task_prio = 3;
    ping_config.data_size = 32;
    
    esp_ping_callbacks_t cbs = {
        .on_ping_success = NULL,  // 简单测试，不需要回调
        .on_ping_timeout = NULL,
        .on_ping_end = NULL,
        .cb_args = NULL
    };
    
    esp_ping_handle_t ping_handle;
    esp_err_t ret = esp_ping_new_session(&ping_config, &cbs, &ping_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建ping会话失败: %s -> %s", name, esp_err_to_name(ret));
        return ret;
    }
    
    // 启动ping
    ret = esp_ping_start(ping_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动ping失败: %s -> %s", name, esp_err_to_name(ret));
        esp_ping_delete_session(ping_handle);
        return ret;
    }
    
    // 等待ping完成
    vTaskDelay(pdMS_TO_TICKS(7000));  // 等待足够时间完成3次ping
    
    // 获取统计信息
    uint32_t transmitted, received;
    esp_ping_get_profile(ping_handle, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(ping_handle, ESP_PING_PROF_REPLY, &received, sizeof(received));
    
    float success_rate = (transmitted > 0) ? (100.0f * received / transmitted) : 0.0f;
    
    if (received > 0) {
        ESP_LOGI(TAG, "连接测试成功: %s (%s) - 成功率: %.1f%% (%"PRIu32"/%"PRIu32")", 
                 name, ip, success_rate, received, transmitted);
    } else {
        ESP_LOGW(TAG, "连接测试失败: %s (%s) - 无响应 (0/%"PRIu32")", name, ip, transmitted);
    }
    
    esp_ping_stop(ping_handle);
    esp_ping_delete_session(ping_handle);
    
    return (received > 0) ? ESP_OK : ESP_FAIL;
}

// 显示网络接口详细信息
static void nm_show_network_interfaces(void) {
    ESP_LOGI(TAG, "=== 网络接口信息 ===");
    
    // 检查Wi-Fi STA接口
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif) {
        ESP_LOGI(TAG, "Wi-Fi STA接口:");
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
            ESP_LOGI(TAG, "  IP: " IPSTR, IP2STR(&ip_info.ip));
            ESP_LOGI(TAG, "  网关: " IPSTR, IP2STR(&ip_info.gw));
            ESP_LOGI(TAG, "  掩码: " IPSTR, IP2STR(&ip_info.netmask));
        }
    }
    
    // 检查以太网接口
    esp_netif_t *eth_netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
    if (eth_netif) {
        ESP_LOGI(TAG, "以太网接口:");
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(eth_netif, &ip_info) == ESP_OK) {
            ESP_LOGI(TAG, "  IP: " IPSTR, IP2STR(&ip_info.ip));
            ESP_LOGI(TAG, "  网关: " IPSTR, IP2STR(&ip_info.gw));
            ESP_LOGI(TAG, "  掩码: " IPSTR, IP2STR(&ip_info.netmask));
        }
    }
    
    ESP_LOGI(TAG, "=== 网络接口信息结束 ===");
}

// 并发监控相关函数实现

// 创建持久化ping会话
static esp_err_t nm_create_persistent_ping_session(int target_index) {
    if (target_index < 0 || target_index >= NM_TARGET_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 如果已经存在，先删除
    if (persistent_ping_handles[target_index] != NULL) {
        esp_ping_stop(persistent_ping_handles[target_index]);
        esp_ping_delete_session(persistent_ping_handles[target_index]);
        persistent_ping_handles[target_index] = NULL;
    }
    
    // 配置ping参数
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = nm_targets[target_index].addr;
    ping_config.count = 1; // 每次只ping一次
    ping_config.interval_ms = 0; // 手动控制间隔
    ping_config.timeout_ms = advanced_config.ping_timeout_ms;
    ping_config.task_stack_size = 2048;
    ping_config.task_prio = 2;
    
    // 创建ping会话
    esp_err_t ret = esp_ping_new_session(&ping_config, NULL, &persistent_ping_handles[target_index]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建ping会话失败，目标索引: %d, 错误: %s", target_index, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "创建持久化ping会话成功，目标: %s", nm_targets[target_index].ip);
    return ESP_OK;
}

// 并发ping成功回调
static void nm_concurrent_ping_success_callback(esp_ping_handle_t hdl, void *args) {
    int target_index = (int)(intptr_t)args;
    uint32_t elapsed_time;
    
    // 获取ping响应时间
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    
    // 创建结果并发送到队列
    ping_result_t result = {
        .target_index = target_index,
        .success = true,
        .response_time = elapsed_time,
        .timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS,
        .handle = hdl
    };
    
    // 发送结果到队列
    if (xQueueSend(ping_result_queue, &result, 0) != pdTRUE) {
        performance_metrics.queue_overflow_count++;
        ESP_LOGW(TAG, "Ping结果队列满，目标: %s", nm_targets[target_index].ip);
    }
    
    ESP_LOGD(TAG, "并发Ping成功: %s, 时间=%" PRIu32 "ms", 
             nm_targets[target_index].ip, elapsed_time);
}

// 并发ping超时回调
static void nm_concurrent_ping_timeout_callback(esp_ping_handle_t hdl, void *args) {
    int target_index = (int)(intptr_t)args;
    
    // 创建结果并发送到队列
    ping_result_t result = {
        .target_index = target_index,
        .success = false,
        .response_time = advanced_config.ping_timeout_ms,
        .timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS,
        .handle = hdl
    };
    
    // 发送结果到队列
    if (xQueueSend(ping_result_queue, &result, 0) != pdTRUE) {
        performance_metrics.queue_overflow_count++;
        ESP_LOGW(TAG, "Ping结果队列满，目标: %s", nm_targets[target_index].ip);
    }
    
    ESP_LOGD(TAG, "并发Ping超时: %s", nm_targets[target_index].ip);
}

// 并发ping结束回调
static void nm_concurrent_ping_end_callback(esp_ping_handle_t hdl, void *args) {
    // 减少活跃ping计数
    if (active_ping_count > 0) {
        active_ping_count--;
    }
    
    // 释放信号量
    xSemaphoreGive(active_ping_semaphore);
    
    ESP_LOGD(TAG, "并发Ping结束，剩余活跃ping: %"PRIu32, active_ping_count);
}

// 启动所有目标的并发ping
static esp_err_t nm_start_concurrent_ping_all(void) {
    esp_err_t overall_result = ESP_OK;
    
    for (int i = 0; i < NM_TARGET_COUNT; i++) {
        // 等待信号量（控制并发数量）
        if (xSemaphoreTake(active_ping_semaphore, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "获取并发控制信号量失败，跳过目标: %s", nm_targets[i].ip);
            continue;
        }
        
        // 确保持久化会话存在
        if (persistent_ping_handles[i] == NULL) {
            esp_err_t ret = nm_create_persistent_ping_session(i);
            if (ret != ESP_OK) {
                xSemaphoreGive(active_ping_semaphore); // 释放信号量
                overall_result = ret;
                continue;
            }
        }
        
        // ESP-IDF ping库不支持动态设置回调，需要在创建会话时设置
        // 重新创建会话并设置回调
        esp_ping_stop(persistent_ping_handles[i]);
        esp_ping_delete_session(persistent_ping_handles[i]);
        
        esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
        ping_config.target_addr = nm_targets[i].addr;
        ping_config.count = 1;
        ping_config.interval_ms = 0;
        ping_config.timeout_ms = advanced_config.ping_timeout_ms;
        ping_config.task_stack_size = 2048;
        ping_config.task_prio = 2;
        
        esp_ping_callbacks_t cbs = {
            .on_ping_success = nm_concurrent_ping_success_callback,
            .on_ping_timeout = nm_concurrent_ping_timeout_callback,
            .on_ping_end = nm_concurrent_ping_end_callback,
            .cb_args = (void*)(intptr_t)i
        };
        
        esp_err_t ret = esp_ping_new_session(&ping_config, &cbs, &persistent_ping_handles[i]);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "重新创建带回调的ping会话失败，目标: %s, 错误: %s", 
                     nm_targets[i].ip, esp_err_to_name(ret));
            xSemaphoreGive(active_ping_semaphore);
            overall_result = ret;
            continue;
        }
        
        // 启动ping
        ret = esp_ping_start(persistent_ping_handles[i]);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "启动并发ping失败，目标: %s, 错误: %s", 
                     nm_targets[i].ip, esp_err_to_name(ret));
            xSemaphoreGive(active_ping_semaphore); // 释放信号量
            overall_result = ret;
        } else {
            active_ping_count++;
            performance_metrics.concurrent_ping_count++;
            ESP_LOGD(TAG, "启动并发ping，目标: %s", nm_targets[i].ip);
        }
    }
    
    return overall_result;
}

// Ping结果处理任务
static void nm_process_ping_results_task(void *pvParameters) {
    ping_result_t result;
    
    ESP_LOGI(TAG, "Ping结果处理任务启动");
    
    while (1) {
        // 从队列接收ping结果
        if (xQueueReceive(ping_result_queue, &result, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // 验证目标索引
            if (result.target_index >= NM_TARGET_COUNT) {
                ESP_LOGW(TAG, "无效的目标索引: %d", result.target_index);
                continue;
            }
            
            // 获取互斥锁处理结果
            if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                nm_target_t *target = &nm_targets[result.target_index];
                
                // 更新统计信息
                target->packets_sent++;
                performance_stats.total_pings++;
                
                if (result.success) {
                    // 处理成功结果
                    target->packets_received++;
                    target->last_response_time = result.response_time;
                    
                    // 计算平均响应时间
                    if (target->average_response_time == 0) {
                        target->average_response_time = result.response_time;
                    } else {
                        target->average_response_time = 
                            (target->average_response_time + result.response_time) / 2;
                    }
                    
                    // 更新性能统计
                    performance_stats.successful_pings++;
                    consecutive_success_count++;
                    consecutive_failure_count = 0;
                    
                    // 计算平均ping时间
                    if (performance_stats.successful_pings > 0) {
                        performance_stats.avg_response_time = 
                            (performance_stats.avg_response_time * (performance_stats.successful_pings - 1) + result.response_time) / 
                            performance_stats.successful_pings;
                        performance_metrics.avg_ping_time = performance_stats.avg_response_time;
                    }
                    
                    // 更新状态为连接
                    nm_update_status(target, NM_STATUS_UP);
                    
                    ESP_LOGD(TAG, "处理并发ping成功结果: %s, 时间=%" PRIu32 "ms", 
                             target->ip, result.response_time);
                } else {
                    // 处理失败结果
                    performance_stats.failed_pings++;
                    consecutive_failure_count++;
                    consecutive_success_count = 0;
                    
                    // 计算丢包率
                    target->loss_rate = 100.0f * (1.0f - (float)target->packets_received / (float)target->packets_sent);
                    
                    // 更新状态为断开
                    nm_update_status(target, NM_STATUS_DOWN);
                    
                    ESP_LOGD(TAG, "处理并发ping失败结果: %s", target->ip);
                }
                
                xSemaphoreGive(nm_mutex);
            } else {
                ESP_LOGW(TAG, "获取互斥锁失败，无法处理ping结果");
            }
        }
        
        // 定期清理和维护
        static int maintenance_counter = 0;
        maintenance_counter++;
        if (maintenance_counter >= 100) { // 每100次循环清理一次
            // 检查队列使用情况
            UBaseType_t queue_waiting = uxQueueMessagesWaiting(ping_result_queue);
            if (queue_waiting > advanced_config.result_queue_size * 0.8) {
                ESP_LOGW(TAG, "Ping结果队列使用率高: %u/%" PRIu32, 
                         (unsigned int)queue_waiting, advanced_config.result_queue_size);
            }
            maintenance_counter = 0;
        }
    }
}

// 新的并发监控接口实现

// 检查并发监控状态
bool nm_is_concurrent_monitoring_enabled(void) {
    return concurrent_monitoring_enabled;
}

// BSP兼容的并发模式检查接口
bool nm_is_concurrent_mode_enabled(void) {
    return concurrent_monitoring_enabled;
}

// ============================================================================
// 标准化接口实现 (BSP重构第二阶段) - 2025年6月10日
// ============================================================================

// 配置接口实现 (nm_config_*)

// 监控模式配置
void nm_config_set_fast_mode(bool enable) {
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        fast_monitoring_enabled = enable;
        if (enable) {
            monitoring_interval_ms = 800;  // 快速模式间隔
            ESP_LOGI(TAG, "[CONFIG] 启用快速监控模式，监控间隔=%" PRIu32 "ms", monitoring_interval_ms);
        } else {
            monitoring_interval_ms = 2000;  // 普通模式间隔
            ESP_LOGI(TAG, "[CONFIG] 禁用快速监控模式，监控间隔=%" PRIu32 "ms", monitoring_interval_ms);
        }
        xSemaphoreGive(nm_mutex);
    }
}

void nm_config_set_adaptive_mode(bool enable) {
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        adaptive_monitoring_enabled = enable;
        ESP_LOGI(TAG, "[CONFIG] 自适应监控模式 %s", enable ? "启用" : "禁用");
        xSemaphoreGive(nm_mutex);
    }
}

void nm_config_set_concurrent_mode(bool enable) {
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        concurrent_monitoring_enabled = enable;
        
        if (enable) {
            // 创建所有持久化ping会话
            for (int i = 0; i < NM_TARGET_COUNT; i++) {
                esp_err_t ret = nm_create_persistent_ping_session(i);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "[CONFIG] 创建持久化ping会话失败，目标: %s", nm_targets[i].ip);
                }
            }
        } else {
            // 清理持久化会话
            for (int i = 0; i < NM_TARGET_COUNT; i++) {
                if (persistent_ping_handles[i] != NULL) {
                    esp_ping_stop(persistent_ping_handles[i]);
                    esp_ping_delete_session(persistent_ping_handles[i]);
                    persistent_ping_handles[i] = NULL;
                }
            }
        }
        
        ESP_LOGI(TAG, "[CONFIG] 并发监控模式 %s", enable ? "启用" : "禁用");
        xSemaphoreGive(nm_mutex);
    }
}

void nm_config_set_quality_monitor(bool enable) {
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        network_quality_monitoring = enable;
        ESP_LOGI(TAG, "[CONFIG] 网络质量监控 %s", enable ? "启用" : "禁用");
        xSemaphoreGive(nm_mutex);
    }
}

// 参数配置
void nm_config_set_interval(uint32_t interval_ms) {
    if (interval_ms < 300) {
        interval_ms = 300;  // 最小间隔300ms
    } else if (interval_ms > 60000) {
        interval_ms = 60000;  // 最大间隔60秒
    }
    
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        monitoring_interval_ms = interval_ms;
        ESP_LOGI(TAG, "[CONFIG] 设置监控间隔为%" PRIu32 "ms", monitoring_interval_ms);
        xSemaphoreGive(nm_mutex);
    }
}

esp_err_t nm_config_set_advanced(const nm_advanced_config_t* config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        memcpy(&advanced_config, config, sizeof(nm_advanced_config_t));
        ESP_LOGI(TAG, "[CONFIG] 高级配置已更新");
        xSemaphoreGive(nm_mutex);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

// 状态查询
bool nm_config_is_fast_mode_enabled(void) {
    return fast_monitoring_enabled;
}

bool nm_config_is_adaptive_mode_enabled(void) {
    return adaptive_monitoring_enabled;
}

bool nm_config_is_concurrent_mode_enabled(void) {
    return concurrent_monitoring_enabled;
}

void nm_config_get_advanced(nm_advanced_config_t* config) {
    if (!config) {
        return;
    }
    
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        memcpy(config, &advanced_config, sizeof(nm_advanced_config_t));
        xSemaphoreGive(nm_mutex);
    }
}

// 性能接口实现 (nm_perf_*)

// 统计数据管理
void nm_perf_get_stats(nm_performance_stats_t* stats) {
    if (!stats) return;
    
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        memcpy(stats, &performance_stats, sizeof(nm_performance_stats_t));
        xSemaphoreGive(nm_mutex);
    }
}

void nm_perf_reset_stats(void) {
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        memset(&performance_stats, 0, sizeof(nm_performance_stats_t));
        ESP_LOGI(TAG, "[PERF] 性能统计已重置");
        xSemaphoreGive(nm_mutex);
    }
}

// 指标数据管理
void nm_perf_get_metrics(nm_performance_metrics_t* metrics) {
    if (!metrics) {
        return;
    }
    
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        memcpy(metrics, &performance_metrics, sizeof(nm_performance_metrics_t));
        xSemaphoreGive(nm_mutex);
    }
}

void nm_perf_reset_metrics(void) {
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        memset(&performance_metrics, 0, sizeof(nm_performance_metrics_t));
        ESP_LOGI(TAG, "[PERF] 性能指标已重置");
        xSemaphoreGive(nm_mutex);
    }
}

// 实时性能监控
uint32_t nm_perf_get_current_latency(const char* ip) {
    if (!ip) return 0;
    
    uint32_t latency = 0;
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        for (int i = 0; i < NM_TARGET_COUNT; i++) {
            if (strcmp(nm_targets[i].ip, ip) == 0) {
                latency = nm_targets[i].last_response_time;
                break;
            }
        }
        xSemaphoreGive(nm_mutex);
    }
    
    return latency;
}

float nm_perf_get_packet_loss_rate(const char* ip) {
    if (!ip) return 100.0f;
    
    float loss_rate = 100.0f;
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        for (int i = 0; i < NM_TARGET_COUNT; i++) {
            if (strcmp(nm_targets[i].ip, ip) == 0) {
                loss_rate = nm_targets[i].loss_rate;
                break;
            }
        }
        xSemaphoreGive(nm_mutex);
    }
    
    return loss_rate;
}

uint32_t nm_perf_get_uptime_percent(const char* ip) {
    if (!ip) return 0;
    
    uint32_t uptime_percent = 0;
    if (xSemaphoreTake(nm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        for (int i = 0; i < NM_TARGET_COUNT; i++) {
            if (strcmp(nm_targets[i].ip, ip) == 0) {
                if (nm_targets[i].packets_sent > 0) {
                    uptime_percent = (uint32_t)((float)nm_targets[i].packets_received / 
                                               (float)nm_targets[i].packets_sent * 100.0f);
                }
                break;
            }
        }
        xSemaphoreGive(nm_mutex);
    }
      return uptime_percent;
}