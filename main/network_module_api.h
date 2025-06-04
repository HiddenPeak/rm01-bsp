/**
 * @file network_module_api.h
 * @brief 网络模组API接口定义 - 等待设备API启用后实现
 * 
 * 此文件定义了需要与算力模组(10.10.99.98)和应用模组(10.10.99.99)
 * 通信的HTTP API接口。这些功能需要等待对应设备的API服务启用后才能实现。
 */

#ifndef NETWORK_MODULE_API_H
#define NETWORK_MODULE_API_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========== 算力模组 API (10.10.99.98) ==========

/**
 * @brief 算力模组计算状态数据结构
 */
typedef struct {
    float cpu_usage;          // CPU使用率 (0-100%)
    float memory_usage;       // 内存使用率 (0-100%)
    uint32_t task_count;      // 活跃任务数量
    float load_average;       // 系统负载平均值
    bool is_computing;        // 是否正在执行计算任务
    uint32_t uptime_seconds;  // 系统运行时间(秒)
    bool valid;               // 数据有效标志
} compute_module_status_t;

/**
 * @brief 算力模组温度数据结构
 */
typedef struct {
    float cpu_temp;           // CPU温度 (°C)
    float board_temp;         // 主板温度 (°C)
    float ambient_temp;       // 环境温度 (°C)
    uint32_t timestamp;       // 时间戳
    bool valid;               // 数据有效标志
} compute_module_temp_t;

// ========== 应用模组 API (10.10.99.99) ==========

/**
 * @brief 应用模组状态数据结构
 */
typedef struct {
    uint32_t active_processes;    // 活跃进程数
    float resource_usage;         // 资源使用率 (0-100%)
    bool service_running;         // 主要服务是否运行
    uint32_t request_count;       // 请求处理数量
    uint32_t uptime_seconds;      // 系统运行时间(秒)
    bool valid;                   // 数据有效标志
} application_module_status_t;

/**
 * @brief 应用模组温度数据结构
 */
typedef struct {
    float cpu_temp;           // CPU温度 (°C)
    float board_temp;         // 主板温度 (°C)
    float ambient_temp;       // 环境温度 (°C)
    uint32_t timestamp;       // 时间戳
    bool valid;               // 数据有效标志
} application_module_temp_t;

// ========== TODO: 需要等待设备API启用后实现的函数 ==========

/**
 * @brief 从算力模组获取计算状态
 * 
 * HTTP请求: GET http://10.10.99.98/api/compute/status
 * 
 * @param status 输出参数，存储获取的状态数据
 * @return esp_err_t ESP_OK成功，其他值表示失败
 * 
 * @note 此函数需要等待算力模组的HTTP API服务启用后实现
 */
esp_err_t compute_module_get_status(compute_module_status_t* status);

/**
 * @brief 从算力模组获取温度数据
 * 
 * HTTP请求: GET http://10.10.99.98/api/temperature
 * 
 * @param temp 输出参数，存储获取的温度数据
 * @return esp_err_t ESP_OK成功，其他值表示失败
 * 
 * @note 此函数需要等待算力模组的HTTP API服务启用后实现
 */
esp_err_t compute_module_get_temperature(compute_module_temp_t* temp);

/**
 * @brief 从应用模组获取应用状态
 * 
 * HTTP请求: GET http://10.10.99.99/api/app/status
 * 
 * @param status 输出参数，存储获取的状态数据
 * @return esp_err_t ESP_OK成功，其他值表示失败
 * 
 * @note 此函数需要等待应用模组的HTTP API服务启用后实现
 */
esp_err_t application_module_get_status(application_module_status_t* status);

/**
 * @brief 从应用模组获取温度数据
 * 
 * HTTP请求: GET http://10.10.99.99/api/temperature
 * 
 * @param temp 输出参数，存储获取的温度数据
 * @return esp_err_t ESP_OK成功，其他值表示失败
 * 
 * @note 此函数需要等待应用模组的HTTP API服务启用后实现
 */
esp_err_t application_module_get_temperature(application_module_temp_t* temp);

// ========== 辅助函数 ==========

/**
 * @brief 初始化网络模组HTTP客户端
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 * 
 * @note 此函数需要等待ESP HTTP客户端配置完成后实现
 */
esp_err_t network_module_api_init(void);

/**
 * @brief 清理网络模组HTTP客户端资源
 */
void network_module_api_cleanup(void);

/**
 * @brief 测试网络模组API连接
 * 
 * @param computing_reachable 输出参数，算力模组是否可达
 * @param application_reachable 输出参数，应用模组是否可达
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t network_module_api_test_connection(bool* computing_reachable, bool* application_reachable);

// ========== 实现计划 ==========
/*
实现阶段：

阶段1：基础HTTP客户端
- 实现基本的HTTP GET请求功能
- 添加超时和错误处理
- 测试基本连接性

阶段2：API数据解析
- 实现JSON响应解析
- 添加数据验证和清理
- 处理网络异常情况

阶段3：集成到系统状态控制器
- 在 is_high_compute_load() 中集成计算状态检查
- 在 determine_system_state() 中集成温度数据获取
- 添加综合评估算法

阶段4：性能优化
- 实现数据缓存机制
- 添加异步请求支持
- 优化请求频率和超时设置
*/

#ifdef __cplusplus
}
#endif

#endif // NETWORK_MODULE_API_H
