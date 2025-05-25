#include "bsp_network.h"
#include "bsp_board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "ethernet_init.h"
#include "lwip/ip4_addr.h"
#include "esp_netif_types.h" // For dhcps_lease_t
#include "dhcpserver/dhcpserver.h"
#include "dhcpserver/dhcpserver_options.h"
#include "esp_mac.h" // For MAC2STR and MACSTR
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_phy.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "BSP_NETWORK";

// 以太网句柄
static esp_eth_handle_t *eth_handles = NULL;
static esp_netif_t *eth_netif = NULL;

// DHCP事件处理函数
static void dhcp_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_ETH_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
            ESP_LOGI(TAG, "W5500 获取到IP地址: " IPSTR ", 掩码: " IPSTR ", 网关: " IPSTR,
                     IP2STR(&event->ip_info.ip),
                     IP2STR(&event->ip_info.netmask),
                     IP2STR(&event->ip_info.gw));
        }
        else if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
            ESP_LOGI(TAG, "设备获取到IP地址: " IPSTR ", 掩码: " IPSTR ", 网关: " IPSTR,
                     IP2STR(&event->ip_info.ip),
                     IP2STR(&event->ip_info.netmask),
                     IP2STR(&event->ip_info.gw));
        }
    }
    else if (event_base == ETH_EVENT) {
        switch(event_id) {
            case ETHERNET_EVENT_CONNECTED:
                ESP_LOGI(TAG, "以太网连接已建立");
                break;
            case ETHERNET_EVENT_DISCONNECTED:
                ESP_LOGI(TAG, "以太网连接已断开");
                break;
            case ETHERNET_EVENT_START:
                ESP_LOGI(TAG, "以太网已启动");
                break;
            case ETHERNET_EVENT_STOP:
                ESP_LOGI(TAG, "以太网已停止");
                break;
            default:
                break;
        }
    }
}

// DHCP服务器IP分配事件处理函数
static void dhcps_lease_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED) {
        ip_event_ap_staipassigned_t *event = (ip_event_ap_staipassigned_t *)event_data;
        ESP_LOGI(TAG, "DHCP服务器分配IP: " IPSTR " 给设备 (MAC: " MACSTR ")",
                 IP2STR(&event->ip),
                 MAC2STR(event->mac));
    }
}

esp_err_t bsp_w5500_network_init(spi_host_device_t host) {
    // SPI总线配置
    spi_bus_config_t bus_cfg = {
        .miso_io_num = BSP_W5500_MISO_PIN,
        .mosi_io_num = BSP_W5500_MOSI_PIN,
        .sclk_io_num = BSP_W5500_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_bus_initialize(host, &bus_cfg, SPI_DMA_CH_AUTO);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BSP_W5500_CS_PIN) | (1ULL << BSP_W5500_RST_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(BSP_W5500_CS_PIN, 1);
    gpio_set_level(BSP_W5500_RST_PIN, 1);

    io_conf.pin_bit_mask = (1ULL << BSP_W5500_INT_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    
    // W5500 硬件复位
    gpio_set_level(BSP_W5500_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));  // 等待至少10ms
    gpio_set_level(BSP_W5500_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));  // 等待W5500完成初始化
    
    // 1. 初始化TCP/IP协议栈（必须先于ethernet_init_all调用）,返回值为ESP_OK表示成功
    ESP_ERROR_CHECK(esp_netif_init());
    
    // 2. 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // 注册事件处理函数
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &dhcp_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &dhcp_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &dhcps_lease_event_handler, NULL));
    
    // 3. 设置静态IP地址
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 10, 10, 99, 97);        // 设置W5500的IP为10.10.99.97
    IP4_ADDR(&ip_info.gw, 10, 10, 99, 100);        // 网关为LP Mu的IP地址99 用户主机ip 100
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0); // 子网掩码为255.255.255.0

    // 4. 创建并配置默认的以太网网络接口
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    
    // 5. 修改默认网络接口配置，使其成为DHCP服务器
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    // 修改名称以标识这是DHCP服务器接口
    esp_netif_config.if_desc = "w5500-dhcps";
    esp_netif_config.route_prio = 50;
    // 将标志设置为DHCP服务器和自动启动（必选，否则无法提供服务）
    esp_netif_config.flags = ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP ;
    // 设置默认IP信息
    esp_netif_config.ip_info = &ip_info;
    cfg.base = &esp_netif_config;
    
    // 6. 创建网络接口
    eth_netif = esp_netif_new(&cfg);
    
    // 7. 使用ethernet_init组件初始化以太网
    uint8_t eth_cnt = 0;
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_cnt));
    
    if (eth_cnt == 0) {
        ESP_LOGE(TAG, "No Ethernet devices found");
        return ESP_FAIL;
    }
    
    // 8. 配置DHCP服务器
    dhcps_lease_t dhcps_lease;
    dhcps_lease.enable = true;
    IP4_ADDR(&dhcps_lease.start_ip, 10, 10, 99, 100);    // DHCP起始地址
    IP4_ADDR(&dhcps_lease.end_ip, 10, 10, 99, 101);     // DHCP结束地址
    
    // 应用DHCP服务器租约配置 - 修正DHCP选项
    ESP_ERROR_CHECK(esp_netif_dhcps_option(
        eth_netif,
        ESP_NETIF_OP_SET,
        ESP_NETIF_REQUESTED_IP_ADDRESS,  // 使用正确的DHCP服务器选项
        &dhcps_lease,
        sizeof(dhcps_lease)));
        
    // 配置DNS服务器（将自身设为DNS服务器）
    esp_netif_dns_info_t dns;
    IP4_ADDR(&dns.ip.u_addr.ip4, 8, 8, 8, 8); // 使用Google DNS作为示例
    ESP_ERROR_CHECK(esp_netif_set_dns_info(eth_netif, ESP_NETIF_DNS_MAIN, &dns));

    // 启用 DHCP 服务器的DNS选项
    // 注意：在当前ESP-IDF版本中，无法直接获取dhcps_t*句柄
    // 通过esp_netif_dhcps_option设置DNS选项
    uint8_t dns_offer = 1; // 启用DNS服务器选项
    ESP_ERROR_CHECK(esp_netif_dhcps_option(
        eth_netif,
        ESP_NETIF_OP_SET,
        ESP_NETIF_DOMAIN_NAME_SERVER,
        &dns_offer,
        sizeof(dns_offer)));

    // 确保这是一个有效的URL，包含协议、IP和可能的路径
    const char *uri = "http://10.10.99.97/index.html";
    
    // 设置捕获门户URI
    ESP_ERROR_CHECK(esp_netif_dhcps_option(
        eth_netif,
        ESP_NETIF_OP_SET,
        ESP_NETIF_CAPTIVEPORTAL_URI,  // 这对应DHCP选项114
        (void *)uri,
        strlen(uri) + 1));  // +1 包含null终止符
        
    // 9. 启动DHCP服务器
    ESP_ERROR_CHECK(esp_netif_dhcps_start(eth_netif));
    
    // 10. 连接网络接口和以太网驱动程序
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));
    
    // 11. 启动以太网驱动程序
    ESP_ERROR_CHECK(esp_eth_start(eth_handles[0]));
    ESP_LOGI(TAG, "W5500 initialized as DHCP server with IP: 10.10.99.97");

    return ESP_OK;
}

esp_err_t bsp_rtl8367_network_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BSP_RTL8367_RESET_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RTL8367 GPIO配置失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    gpio_set_level(BSP_RTL8367_RESET_PIN, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms
    // 复位交换机
    gpio_set_level(BSP_RTL8367_RESET_PIN, 1);
    
    ESP_LOGI(TAG, "RTL8367 交换机已复位");
    return ESP_OK;
}
