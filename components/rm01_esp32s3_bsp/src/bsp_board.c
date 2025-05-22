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
#include "ping/ping.h"
#include "ping/ping_sock.h"
#include "esp_netif_ip_addr.h"
#include "esp_mac.h" // For MAC2STR and MACSTR
#include "color_calibration.h"
#include <math.h>
#include <inttypes.h> // 添加此头文件以支持 PRIuXX 宏
#include "sdmmc_cmd.h" // 用于SD卡命令和信息打印

static const char *TAG = "BSP";

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

// WS2812全局句柄
led_strip_handle_t ws2812_onboard_strip;
led_strip_handle_t ws2812_array_strip;

// ADC全局句柄
adc_oneshot_unit_handle_t adc1_handle;
adc_oneshot_unit_handle_t adc2_handle;
adc_cali_handle_t adc1_cali_handle;
adc_cali_handle_t adc2_cali_handle;

// 以太网句柄
esp_eth_handle_t *eth_handles = NULL;
esp_netif_t *eth_netif = NULL;

// 前置声明网络监控相关的函数
static void bsp_init_network_monitor(void);
static void network_monitor_task(void *pvParameters);
static void network_ping_success_callback(esp_ping_handle_t hdl, void *args);
static void network_ping_timeout_callback(esp_ping_handle_t hdl, void *args);
static void network_ping_end_callback(esp_ping_handle_t hdl, void *args);

// 定义一些LWIP相关变量
static ip_addr_t network_target_addr[NETWORK_TARGET_COUNT]; // 存储IP地址结构

// 网络监控的IP地址定义
#define COMPUTING_MODULE_IP "10.10.99.98"   // 算力模块
#define APPLICATION_MODULE_IP "10.10.99.99" // 应用模块
#define USER_HOST_IP "10.10.99.100"        // 用户主机
#define INTERNET_IP "8.8.8.8"              // 外网访问测试

// 全局网络监控目标数组
static network_target_t network_targets[NETWORK_TARGET_COUNT];

// 任务句柄
static TaskHandle_t network_monitor_task_handle = NULL;

// 获取网络监控目标数组（公开API）
const network_target_t* bsp_get_network_targets(void) {
    return network_targets;
}

void bsp_tf_card_init(void) {
    ESP_LOGI(TAG, "初始化 TF 卡...");
    
    // 主机配置
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_1;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    // 插槽配置
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = BSP_TF_CK_PIN;
    slot_config.cmd = BSP_TF_CMD_PIN;
    slot_config.d0 = BSP_TF_D0_PIN;
    slot_config.d1 = BSP_TF_D1_PIN;
    slot_config.d2 = BSP_TF_D2_PIN;
    slot_config.d3 = BSP_TF_D3_PIN;
    slot_config.width = 4; // 4-bit模式
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP; // 使用内部上拉

    // 初始化SDMMC主机
    esp_err_t ret = sdmmc_host_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SDMMC主机初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    
    // 初始化SDMMC插槽
    ret = sdmmc_host_init_slot(host.slot, &slot_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SDMMC插槽初始化失败: %s", esp_err_to_name(ret));
        sdmmc_host_deinit();
        return;
    }
    
    // SD卡信息结构体
    sdmmc_card_t* card = (sdmmc_card_t*)malloc(sizeof(sdmmc_card_t));
    if (card == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for SD card structure");
        sdmmc_host_deinit();
        return;
    }
      // 探测SD卡并获取卡信息
    ret = sdmmc_card_init(&host, card);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGW(TAG, "未检测到SD卡，请检查SD卡是否正确插入");
        } else {
            ESP_LOGE(TAG, "SD卡初始化失败: %s", esp_err_to_name(ret));
        }
        free(card); // Free allocated memory on failure
        sdmmc_host_deinit();
        return;
    }
    
    // 打印SD卡信息
    ESP_LOGI(TAG, "SD卡信息:");
    ESP_LOGI(TAG, "卡名称: %s", card->cid.name);
    ESP_LOGI(TAG, "卡类型: %s", (card->ocr & (1 << 30)) ? "SDHC/SDXC" : "SDSC"); // bit 30 表示 SDHC/SDXC 卡
    ESP_LOGI(TAG, "卡速度: %s", (card->csd.tr_speed > 25000000) ? "高速" : "标准速度");
    ESP_LOGI(TAG, "卡容量: %lluMB", ((uint64_t) card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
      // 保持卡挂载，不释放主机资源
    // 注意：要使用文件系统，需要在CMakeLists中添加fatfs组件，并使用esp_vfs_fat_sdmmc_mount函数
    
    // 读取并打印SD卡第一个扇区内容作为演示
    uint8_t *buffer = malloc(card->csd.sector_size);
    if (buffer) {
        ret = sdmmc_read_sectors(card, buffer, 0, 1);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "SD卡第一个扇区前16字节内容:");
            for (int i = 0; i < 16 && i < card->csd.sector_size; i++) {
                printf("%02x ", buffer[i]);
                if ((i + 1) % 8 == 0) {
                    printf("\n");
                }
            }
            printf("\n");
        } else {
            ESP_LOGE(TAG, "读取SD卡扇区失败: %s", esp_err_to_name(ret));
        }
        free(buffer);
    }
    
    ESP_LOGI(TAG, "TF卡初始化完成");
}

void bsp_ws2812_onboard_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = BSP_WS2812_ONBOARD_PIN,
        .max_leds = BSP_WS2812_ONBOARD_COUNT,
        // Removed invalid field assignment as 'led_strip_config_t' does not have 'led_pixel_format'
        .led_model = LED_MODEL_WS2812,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &ws2812_onboard_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WS2812 Onboard Init Failed: %s", esp_err_to_name(ret));
    }
}

void bsp_ws2812_array_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = BSP_WS2812_ARRAY_PIN,
        .max_leds = BSP_WS2812_ARRAY_COUNT,
        // Removed invalid field assignment as 'led_strip_config_t' does not have 'led_pixel_format'
        .led_model = LED_MODEL_WS2812,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &ws2812_array_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WS2812 Array Init Failed: %s", esp_err_to_name(ret));
    }
}

void bsp_ws2812_touch_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = BSP_WS2812_Touch_LED_PIN,
        .max_leds = BSP_WS2812_Touch_LED_COUNT,
        // Removed invalid field assignment as 'led_strip_config_t' does not have 'led_pixel_format'
        .led_model = LED_MODEL_WS2812,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &ws2812_array_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WS2812 Touch Init Failed: %s", esp_err_to_name(ret));
    }
}

void bsp_orin_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BSP_ORIN_RESET_PIN) | (1ULL << BSP_ORIN_POWER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(BSP_ORIN_RESET_PIN, 1); // 默认不复位
    gpio_set_level(BSP_ORIN_POWER_PIN, 0); // 持续拉低保持开启
}

void bsp_lpn100_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BSP_LPN100_RESET_PIN) | (1ULL << BSP_LPN100_POWER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(BSP_LPN100_RESET_PIN, 1);
    // 确保PWR_BTN引脚(GPIO46)为高电平，避免不小心触发BIOS清除
    // 该引脚连接到LPN100的PWR_BTN，拉低超过5秒会清空BIOS
    // 拉低100ms 
    gpio_set_level(BSP_LPN100_POWER_PIN, 0); // 按下电源按钮
    vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms
    gpio_set_level(BSP_LPN100_POWER_PIN, 1); // 默认高电平
    ESP_LOGI(TAG, "LPN100 PWR_BTN引脚初始化为高电平，避免清空BIOS");
}

void bsp_w5500_init(spi_host_device_t host) {
    // return value
    // esp_err_t ret = ESP_OK;
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
        return;
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

    // 12. 启动网络监控系统
    bsp_start_network_monitor();
}

void bsp_rtl8367_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BSP_RTL8367_RESET_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(BSP_RTL8367_RESET_PIN, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms
    // 复位交换机
    gpio_set_level(BSP_RTL8367_RESET_PIN, 1);
}

void bsp_voltage_init(void) {
    // ADC初始化 主电源
    adc_oneshot_unit_init_cfg_t init_config2 = {
        .unit_id = ADC_UNIT_2,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config2, &adc2_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC init failed: %s", esp_err_to_name(ret));
    }

    adc_oneshot_chan_cfg_t chan_config2 = {
        .atten = ADC_ATTEN_DB_12,  // 0~3.3V
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_oneshot_config_channel(adc2_handle, ADC_CHANNEL_7, &chan_config2); // GPIO18

    // 切换到线性拟合校准
    adc_cali_curve_fitting_config_t cali_config2 = {
        .unit_id = ADC_UNIT_2,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config2, &adc2_cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC calibration failed: %s", esp_err_to_name(ret));
    }


    // ADC初始化 辅助电源
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ret = adc_oneshot_new_unit(&init_config, &adc1_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC init failed: %s", esp_err_to_name(ret));
    }

    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_DB_12,  // 0~3.3V
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_7, &chan_config); // GPIO8
        // 切换到线性拟合校准
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ADC calibration failed: %s", esp_err_to_name(ret));
        }
}


// 读取电压函数
float bsp_get_main_voltage(void) {
    int raw;
    int voltage_mv;
    esp_err_t ret = adc_oneshot_read(adc2_handle, ADC_CHANNEL_7, &raw);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC read main failed: %s", esp_err_to_name(ret));
        return 0.0;
    }
    ret = adc_cali_raw_to_voltage(adc2_cali_handle, raw, &voltage_mv);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC cali main failed: %s", esp_err_to_name(ret));
        return 0.0;
    }
    float voltage = (voltage_mv / 1000.0) * VOLTAGE_RATIO;
    ESP_LOGI(TAG, "Main ADC raw: %d, cali: %d mV, final: %.2f V", raw, voltage_mv, voltage);
    return voltage;
}

float bsp_get_aux_12v_voltage(void) {
    int raw;
    int voltage_mv;
    esp_err_t ret = adc_oneshot_read(adc1_handle, ADC_CHANNEL_7, &raw); // GPIO8
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC read aux failed: %s", esp_err_to_name(ret));
        return 0.0;
    }
    ret = adc_cali_raw_to_voltage(adc1_cali_handle, raw, &voltage_mv);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC cali aux failed: %s", esp_err_to_name(ret));
        return 0.0;
    }
    float voltage = (voltage_mv / 1000.0) * VOLTAGE_RATIO;
    return voltage;
}

void bsp_board_init(void) {
    bsp_tf_card_init();
    bsp_ws2812_onboard_init();
    // bsp_ws2812_array_init(); 与 matrix_init();二选一
    bsp_orin_init();
    bsp_lpn100_init();
    bsp_w5500_init(SPI3_HOST);
    // bsp_rtl8367_init();
    bsp_voltage_init();
}

void bsp_ws2812_onboard_test(void) {
    for (int i = 0; i < BSP_WS2812_ONBOARD_COUNT; i++) {
        led_strip_set_pixel(ws2812_onboard_strip, i, 255, 0, 0); // 红色
        led_strip_refresh(ws2812_onboard_strip);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    // led_strip_refresh(ws2812_onboard_strip);
    // vTaskDelay(1000 / portTICK_PERIOD_MS);
    led_strip_clear(ws2812_onboard_strip);
}

void bsp_ws2812_array_test(void) {
    for (int i = 0; i < BSP_WS2812_ARRAY_COUNT; i++) {
        led_strip_set_pixel(ws2812_array_strip, i, 64, 64, 64); // 白色
        led_strip_refresh(ws2812_array_strip);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    // led_strip_refresh(ws2812_array_strip);
    // vTaskDelay(1000 / portTICK_PERIOD_MS);
    led_strip_clear(ws2812_array_strip);
}

void bsp_ws2812_touch_test(){
    // 测试触摸传感器上的ws2812灯珠
    for (int i = 0; i < BSP_WS2812_Touch_LED_COUNT; i++) {
        led_strip_set_pixel(ws2812_touch_strip, i, 0, 0, 255); // 蓝色
    }
    led_strip_refresh(ws2812_touch_strip);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    led_strip_clear(ws2812_touch_strip);
    // 关闭灯珠
}

void bsp_lpn100_power_toggle(void) {
    // 按下电源按钮不超过300毫秒，足够触发电源操作但不会清空BIOS
    // LP N100的PWR_BTN按下超过5秒会清空BIOS
    ESP_LOGI(TAG, "LPN100电源按钮按下，时间控制在300ms以内，避免清空BIOS");
    gpio_set_level(BSP_LPN100_POWER_PIN, 0);
    vTaskDelay(300 / portTICK_PERIOD_MS); // 300ms，安全时间
    gpio_set_level(BSP_LPN100_POWER_PIN, 1);
}

// 网络监控的 ping 回调函数
static void network_ping_success_callback(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    uint8_t index = *((uint8_t*)args);
    
    // 获取ping结果信息
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));    // 更新目标状态
    if (index < NETWORK_TARGET_COUNT) {
        network_targets[index].last_response_time = elapsed_time;
        network_targets[index].packets_received++;
        
        // 简单计算平均响应时间
        if (network_targets[index].average_response_time == 0) {
            network_targets[index].average_response_time = elapsed_time;
        } else {
            network_targets[index].average_response_time = 
                (network_targets[index].average_response_time + elapsed_time) / 2;
        }
            
        // 更新状态，如果之前是断开状态则打印信息
        network_status_t prev_status = network_targets[index].status;
        network_targets[index].status = NETWORK_STATUS_UP;
        
        if (prev_status != NETWORK_STATUS_UP) {
            ESP_LOGI(TAG, "网络状态变化: %s 【已连接】, 响应时间=%" PRIu32 "ms",
                     network_targets[index].ip, elapsed_time);
        }
          ESP_LOGD(TAG, "Ping成功: %s, 序列号=%" PRIu16 ", 时间=%" PRIu32 "ms", 
                 network_targets[index].ip, seqno, elapsed_time);
    }
}

// 网络ping超时回调
static void network_ping_timeout_callback(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    uint8_t index = *((uint8_t*)args);
    
    // 获取ping超时信息
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));    // 更新目标状态
    if (index < NETWORK_TARGET_COUNT) {
        // 连续超时几次后才认为断开，这里简单判断如果收包率低于30%就认为断开
        network_targets[index].packets_sent++;
        float loss_rate = 0;
        
        if (network_targets[index].packets_sent > 0) {
            loss_rate = 100.0f * (1.0f - (float)network_targets[index].packets_received / 
                                  (float)network_targets[index].packets_sent);
        }
        
        network_targets[index].loss_rate = loss_rate;
        
        if (loss_rate > 70.0f && network_targets[index].packets_sent >= 5) {
            // 更新状态，如果之前是连接状态则打印信息
            network_status_t prev_status = network_targets[index].status;
            network_targets[index].status = NETWORK_STATUS_DOWN;
            
            if (prev_status != NETWORK_STATUS_DOWN) {
                ESP_LOGI(TAG, "网络状态变化: %s 【已断开】, 丢包率=%.1f%%", 
                         network_targets[index].ip, loss_rate);
            }
        }
          ESP_LOGD(TAG, "Ping超时: %s, 序列号=%" PRIu16 ", 丢包率=%.1f%%", 
                 network_targets[index].ip, seqno, loss_rate);
    }
}

// 网络ping会话完成回调
static void network_ping_end_callback(esp_ping_handle_t hdl, void *args)
{
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;
    uint8_t index = *((uint8_t*)args);
    
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    float loss_rate = 0;
    
    if (transmitted > 0) {
        loss_rate = (transmitted - received) * 100.0f / transmitted;
    }    if (index < NETWORK_TARGET_COUNT) {
        network_targets[index].packets_sent = transmitted;
        network_targets[index].packets_received = received;
        network_targets[index].loss_rate = loss_rate;
        
        ESP_LOGI(TAG, "Ping会话结束: %s, 发送=%" PRIu32 ", 接收=%" PRIu32 ", 丢包率=%.1f%%, 总时间=%" PRIu32 "ms",
               network_targets[index].ip, transmitted, received, loss_rate, total_time_ms);
            
        // 重启ping会话，实现持续监控
        esp_ping_stop((esp_ping_handle_t)network_targets[index].ping_handle);
        esp_ping_delete_session((esp_ping_handle_t)network_targets[index].ping_handle);
        network_targets[index].ping_handle = NULL;
        
        // 稍微延迟一点再重启，避免过于频繁
        vTaskDelay(pdMS_TO_TICKS(1000));
          // 重新启动ping
        esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
        ping_config.target_addr = network_target_addr[index];      // 目标IP地址
        ping_config.count = 5;                      // 发送10个ping包
        ping_config.interval_ms = 1000;              // 每秒发送一次
        ping_config.timeout_ms = 1000;               // 1秒超时
        ping_config.task_stack_size = 4096;          // ping任务的栈大小
        ping_config.task_prio = 1;                   // 任务优先级
        ping_config.tos = 0;                         // 服务类型
        ping_config.data_size = 64;                  // ICMP包数据大小
        
        esp_ping_callbacks_t cbs = {
            .on_ping_success = network_ping_success_callback,
            .on_ping_timeout = network_ping_timeout_callback,
            .on_ping_end = network_ping_end_callback,
            .cb_args = &network_targets[index].index
        };
        
        esp_ping_handle_t ping_handle = NULL;
        esp_err_t ret = esp_ping_new_session(&ping_config, &cbs, &ping_handle);
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "创建ping会话失败: %s, 目标=%s", esp_err_to_name(ret), network_targets[index].ip);
        } else {
            // 保存ping句柄到网络目标结构体中
            network_targets[index].ping_handle = ping_handle;
            
            ret = esp_ping_start(ping_handle);
            
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "启动ping会话失败: %s, 目标=%s", esp_err_to_name(ret), network_targets[index].ip);
                esp_ping_delete_session(ping_handle);
                network_targets[index].ping_handle = NULL;
            }
        }
    }
}

// 保留旧的接口以兼容代码
void bsp_start_ping_test(void)
{
    // 转而调用网络监控系统
    bsp_start_network_monitor();
}

// 启动网络监控
void bsp_start_network_monitor(void)
{
    ESP_LOGI(TAG, "启动网络监控系统");
    
    // 初始化网络监控
    bsp_init_network_monitor();
    
    // 创建监控任务
    if (network_monitor_task_handle == NULL) {
        xTaskCreate(network_monitor_task, "network_monitor", 4096, NULL, 5, &network_monitor_task_handle);
        ESP_LOGI(TAG, "网络监控任务已启动");
    } else {
        ESP_LOGW(TAG, "网络监控任务已在运行");
    }
}

// 停止网络监控
void bsp_stop_network_monitor(void)
{
    ESP_LOGI(TAG, "停止网络监控系统");
    
    // 停止任务
    if (network_monitor_task_handle != NULL) {
        // 停止所有ping会话
        for (int i = 0; i < NETWORK_TARGET_COUNT; i++) {
            if (network_targets[i].ping_handle != NULL) {
                esp_ping_stop((esp_ping_handle_t)network_targets[i].ping_handle);
                esp_ping_delete_session((esp_ping_handle_t)network_targets[i].ping_handle);
                network_targets[i].ping_handle = NULL;
            }
        }
        
        // 删除任务
        vTaskDelete(network_monitor_task_handle);
        network_monitor_task_handle = NULL;
        ESP_LOGI(TAG, "网络监控任务已停止");
    }
}

// 获取网络状态
void bsp_get_network_status(void)
{
    ESP_LOGI(TAG, "网络状态报告:");
    
    for (int i = 0; i < NETWORK_TARGET_COUNT; i++) {
        const char *status_str = "未知";
        if (network_targets[i].status == NETWORK_STATUS_UP) {
            status_str = "连接";
        } else if (network_targets[i].status == NETWORK_STATUS_DOWN) {
            status_str = "断开";
        }
        
        ESP_LOGI(TAG, "目标 %s: 状态=%s, 响应时间=%" PRIu32 "ms, 丢包率=%.1f%%",
                 network_targets[i].ip, status_str, 
                 network_targets[i].last_response_time,
                 network_targets[i].loss_rate);
    }
}

// 初始化网络监控系统
static void bsp_init_network_monitor(void)
{
    // 配置目标IP和状态
    strncpy(network_targets[0].ip, COMPUTING_MODULE_IP, sizeof(network_targets[0].ip));
    strncpy(network_targets[1].ip, APPLICATION_MODULE_IP, sizeof(network_targets[1].ip));
    strncpy(network_targets[2].ip, USER_HOST_IP, sizeof(network_targets[2].ip));
    strncpy(network_targets[3].ip, INTERNET_IP, sizeof(network_targets[3].ip));
    
    // 配置目标名称
    strncpy(network_targets[0].name, "算力模块", sizeof(network_targets[0].name));
    strncpy(network_targets[1].name, "应用模块", sizeof(network_targets[1].name));
    strncpy(network_targets[2].name, "用户主机", sizeof(network_targets[2].name));
    strncpy(network_targets[3].name, "互联网", sizeof(network_targets[3].name));
    
    // 设置索引和初始状态
    for (int i = 0; i < NETWORK_TARGET_COUNT; i++) {
        network_targets[i].index = i;
        network_targets[i].status = NETWORK_STATUS_UNKNOWN;
        network_targets[i].prev_status = NETWORK_STATUS_UNKNOWN;
        
        // 将IP字符串转换为IP地址结构
        ipaddr_aton(network_targets[i].ip, &network_target_addr[i]);
    }
    
    ESP_LOGI(TAG, "网络监控系统初始化完成, 监控 %d 个目标", NETWORK_TARGET_COUNT);
}

// 网络监控任务函数
static void network_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "网络监控任务开始运行");
    
    // 启动对所有目标的ping
    for (int i = 0; i < NETWORK_TARGET_COUNT; i++) {
        esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
        
        ping_config.target_addr = network_target_addr[i];      // 目标IP地址
        ping_config.count = 5;                      // 发送10个ping包
        ping_config.interval_ms = 1000;              // 每秒发送一次
        ping_config.timeout_ms = 1000;               // 1秒超时
        ping_config.task_stack_size = 4096;          // ping任务的栈大小
        ping_config.task_prio = 1;                   // 任务优先级
        ping_config.tos = 0;                         // 服务类型
        ping_config.data_size = 64;                  // ICMP包数据大小
        
        esp_ping_callbacks_t cbs = {
            .on_ping_success = network_ping_success_callback,
            .on_ping_timeout = network_ping_timeout_callback,
            .on_ping_end = network_ping_end_callback,
            .cb_args = &network_targets[i].index
        };
        
        ESP_LOGI(TAG, "开始ping测试，目标: %s (索引 %d)", network_targets[i].ip, i);          esp_ping_handle_t ping_handle = NULL;
        esp_err_t ret = esp_ping_new_session(&ping_config, &cbs, &ping_handle);
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "创建ping会话失败: %s, 目标=%s", esp_err_to_name(ret), network_targets[i].ip);
        } else {
            // 保存ping句柄到网络目标结构体中
            network_targets[i].ping_handle = ping_handle;
            
            ret = esp_ping_start(ping_handle);
            
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "启动ping会话失败: %s, 目标=%s", esp_err_to_name(ret), network_targets[i].ip);
                esp_ping_delete_session(ping_handle);
                network_targets[i].ping_handle = NULL;
            }
        }
        
        // 稍微间隔一下，避免同时发起多个ping导致网络压力
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // 任务主循环
    while (1) {
        // 定期打印汇总信息
        vTaskDelay(pdMS_TO_TICKS(30000)); // 每30秒打印一次汇总信息
        bsp_get_network_status();
    }
}