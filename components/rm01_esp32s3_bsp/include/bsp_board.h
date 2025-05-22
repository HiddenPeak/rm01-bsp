#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_phy.h"
#include "esp_netif.h"
#include "driver/sdmmc_host.h"
#include "led_strip.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "ethernet_init.h"

// 网络监控相关定义
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

// touchpad and status led(ws2812)
#define BSP_TOUCHPAD_PIN 14
#define BSP_WS2812_Touch_LED_PIN 45
#define BSP_WS2812_Touch_LED_COUNT 1

// Onboard WS2812
#define BSP_WS2812_ONBOARD_PIN   42
#define BSP_WS2812_ONBOARD_COUNT 28

// WS2812 Array (32x32)
#define BSP_WS2812_ARRAY_PIN     9
#define BSP_WS2812_ARRAY_COUNT   (32 * 32)

#define WIDTH 32             // Matrix width
#define HEIGHT 32            // Matrix height
#define NUM_LEDS (WIDTH * HEIGHT) // Total LEDs: 1024

// Flash animation parameters
#define FLASH_WIDTH 2
#define ANIMATION_SPEED 1

// 白点参考值宏定义（与 CALIB_WHITE 保持一致）
#define WHITE_R 42
#define WHITE_G 28
#define WHITE_B 19

// Orin Control
#define BSP_ORIN_RESET_PIN       1
#define BSP_ORIN_POWER_PIN       3

// LP N100 Control
#define BSP_LPN100_RESET_PIN     2
#define BSP_LPN100_POWER_PIN     46

// W5500 (Ethernet)
#define BSP_W5500_RST_PIN        39
#define BSP_W5500_INT_PIN        38
#define BSP_W5500_MISO_PIN       13
#define BSP_W5500_SCLK_PIN       12
#define BSP_W5500_MOSI_PIN       11
#define BSP_W5500_CS_PIN         10

// RTL8367 (Switch)
#define BSP_RTL8367_RESET_PIN    17

// Voltage Detection (ADC) 实际未使用该定义，但用于标注IO口占用情况
#define BSP_MAIN_VOLTAGE_PIN     18
#define BSP_AUX_12V_PIN          8

// 分压比
#define VOLTAGE_RATIO  11.0  // 100K:10K分压

// TF Card (SDMMC 4-bit)
#define BSP_TF_D0_PIN    4
#define BSP_TF_D1_PIN    5
#define BSP_TF_D2_PIN    6
#define BSP_TF_D3_PIN    7
#define BSP_TF_CMD_PIN   15
#define BSP_TF_CK_PIN    16

// WS2812全局句柄
extern led_strip_handle_t ws2812_onboard_strip;
extern led_strip_handle_t ws2812_array_strip;
extern led_strip_handle_t ws2812_touch_strip;

// ADC全局句柄
extern adc_oneshot_unit_handle_t adc1_handle;  // ADC1 for GPIO8
extern adc_oneshot_unit_handle_t adc2_handle;  // ADC2 for GPIO18
extern adc_cali_handle_t adc1_cali_handle;
extern adc_cali_handle_t adc2_cali_handle;

// 主初始化函数
void bsp_board_init(void);

// 子模块初始化
void bsp_tf_card_init(void);
void bsp_ws2812_onboard_init(void);
void bsp_ws2812_array_init(void);
void bsp_orin_init(void);
void bsp_lpn100_init(void);
void bsp_w5500_init(spi_host_device_t host);
void bsp_rtl8367_init(void);
void bsp_voltage_init(void);


// WS2812测试函数
void bsp_ws2812_onboard_test(void);
void bsp_ws2812_array_test(void);
void bsp_ws2812_touch_test(void);

// LP N100电源控制
void bsp_lpn100_power_toggle(void);

// 电压读取函数
float bsp_get_main_voltage(void);
float bsp_get_aux_12v_voltage(void);

// 网络监控函数
void bsp_start_ping_test(void); // 保留旧的接口
void bsp_start_network_monitor(void); // 启动网络监控
void bsp_stop_network_monitor(void);  // 停止网络监控
void bsp_get_network_status(void);    // 获取网络状态
const network_target_t* bsp_get_network_targets(void); // 获取网络监控目标数组

#endif