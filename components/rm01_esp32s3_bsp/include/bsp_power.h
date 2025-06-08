#ifndef BSP_POWER_H
#define BSP_POWER_H

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 电源管理引脚定义
#define BSP_ORIN_RESET_PIN       39
#define BSP_ORIN_POWER_PIN       3
#define BSP_LPN100_RESET_PIN     38
#define BSP_LPN100_POWER_PIN     46

// 电压检测引脚定义
#define BSP_MAIN_VOLTAGE_PIN     18  // GPIO18 -> ADC2_CHANNEL_7
#define BSP_AUX_12V_PIN          8   // GPIO8  -> ADC1_CHANNEL_7

// 电源芯片UART通信引脚定义
#define BSP_POWER_UART_PORT      UART_NUM_1
#define BSP_POWER_UART_TX_PIN    -1              // 不需要发送，设为-1
#define BSP_POWER_UART_RX_PIN    47              // GPIO47接收电源信息
#define BSP_POWER_UART_BAUDRATE  9600            // 波特率，根据电源芯片规格调整

// 分压比配置
#define VOLTAGE_RATIO  11.0  // 100K:10K分压

// 电源芯片数据结构
typedef struct {
    float voltage;          // 电压值 (V)
    float current;          // 电流值 (A)
    float power;           // 功率值 (W)
    uint32_t timestamp;    // 协商发生时的系统运行时间 (毫秒，基于esp_log_timestamp())
    bool valid;            // 数据有效性
} bsp_power_chip_data_t;

// ADC全局句柄声明
extern adc_oneshot_unit_handle_t adc1_handle;  // ADC1 for GPIO8
extern adc_oneshot_unit_handle_t adc2_handle;  // ADC2 for GPIO18
extern adc_cali_handle_t adc1_cali_handle;
extern adc_cali_handle_t adc2_cali_handle;

/**
 * @brief 初始化电源管理模块
 */
void bsp_power_init(void);

/**
 * @brief 初始化ORIN模块电源控制
 */
void bsp_orin_init(void);

/**
 * @brief 初始化LPN100模块电源控制
 */
void bsp_lpn100_init(void);

/**
 * @brief 初始化电压监测ADC
 */
void bsp_voltage_init(void);

/**
 * @brief 初始化电源芯片UART通信
 */
void bsp_power_chip_uart_init(void);

/**
 * @brief 停止电源芯片监控任务
 */
void bsp_power_chip_monitor_stop(void);

/**
 * @brief 触发电源芯片协商读取
 * @note 在上电或检测到电压变化时调用，读取电源芯片协商信息
 */
void bsp_trigger_power_chip_negotiation(void);

/**
 * @brief 设置电压变化监控阈值
 * @param main_threshold 主电源电压变化阈值(V)，默认3.0V
 * @param aux_threshold 辅助电源电压变化阈值(V)，默认3.0V
 */
void bsp_set_voltage_change_threshold(float main_threshold, float aux_threshold);

/**
 * @brief 获取电源芯片协商数据状态信息
 * @param is_valid 数据是否有效的指针
 * @param age_seconds 数据年龄（秒）的指针
 * @return ESP_OK成功，其他值失败
 */
esp_err_t bsp_get_power_chip_data_status(bool *is_valid, uint32_t *age_seconds);

/**
 * @brief 读取主电源电压
 * @return 电压值(V)，失败返回0.0
 */
float bsp_get_main_voltage(void);

/**
 * @brief 读取辅助12V电源电压
 * @return 电压值(V)，失败返回0.0
 */
float bsp_get_aux_12v_voltage(void);

/**
 * @brief 读取电源芯片数据
 * @param data 电源芯片数据结构指针
 * @return ESP_OK成功，其他值失败
 */
esp_err_t bsp_get_power_chip_data(bsp_power_chip_data_t *data);

/**
 * @brief 获取电源芯片最新协商数据
 * @note 数据只在电压变化超过阈值时重新协商，开机后的数据会一直有效直到下次电压变化
 * @return 电源芯片数据结构指针，如果无协商数据返回NULL
 */
const bsp_power_chip_data_t* bsp_get_latest_power_chip_data(void);

/**
 * @brief LPN100电源按钮控制
 * @note 按下电源按钮300ms，足够触发电源操作但不会清空BIOS
 *       LPN100的PWR_BTN按下超过5秒会清空BIOS
 */
void bsp_lpn100_power_toggle(void);

/**
 * @brief ORIN模块复位
 */
void bsp_orin_reset(void);

/**
 * @brief ORIN模块电源控制
 * @param enable true=开启电源，false=关闭电源
 */
void bsp_orin_power_control(bool enable);

/**
 * @brief LPN100模块复位
 */
void bsp_lpn100_reset(void);

/**
 * @brief 获取电源状态信息
 * @param main_voltage 主电源电压指针
 * @param aux_voltage 辅助电源电压指针
 * @return ESP_OK成功，其他值失败
 */
esp_err_t bsp_get_power_status(float *main_voltage, float *aux_voltage);

#ifdef __cplusplus
}
#endif

#endif // BSP_POWER_H
