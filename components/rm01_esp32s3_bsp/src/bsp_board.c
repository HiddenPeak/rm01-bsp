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
#include "esp_netif_ip_addr.h"
#include "esp_mac.h" // For MAC2STR and MACSTR
#include "color_calibration.h"
#include <math.h>
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
    IP4_ADDR(&dhcps_lease.end_ip, 10, 10, 99, 120);     // DHCP结束地址
    
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
        
    // 9. 启动DHCP服务器
    ESP_ERROR_CHECK(esp_netif_dhcps_start(eth_netif));
    
    // 10. 连接网络接口和以太网驱动程序
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));
    
    // 11. 启动以太网驱动程序
    ESP_ERROR_CHECK(esp_eth_start(eth_handles[0]));
    
    ESP_LOGI(TAG, "W5500 initialized as DHCP server with IP: 10.10.99.97");
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