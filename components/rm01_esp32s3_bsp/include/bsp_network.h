#ifndef BSP_NETWORK_H
#define BSP_NETWORK_H

#include "esp_err.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化W5500以太网控制器（网络层实现）
 * 
 * 此函数配置W5500作为DHCP服务器，包括：
 * - SPI总线初始化
 * - GPIO配置和硬件复位
 * - TCP/IP协议栈初始化
 * - 事件处理器注册
 * - 静态IP配置
 * - DHCP服务器配置
 * - 以太网驱动启动
 * 
 * @param host SPI主机设备
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_w5500_network_init(spi_host_device_t host);

/**
 * @brief 初始化RTL8367交换机（网络层实现）
 * 
 * 此函数通过GPIO控制RTL8367交换机的复位：
 * - 配置复位GPIO
 * - 执行硬件复位序列
 * 
 * @return esp_err_t ESP_OK成功，其他值表示失败
 */
esp_err_t bsp_rtl8367_network_init(void);

#ifdef __cplusplus
}
#endif

#endif // BSP_NETWORK_H
