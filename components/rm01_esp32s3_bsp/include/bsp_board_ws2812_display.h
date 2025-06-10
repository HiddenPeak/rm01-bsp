/**
 * @file bsp_board_ws2812_display.h
 * @brief Board WS2812 显示控制器 - 系统状态温度GPU内存监控显示
 * 
 * 基于优先级的系统状态显示： * - N305/Jetson高温: 红色慢速呼吸 (高优先级)
 * - Jetson功率过高: 紫色快速呼吸 (中优先级)  
 * - Jetson内存高使用率(90%): 紫色慢速呼吸 (低优先级)
 * 
 * 通过Prometheus API获取系统监控数据
 */

#ifndef BSP_BOARD_WS2812_DISPLAY_H
#define BSP_BOARD_WS2812_DISPLAY_H

#include "esp_err.h"
#include "bsp_state_manager.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========== 显示优先级和模式定义 ==========

/**
 * @brief Board WS2812 显示模式 (基于优先级)
 */
typedef enum {
    BOARD_DISPLAY_MODE_OFF = 0,             // 关闭状态
    BOARD_DISPLAY_MODE_HIGH_TEMP,           // 高温警告 - 红色慢速呼吸 (最高优先级)
    BOARD_DISPLAY_MODE_HIGH_POWER,          // 功率过高 - 紫色快速呼吸 (中优先级)
    BOARD_DISPLAY_MODE_MEMORY_HIGH_USAGE,   // 内存高使用率 - 紫色慢速呼吸 (低优先级)
    BOARD_DISPLAY_MODE_COUNT                // 模式总数
} board_display_mode_t;

/**
 * @brief 呼吸动画速度
 */
typedef enum {
    BOARD_BREATH_SPEED_SLOW = 0,    // 慢速呼吸 (3000ms周期)
    BOARD_BREATH_SPEED_NORMAL,      // 正常呼吸 (2000ms周期)
    BOARD_BREATH_SPEED_FAST         // 快速呼吸 (1000ms周期)
} board_breath_speed_t;

// ========== 监控阈值定义 ==========

// 温度阈值
#define BOARD_TEMP_THRESHOLD_HIGH        85.0f   // 高温阈值 (°C)
#define BOARD_TEMP_THRESHOLD_RECOVERY    80.0f   // 温度恢复阈值 (°C，滞回)

// Jetson功率阈值 (替代GPU使用率)
#define BOARD_POWER_THRESHOLD_HIGH       15000.0f   // 高功率阈值 (mW，15W)
#define BOARD_POWER_THRESHOLD_RECOVERY   12000.0f   // 功率恢复阈值 (mW，12W)

// 内存使用率阈值
#define BOARD_MEMORY_USAGE_THRESHOLD     90.0f   // 内存高使用率阈值 (%)
#define BOARD_MEMORY_USAGE_RECOVERY      85.0f   // 内存使用率恢复阈值 (%)

// Prometheus API配置 - 统一使用N305的Prometheus服务器查询所有数据
#define BOARD_PROMETHEUS_API            "http://10.10.99.99:59100/api/v1/query"
#define BOARD_METRICS_UPDATE_INTERVAL   10000   // 数据更新间隔 (ms)
#define BOARD_HTTP_TIMEOUT_MS           5000    // HTTP请求超时时间 (ms)

// 查询参数定义 - 根据实际API响应格式更新
#define N305_TEMP_QUERY                 "node_hwmon_temp_celsius{chip=\"platform_coretemp_0\",sensor=\"temp1\"}"
#define JETSON_CPU_TEMP_QUERY           "temperature_C{statistic=\"cpu\"}"
#define JETSON_GPU_TEMP_QUERY           "temperature_C{statistic=\"gpu\"}"
#define JETSON_POWER_QUERY              "integrated_power_mW{statistic=\"power\"}"
#define JETSON_MEMORY_TOTAL_QUERY       "ram_kB{statistic=\"total\"}"
#define JETSON_MEMORY_USED_QUERY        "ram_kB{statistic=\"used\"}"
#define JSON_BUFFER_SIZE                2048    // JSON解析缓冲区大小

// ========== 配置结构体 ==========

/**
 * @brief Board WS2812 显示控制器配置
 */
typedef struct {
    bool auto_mode_enabled;          // 是否启用自动模式
    bool debug_mode;                 // 调试模式
    uint8_t brightness;              // LED亮度 (0-255)
    uint32_t update_interval_ms;     // 状态更新间隔 (ms)
    uint32_t metrics_interval_ms;    // 监控数据获取间隔 (ms)
} board_display_config_t;

/**
 * @brief 系统监控数据结构
 */
typedef struct {
    // 温度数据
    float n305_cpu_temp;            // N305 CPU温度 (°C)
    float jetson_cpu_temp;          // Jetson CPU温度 (°C)
    float jetson_gpu_temp;          // Jetson GPU温度 (°C)
      // 功率数据
    float jetson_power_mw;          // Jetson功率 (mW)
    
    // GPU数据 (已弃用，使用功率数据替代)
    // float jetson_gpu_usage;      // Jetson GPU使用率 (%)
    
    // 内存数据
    float jetson_memory_total;      // Jetson总内存 (MB)
    float jetson_memory_used;       // Jetson已用内存 (MB)
    float jetson_memory_usage;      // Jetson内存使用率 (%)
    
    // 数据有效性
    bool n305_data_valid;           // N305数据有效性
    bool jetson_data_valid;         // Jetson数据有效性
    uint32_t last_update_time;      // 上次更新时间
} system_metrics_t;

/**
 * @brief Board WS2812 显示状态
 */
typedef struct {
    board_display_mode_t current_mode;      // 当前显示模式
    board_display_mode_t previous_mode;     // 前一个显示模式
    uint32_t mode_change_count;             // 模式变化次数
    uint32_t time_in_current_mode;          // 在当前模式的时间 (ms)
    bool is_active;                         // 是否激活
    system_metrics_t metrics;               // 系统监控数据
    uint32_t system_uptime_ms;              // 系统运行时间
} board_display_status_t;

// ========== 核心接口 ==========

/**
 * @brief 初始化Board WS2812显示控制器
 * 
 * @param config 配置参数，NULL使用默认配置
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_board_ws2812_display_init(const board_display_config_t* config);

/**
 * @brief 启动Board WS2812显示控制器
 * 
 * 开始自动显示控制和监控数据获取
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_board_ws2812_display_start(void);

/**
 * @brief 停止Board WS2812显示控制器
 */
void bsp_board_ws2812_display_stop(void);

/**
 * @brief 手动设置显示模式
 * 
 * @param mode 显示模式
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_board_ws2812_display_set_mode(board_display_mode_t mode);

/**
 * @brief 恢复自动模式
 * 
 * 从手动模式恢复到自动状态监控模式
 */
void bsp_board_ws2812_display_resume_auto(void);

/**
 * @brief 获取显示状态
 * 
 * @param status 状态结构指针
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_board_ws2812_display_get_status(board_display_status_t* status);

/**
 * @brief 打印显示状态信息
 */
void bsp_board_ws2812_display_print_status(void);

// ========== 配置接口 ==========

/**
 * @brief 获取默认配置
 * 
 * @return board_display_config_t 默认配置结构
 */
board_display_config_t bsp_board_ws2812_display_get_default_config(void);

/**
 * @brief 设置自动模式开关
 * 
 * @param enabled 是否启用自动模式
 */
void bsp_board_ws2812_display_set_auto_mode(bool enabled);

/**
 * @brief 设置亮度
 * 
 * @param brightness 亮度 (0-255)
 */
void bsp_board_ws2812_display_set_brightness(uint8_t brightness);

/**
 * @brief 设置调试模式
 * 
 * @param debug_mode 是否启用调试模式
 */
void bsp_board_ws2812_display_set_debug_mode(bool debug_mode);

// ========== 手动控制接口 ==========

/**
 * @brief 手动设置颜色（固定颜色）
 * 
 * @param r 红色值 (0-255)
 * @param g 绿色值 (0-255)
 * @param b 蓝色值 (0-255)
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_board_ws2812_display_set_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief 手动设置呼吸灯效果
 * 
 * @param r 红色值 (0-255)
 * @param g 绿色值 (0-255)
 * @param b 蓝色值 (0-255)
 * @param speed 呼吸速度
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_board_ws2812_display_set_breath(uint8_t r, uint8_t g, uint8_t b, board_breath_speed_t speed);

/**
 * @brief 关闭Board WS2812显示
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_board_ws2812_display_off(void);

// ========== 监控数据接口 ==========

/**
 * @brief 获取当前系统监控数据
 * 
 * @param metrics 监控数据结构指针
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_board_ws2812_display_get_metrics(system_metrics_t* metrics);

/**
 * @brief 更新系统监控数据
 *  * 手动触发从Prometheus API获取最新的监控数据
 * 包括N305和Jetson的温度、功率、内存使用率等
 * 
 * @return ESP_OK 成功获取至少一组数据; ESP_FAIL 所有数据获取失败
 */
esp_err_t bsp_board_ws2812_display_update_metrics(void);

/**
 * @brief 网络连接诊断测试
 * 
 * 检查N305和Jetson的网络连接状态，并提供诊断建议
 * 用于排查网络连接问题和Prometheus数据获取失败
 */
void bsp_board_ws2812_display_test_network_connectivity(void);

/**
 * @brief 获取显示模式名称
 * 
 * @param mode 显示模式
 * @return const char* 模式名称字符串
 */
const char* bsp_board_ws2812_display_get_mode_name(board_display_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif // BSP_BOARD_WS2812_DISPLAY_H
