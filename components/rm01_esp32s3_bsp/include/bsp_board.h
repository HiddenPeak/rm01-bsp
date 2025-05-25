#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_phy.h"
#include "esp_netif.h"
#include "driver/sdmmc_host.h"
#include "ethernet_init.h"
#include "bsp_webserver.h" // 添加Webserver头文件引用
#include "network_monitor.h" // 引入网络监控头文件
#include "bsp_power.h" // 引入电源管理头文件
#include "bsp_ws2812.h" // 引入WS2812模块头文件

// touchpad
#define BSP_TOUCHPAD_PIN 14

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

// W5500 (Ethernet)
#define BSP_W5500_RST_PIN        39
#define BSP_W5500_INT_PIN        38
#define BSP_W5500_MISO_PIN       13
#define BSP_W5500_SCLK_PIN       12
#define BSP_W5500_MOSI_PIN       11
#define BSP_W5500_CS_PIN         10

// RTL8367 (Switch)
#define BSP_RTL8367_RESET_PIN    17

// TF Card (SDMMC 4-bit)
#define BSP_TF_D0_PIN    4
#define BSP_TF_D1_PIN    5
#define BSP_TF_D2_PIN    6
#define BSP_TF_D3_PIN    7
#define BSP_TF_CMD_PIN   15
#define BSP_TF_CK_PIN    16

// 主初始化函数
void bsp_board_init(void);

// 子模块初始化
void bsp_w5500_init(spi_host_device_t host);
void bsp_rtl8367_init(void);

// 保留旧的网络监控接口（重定向到network_monitor模块）
void bsp_start_ping_test(void); // 保留旧的接口，重定向到nm_start_network_monitor

#endif