#ifndef PING_UTILS_H
#define PING_UTILS_H

#include "ping/ping_sock.h"
#include "lwip/ip4_addr.h"
#include "esp_log.h"
#include <inttypes.h> // 添加此头文件以支持 PRIuXX 宏

/**
 * @brief 启动一个标准的 ping 测试
 * 
 * @param target_ip 目标 IP 地址字符串，如 "8.8.8.8"
 * @param count ping 包的数量
 * @return esp_ping_handle_t ping 会话句柄，失败则返回 NULL
 */
static inline esp_ping_handle_t ping_start_simple(const char *target_ip, uint32_t count)
{
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    esp_ping_handle_t ping = NULL;
    
    ip_addr_t target_addr;
    if (ipaddr_aton(target_ip, &target_addr) == 0) {
        ESP_LOGE("PING", "无效的IP地址: %s", target_ip);
        return NULL;
    }
    
    // 设置 ping 测试参数
    ping_config.target_addr = target_addr;
    ping_config.count = count;
    ping_config.interval_ms = 1000;
    ping_config.timeout_ms = 1000;
    ping_config.task_stack_size = 4096;
    ping_config.task_prio = 1;
    ping_config.data_size = 64;
    
    // 定义 ping 回调函数
    esp_ping_callbacks_t cbs = {
        .on_ping_success = NULL,
        .on_ping_timeout = NULL,
        .on_ping_end = NULL,
        .cb_args = NULL
    };
    
    // 创建并启动 ping 会话
    esp_err_t ret = esp_ping_new_session(&ping_config, &cbs, &ping);
    if (ret != ESP_OK) {
        ESP_LOGE("PING", "创建ping会话失败: %s", esp_err_to_name(ret));
        return NULL;
    }
    
    ret = esp_ping_start(ping);
    if (ret != ESP_OK) {
        ESP_LOGE("PING", "启动ping会话失败: %s", esp_err_to_name(ret));
        esp_ping_delete_session(ping);
        return NULL;
    }
    
    return ping;
}

/**
 * @brief 删除一个 ping 会话
 * 
 * @param ping ping 会话句柄
 */
static inline void ping_delete(esp_ping_handle_t ping)
{
    if (ping != NULL) {
        esp_ping_delete_session(ping);
    }
}

/**
 * @brief 获取 ping 会话的统计信息
 * 
 * @param hdl ping 会话句柄
 * @param transmitted 已发送的包数量
 * @param received 已接收的包数量
 * @param total_time_ms 总耗时（毫秒）
 */
static inline void ping_get_stats(esp_ping_handle_t hdl, uint32_t *transmitted, uint32_t *received, uint32_t *total_time_ms)
{
    if (hdl != NULL) {
        if (transmitted) {
            esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, transmitted, sizeof(uint32_t));
        }
        if (received) {
            esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, received, sizeof(uint32_t));
        }
        if (total_time_ms) {
            esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, total_time_ms, sizeof(uint32_t));
        }
    }
}

#endif /* PING_UTILS_H */