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
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_1;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = BSP_TF_CK_PIN;
    slot_config.cmd = BSP_TF_CMD_PIN;
    slot_config.d0 = BSP_TF_D0_PIN;
    slot_config.d1 = BSP_TF_D1_PIN;
    slot_config.d2 = BSP_TF_D2_PIN;
    slot_config.d3 = BSP_TF_D3_PIN;
    slot_config.width = 4;

    esp_err_t ret = sdmmc_host_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SDMMC Host Init Failed: %s", esp_err_to_name(ret));
        return;
    }
    ret = sdmmc_host_init_slot(host.slot, &slot_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SDMMC Slot Init Failed: %s", esp_err_to_name(ret));
    }
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


// 颜色校准基准值定义（与头文件严格对应）
const white_point_t CALIB_WHITE = {
    .r = 42,  // 您的理想值
    .g = 28,
    .b = 19
};

// RGB structure
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGB;

// HSL structure
typedef struct {
    float h; // Hue (0-360)
    float s; // Saturation (0-1)
    float l; // Lightness (0-1)
} HSL;


// Color correction function
RGB color_correct(uint8_t input_r, uint8_t input_g, uint8_t input_b) {
    RGB result;
    const RGB black = {0, 0, 0};
    const RGB min_white = {5, 4, 3};
    const RGB max_white = {168, 112, 76};
    const float input_min = 5.0f;
    const float input_max = 255.0f;

    const float r_slope = (float)(max_white.r - min_white.r) / (input_max - input_min);
    const float g_slope = (float)(max_white.g - min_white.g) / (input_max - input_min);
    const float b_slope = (float)(max_white.b - min_white.b) / (input_max - input_min);

    const float r_intercept = min_white.r - r_slope * input_min;
    const float g_intercept = min_white.g - g_slope * input_min;
    const float b_intercept = min_white.b - b_slope * input_min;

    float temp_r, temp_g, temp_b;
    if (input_r <= 5 && input_g <= 5 && input_b <= 5) {
        temp_r = (float)input_r * (min_white.r / input_min);
        temp_g = (float)input_g * (min_white.g / input_min);
        temp_b = (float)input_b * (min_white.b / input_min);
    } else {
        temp_r = (float)input_r * r_slope + r_intercept;
        temp_g = (float)input_g * g_slope + g_intercept;
        temp_b = (float)input_b * b_slope + b_intercept;
    }

    temp_r = (temp_r < black.r) ? black.r : (temp_r > max_white.r) ? max_white.r : temp_r;
    temp_g = (temp_g < black.g) ? black.g : (temp_g > max_white.g) ? max_white.g : temp_g;
    temp_b = (temp_b < black.b) ? black.b : (temp_b > max_white.b) ? max_white.b : temp_b;

    result.r = (uint8_t)(temp_r + 0.5f);
    result.g = (uint8_t)(temp_g + 0.5f);
    result.b = (uint8_t)(temp_b + 0.5f);
    
    return result;
}

// RGB to HSL conversion
HSL rgb_to_hsl(uint8_t r, uint8_t g, uint8_t b) {
    HSL hsl;
    float r_norm = r / 255.0f;
    float g_norm = g / 255.0f;
    float b_norm = b / 255.0f;

    float max = fmaxf(fmaxf(r_norm, g_norm), b_norm);
    float min = fminf(fminf(r_norm, g_norm), b_norm);
    float delta = max - min;

    // Lightness
    hsl.l = (max + min) / 2.0f;

    // Saturation
    if (delta == 0.0f) {
        hsl.s = 0.0f;
        hsl.h = 0.0f; // Undefined, but set to 0
    } else {
        hsl.s = (hsl.l > 0.5f) ? (delta / (2.0f - max - min)) : (delta / (max + min));

        // Hue
        if (max == r_norm) {
            hsl.h = (g_norm - b_norm) / delta + (g_norm < b_norm ? 6.0f : 0.0f);
        } else if (max == g_norm) {
            hsl.h = (b_norm - r_norm) / delta + 2.0f;
        } else {
            hsl.h = (r_norm - g_norm) / delta + 4.0f;
        }
        hsl.h *= 60.0f;
    }

    return hsl;
}

// HSL to RGB conversion
RGB hsl_to_rgb(float h, float s, float l) {
    RGB rgb;
    float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = l - c / 2.0f;

    float r, g, b;
    if (h >= 0.0f && h < 60.0f) {
        r = c; g = x; b = 0.0f;
    } else if (h >= 60.0f && h < 120.0f) {
        r = x; g = c; b = 0.0f;
    } else if (h >= 120.0f && h < 180.0f) {
        r = 0.0f; g = c; b = x;
    } else if (h >= 180.0f && h < 240.0f) {
        r = 0.0f; g = x; b = c;
    } else if (h >= 240.0f && h < 300.0f) {
        r = x; g = 0.0f; b = c;
    } else {
        r = c; g = 0.0f; b = x;
    }

    rgb.r = (uint8_t)((r + m) * 255.0f + 0.5f);
    rgb.g = (uint8_t)((g + m) * 255.0f + 0.5f);
    rgb.b = (uint8_t)((b + m) * 255.0f + 0.5f);

    return rgb;
}

// Adjust brightness and saturation
RGB adjust_brightness_saturation(uint8_t r, uint8_t g, uint8_t b) {
    // Step 1: Reduce brightness by 52.4% (0.595 * 0.8)
    float brightness_factor = 0.476f; // Total brightness reduction: 1 - 0.524
    float adjusted_r = r * brightness_factor;
    float adjusted_g = g * brightness_factor;
    float adjusted_b = b * brightness_factor;

    // Clamp to valid range
    adjusted_r = (adjusted_r < 0) ? 0 : (adjusted_r > 255) ? 255 : adjusted_r;
    adjusted_g = (adjusted_g < 0) ? 0 : (adjusted_g > 255) ? 255 : adjusted_g;
    adjusted_b = (adjusted_b < 0) ? 0 : (adjusted_b > 255) ? 255 : adjusted_b;

    // Step 2: Convert to HSL and increase saturation by 52.0875% (1.3225 * 1.15)
    HSL hsl = rgb_to_hsl((uint8_t)adjusted_r, (uint8_t)adjusted_g, (uint8_t)adjusted_b);
    hsl.s *= 1.520875f; // Total saturation increase: 1 + 0.520875
    hsl.s = (hsl.s > 1.0f) ? 1.0f : hsl.s; // Clamp saturation to 1.0

    // Step 3: Convert back to RGB
    return hsl_to_rgb(hsl.h, hsl.s, hsl.l);
}

// Global variables
static led_strip_handle_t led_strip;
static uint8_t grid[HEIGHT][WIDTH][3]; // RGB grid
static int flash_position = 0; // Initial position of flash (starting at 0,0)
static uint8_t mask[HEIGHT][WIDTH]; // Mask for which pixels should be illuminated
static uint8_t original_colors[HEIGHT][WIDTH][3]; // Original colors for each pixel

// Initialize LED and matrix
void matrix_init() {
    led_strip_config_t strip_config = {
        .strip_gpio_num = BSP_WS2812_ARRAY_PIN,
        .max_leds = NUM_LEDS,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    
    led_strip_clear(led_strip);
    memset(grid, 0, sizeof(grid));
    memset(mask, 0, sizeof(mask));
    memset(original_colors, 0, sizeof(original_colors));
    
    // Initialize mask and original colors with given points
    // Points list with original colors

    // Store only the points in the mask (1 means it's a point)
    mask[8][11] = 1;
    mask[8][22] = 1;
    mask[8][23] = 1;
    mask[8][24] = 1;
    mask[9][9] = 1;
    mask[9][10] = 1;
    mask[9][11] = 1;
    mask[9][12] = 1;
    mask[9][13] = 1;
    mask[9][20] = 1;
    mask[9][21] = 1;
    mask[9][22] = 1;
    mask[9][23] = 1;
    mask[9][24] = 1;
    mask[9][25] = 1;
    mask[10][8] = 1;
    mask[10][9] = 1;
    mask[10][10] = 1;
    mask[10][11] = 1;
    mask[10][12] = 1;
    mask[10][13] = 1;
    mask[10][14] = 1;
    mask[10][15] = 1;
    mask[10][18] = 1;
    mask[10][19] = 1;
    mask[10][20] = 1;
    mask[10][21] = 1;
    mask[10][22] = 1;
    mask[10][23] = 1;
    mask[11][8] = 1;
    mask[11][9] = 1;
    mask[11][10] = 1;
    mask[11][11] = 1;
    mask[11][12] = 1;
    mask[11][13] = 1;
    mask[11][14] = 1;
    mask[11][15] = 1;
    mask[11][16] = 1;
    mask[11][17] = 1;
    mask[11][18] = 1;
    mask[11][19] = 1;
    mask[11][20] = 1;
    mask[11][21] = 1;
    mask[11][22] = 1;
    mask[12][8] = 1;
    mask[12][9] = 1;
    mask[12][10] = 1;
    mask[12][14] = 1;
    mask[12][15] = 1;
    mask[12][16] = 1;
    mask[12][17] = 1;
    mask[12][18] = 1;
    mask[12][19] = 1;
    mask[12][23] = 1;
    mask[13][8] = 1;
    mask[13][9] = 1;
    mask[13][10] = 1;
    mask[13][15] = 1;
    mask[13][16] = 1;
    mask[13][17] = 1;
    mask[13][18] = 1;
    mask[13][23] = 1;
    mask[13][24] = 1;
    mask[14][8] = 1;
    mask[14][9] = 1;
    mask[14][10] = 1;
    mask[14][15] = 1;
    mask[14][16] = 1;
    mask[14][17] = 1;
    mask[14][18] = 1;
    mask[14][23] = 1;
    mask[14][24] = 1;
    mask[14][25] = 1;
    mask[15][8] = 1;
    mask[15][9] = 1;
    mask[15][10] = 1;
    mask[15][15] = 1;
    mask[15][16] = 1;
    mask[15][17] = 1;
    mask[15][18] = 1;
    mask[15][23] = 1;
    mask[15][24] = 1;
    mask[15][25] = 1;
    mask[16][8] = 1;
    mask[16][9] = 1;
    mask[16][10] = 1;
    mask[16][15] = 1;
    mask[16][16] = 1;
    mask[16][17] = 1;
    mask[16][18] = 1;
    mask[16][23] = 1;
    mask[16][24] = 1;
    mask[16][25] = 1;
    mask[17][8] = 1;
    mask[17][9] = 1;
    mask[17][10] = 1;
    mask[17][15] = 1;
    mask[17][16] = 1;
    mask[17][17] = 1;
    mask[17][18] = 1;
    mask[17][23] = 1;
    mask[17][24] = 1;
    mask[17][25] = 1;
    mask[18][9] = 1;
    mask[18][10] = 1;
    mask[18][15] = 1;
    mask[18][16] = 1;
    mask[18][17] = 1;
    mask[18][18] = 1;
    mask[18][23] = 1;
    mask[18][24] = 1;
    mask[18][25] = 1;
    mask[19][10] = 1;
    mask[19][13] = 1;
    mask[19][14] = 1;
    mask[19][15] = 1;
    mask[19][16] = 1;
    mask[19][17] = 1;
    mask[19][18] = 1;
    mask[19][19] = 1;
    mask[19][20] = 1;
    mask[19][23] = 1;
    mask[19][24] = 1;
    mask[19][25] = 1;
    mask[20][11] = 1;
    mask[20][12] = 1;
    mask[20][13] = 1;
    mask[20][14] = 1;
    mask[20][15] = 1;
    mask[20][16] = 1;
    mask[20][17] = 1;
    mask[20][18] = 1;
    mask[20][19] = 1;
    mask[20][20] = 1;
    mask[20][21] = 1;
    mask[20][22] = 1;
    mask[20][23] = 1;
    mask[20][24] = 1;
    mask[20][25] = 1;
    mask[21][10] = 1;
    mask[21][11] = 1;
    mask[21][12] = 1;
    mask[21][13] = 1;
    mask[21][14] = 1;
    mask[21][15] = 1;
    mask[21][18] = 1;
    mask[21][19] = 1;
    mask[21][20] = 1;
    mask[21][21] = 1;
    mask[21][22] = 1;
    mask[21][23] = 1;
    mask[21][24] = 1;
    mask[21][25] = 1;
    mask[22][8] = 1;
    mask[22][9] = 1;
    mask[22][10] = 1;
    mask[22][11] = 1;
    mask[22][12] = 1;
    mask[22][13] = 1;
    mask[22][20] = 1;
    mask[22][21] = 1;
    mask[22][22] = 1;
    mask[22][23] = 1;
    mask[22][24] = 1;
    mask[23][9] = 1;
    mask[23][10] = 1;
    mask[23][11] = 1;
    mask[23][22] = 1;

    // Store original colors for each point
    // Left part (blue gradient)
    original_colors[8][10][0] = 149; original_colors[8][10][1] = 192; original_colors[8][10][2] = 246;
    original_colors[8][11][0] = 151; original_colors[8][11][1] = 189; original_colors[8][11][2] = 246;
    original_colors[8][12][0] = 159; original_colors[8][12][1] = 172; original_colors[8][12][2] = 246;
    original_colors[8][22][0] = 220; original_colors[8][22][1] = 156; original_colors[8][22][2] = 208;
    original_colors[8][23][0] = 223; original_colors[8][23][1] = 157; original_colors[8][23][2] = 205;
    original_colors[8][24][0] = 232; original_colors[8][24][1] = 160; original_colors[8][24][2] = 199;
    original_colors[9][9][0] = 146; original_colors[9][9][1] = 202; original_colors[9][9][2] = 247;
    original_colors[9][10][0] = 149; original_colors[9][10][1] = 192; original_colors[9][10][2] = 246;
    original_colors[9][11][0] = 153; original_colors[9][11][1] = 184; original_colors[9][11][2] = 246;
    original_colors[9][12][0] = 159; original_colors[9][12][1] = 172; original_colors[9][12][2] = 246;
    original_colors[9][13][0] = 166; original_colors[9][13][1] = 161; original_colors[9][13][2] = 245;
    original_colors[9][20][0] = 204; original_colors[9][20][1] = 150; original_colors[9][20][2] = 220;
    original_colors[9][21][0] = 211; original_colors[9][21][1] = 153; original_colors[9][21][2] = 215;
    original_colors[9][22][0] = 220; original_colors[9][22][1] = 157; original_colors[9][22][2] = 208;
    original_colors[9][23][0] = 228; original_colors[9][23][1] = 159; original_colors[9][23][2] = 201;
    original_colors[9][24][0] = 236; original_colors[9][24][1] = 162; original_colors[9][24][2] = 200;
    original_colors[9][25][0] = 240; original_colors[9][25][1] = 162; original_colors[9][25][2] = 195;
    original_colors[10][8][0] = 145; original_colors[10][8][1] = 210; original_colors[10][8][2] = 247;
    original_colors[10][9][0] = 147; original_colors[10][9][1] = 203; original_colors[10][9][2] = 248;
    original_colors[10][10][0] = 148; original_colors[10][10][1] = 198; original_colors[10][10][2] = 247;
    original_colors[10][11][0] = 154; original_colors[10][11][1] = 184; original_colors[10][11][2] = 246;
    original_colors[10][12][0] = 159; original_colors[10][12][1] = 172; original_colors[10][12][2] = 246;
    original_colors[10][13][0] = 166; original_colors[10][13][1] = 162; original_colors[10][13][2] = 245;
    original_colors[10][14][0] = 172; original_colors[10][14][1] = 154; original_colors[10][14][2] = 243;
    original_colors[10][15][0] = 178; original_colors[10][15][1] = 147; original_colors[10][15][2] = 243;
    original_colors[10][18][0] = 191; original_colors[10][18][1] = 144; original_colors[10][18][2] = 235;
    original_colors[10][19][0] = 196; original_colors[10][19][1] = 148; original_colors[10][19][2] = 229;
    original_colors[10][20][0] = 203; original_colors[10][20][1] = 150; original_colors[10][20][2] = 222;
    original_colors[10][21][0] = 212; original_colors[10][21][1] = 153; original_colors[10][21][2] = 214;
    original_colors[10][22][0] = 219; original_colors[10][22][1] = 156; original_colors[10][22][2] = 208;
    original_colors[10][23][0] = 226; original_colors[10][23][1] = 158; original_colors[10][23][2] = 202;
    original_colors[11][8][0] = 145; original_colors[11][8][1] = 209; original_colors[11][8][2] = 247;
    original_colors[11][9][0] = 146; original_colors[11][9][1] = 203; original_colors[11][9][2] = 247;
    original_colors[11][10][0] = 148; original_colors[11][10][1] = 194; original_colors[11][10][2] = 246;
    original_colors[11][11][0] = 153; original_colors[11][11][1] = 184; original_colors[11][11][2] = 246;
    original_colors[11][12][0] = 159; original_colors[11][12][1] = 172; original_colors[11][12][2] = 245;
    original_colors[11][13][0] = 165; original_colors[11][13][1] = 162; original_colors[11][13][2] = 245;
    original_colors[11][14][0] = 172; original_colors[11][14][1] = 154; original_colors[11][14][2] = 243;
    original_colors[11][15][0] = 179; original_colors[11][15][1] = 147; original_colors[11][15][2] = 243;
    original_colors[11][16][0] = 182; original_colors[11][16][1] = 142; original_colors[11][16][2] = 242;
    original_colors[11][17][0] = 186; original_colors[11][17][1] = 141; original_colors[11][17][2] = 239;
    original_colors[11][18][0] = 191; original_colors[11][18][1] = 144; original_colors[11][18][2] = 235;
    original_colors[11][19][0] = 196; original_colors[11][19][1] = 147; original_colors[11][19][2] = 228;
    original_colors[11][20][0] = 203; original_colors[11][20][1] = 149; original_colors[11][20][2] = 220;
    original_colors[11][21][0] = 212; original_colors[11][21][1] = 153; original_colors[11][21][2] = 215;
    original_colors[11][22][0] = 220; original_colors[11][22][1] = 157; original_colors[11][22][2] = 208;
    original_colors[12][8][0] = 145; original_colors[12][8][1] = 209; original_colors[12][8][2] = 247;
    original_colors[12][9][0] = 146; original_colors[12][9][1] = 202; original_colors[12][9][2] = 247;
    original_colors[12][10][0] = 149; original_colors[12][10][1] = 194; original_colors[12][10][2] = 246;
    original_colors[12][14][0] = 172; original_colors[12][14][1] = 153; original_colors[12][14][2] = 243;
    original_colors[12][15][0] = 178; original_colors[12][15][1] = 147; original_colors[12][15][2] = 242;
    original_colors[12][16][0] = 183; original_colors[12][16][1] = 142; original_colors[12][16][2] = 243;
    original_colors[12][17][0] = 185; original_colors[12][17][1] = 142; original_colors[12][17][2] = 240;
    original_colors[12][18][0] = 191; original_colors[12][18][1] = 144; original_colors[12][18][2] = 234;
    original_colors[12][19][0] = 197; original_colors[12][19][1] = 147; original_colors[12][19][2] = 229;
    original_colors[12][23][0] = 226; original_colors[12][23][1] = 158; original_colors[12][23][2] = 203;
    original_colors[13][8][0] = 146; original_colors[13][8][1] = 211; original_colors[13][8][2] = 247;
    original_colors[13][9][0] = 146; original_colors[13][9][1] = 204; original_colors[13][9][2] = 247;
    original_colors[13][10][0] = 149; original_colors[13][10][1] = 195; original_colors[13][10][2] = 247;
    original_colors[13][15][0] = 178; original_colors[13][15][1] = 148; original_colors[13][15][2] = 242;
    original_colors[13][16][0] = 184; original_colors[13][16][1] = 142; original_colors[13][16][2] = 242;
    original_colors[13][17][0] = 185; original_colors[13][17][1] = 142; original_colors[13][17][2] = 240;
    original_colors[13][18][0] = 190; original_colors[13][18][1] = 144; original_colors[13][18][2] = 235;
    original_colors[13][23][0] = 228; original_colors[13][23][1] = 158; original_colors[13][23][2] = 204;
    original_colors[13][24][0] = 235; original_colors[13][24][1] = 161; original_colors[13][24][2] = 198;
    original_colors[14][8][0] = 145; original_colors[14][8][1] = 209; original_colors[14][8][2] = 247;
    original_colors[14][9][0] = 147; original_colors[14][9][1] = 203; original_colors[14][9][2] = 247;
    original_colors[14][10][0] = 149; original_colors[14][10][1] = 194; original_colors[14][10][2] = 246;
    original_colors[14][15][0] = 178; original_colors[14][15][1] = 147; original_colors[14][15][2] = 243;
    original_colors[14][16][0] = 183; original_colors[14][16][1] = 142; original_colors[14][16][2] = 242;
    original_colors[14][17][0] = 186; original_colors[14][17][1] = 142; original_colors[14][17][2] = 241;
    original_colors[14][18][0] = 190; original_colors[14][18][1] = 144; original_colors[14][18][2] = 235;
    original_colors[14][23][0] = 227; original_colors[14][23][1] = 159; original_colors[14][23][2] = 203;
    original_colors[14][24][0] = 234; original_colors[14][24][1] = 161; original_colors[14][24][2] = 197;
    original_colors[14][25][0] = 241; original_colors[14][25][1] = 163; original_colors[14][25][2] = 195;
    original_colors[15][8][0] = 145; original_colors[15][8][1] = 209; original_colors[15][8][2] = 247;
    original_colors[15][9][0] = 147; original_colors[15][9][1] = 203; original_colors[15][9][2] = 247;
    original_colors[15][10][0] = 149; original_colors[15][10][1] = 194; original_colors[15][10][2] = 246;
    original_colors[15][15][0] = 178; original_colors[15][15][1] = 147; original_colors[15][15][2] = 243;
    original_colors[15][16][0] = 183; original_colors[15][16][1] = 142; original_colors[15][16][2] = 242;
    original_colors[15][17][0] = 186; original_colors[15][17][1] = 142; original_colors[15][17][2] = 241;
    original_colors[15][18][0] = 190; original_colors[15][18][1] = 144; original_colors[15][18][2] = 235;
    original_colors[15][23][0] = 227; original_colors[15][23][1] = 159; original_colors[15][23][2] = 203;
    original_colors[15][24][0] = 234; original_colors[15][24][1] = 161; original_colors[15][24][2] = 197;
    original_colors[15][25][0] = 241; original_colors[15][25][1] = 163; original_colors[15][25][2] = 195;
    original_colors[16][8][0] = 145; original_colors[16][8][1] = 209; original_colors[16][8][2] = 247;
    original_colors[16][9][0] = 147; original_colors[16][9][1] = 203; original_colors[16][9][2] = 247;
    original_colors[16][10][0] = 149; original_colors[16][10][1] = 194; original_colors[16][10][2] = 246;
    original_colors[16][15][0] = 178; original_colors[16][15][1] = 147; original_colors[16][15][2] = 243;
    original_colors[16][16][0] = 183; original_colors[16][16][1] = 142; original_colors[16][16][2] = 242;
    original_colors[16][17][0] = 186; original_colors[16][17][1] = 142; original_colors[16][17][2] = 241;
    original_colors[16][18][0] = 190; original_colors[16][18][1] = 144; original_colors[16][18][2] = 235;
    original_colors[16][23][0] = 227; original_colors[16][23][1] = 159; original_colors[16][23][2] = 203;
    original_colors[16][24][0] = 234; original_colors[16][24][1] = 161; original_colors[16][24][2] = 197;
    original_colors[16][25][0] = 241; original_colors[16][25][1] = 163; original_colors[16][25][2] = 195;
    original_colors[17][8][0] = 145; original_colors[17][8][1] = 209; original_colors[17][8][2] = 247;
    original_colors[17][9][0] = 147; original_colors[17][9][1] = 203; original_colors[17][9][2] = 247;
    original_colors[17][10][0] = 149; original_colors[17][10][1] = 194; original_colors[17][10][2] = 246;
    original_colors[17][15][0] = 178; original_colors[17][15][1] = 147; original_colors[17][15][2] = 243;
    original_colors[17][16][0] = 183; original_colors[17][16][1] = 142; original_colors[17][16][2] = 242;
    original_colors[17][17][0] = 186; original_colors[17][17][1] = 142; original_colors[17][17][2] = 241;
    original_colors[17][18][0] = 190; original_colors[17][18][1] = 144; original_colors[17][18][2] = 235;
    original_colors[17][23][0] = 227; original_colors[17][23][1] = 159; original_colors[17][23][2] = 203;
    original_colors[17][24][0] = 234; original_colors[17][24][1] = 161; original_colors[17][24][2] = 197;
    original_colors[17][25][0] = 241; original_colors[17][25][1] = 163; original_colors[17][25][2] = 195;
    original_colors[18][9][0] = 146; original_colors[18][9][1] = 202; original_colors[18][9][2] = 247;
    original_colors[18][10][0] = 149; original_colors[18][10][1] = 194; original_colors[18][10][2] = 245;
    original_colors[18][15][0] = 178; original_colors[18][15][1] = 147; original_colors[18][15][2] = 242;
    original_colors[18][16][0] = 183; original_colors[18][16][1] = 142; original_colors[18][16][2] = 242;
    original_colors[18][17][0] = 185; original_colors[18][17][1] = 142; original_colors[18][17][2] = 240;
    original_colors[18][18][0] = 191; original_colors[18][18][1] = 144; original_colors[18][18][2] = 235;
    original_colors[18][23][0] = 227; original_colors[18][23][1] = 159; original_colors[18][23][2] = 203;
    original_colors[18][24][0] = 234; original_colors[18][24][1] = 161; original_colors[18][24][2] = 197;
    original_colors[18][25][0] = 241; original_colors[18][25][1] = 163; original_colors[18][25][2] = 195;
    original_colors[19][10][0] = 150; original_colors[19][10][1] = 193; original_colors[19][10][2] = 247;
    original_colors[19][13][0] = 165; original_colors[19][13][1] = 162; original_colors[19][13][2] = 245;
    original_colors[19][14][0] = 172; original_colors[19][14][1] = 154; original_colors[19][14][2] = 243;
    original_colors[19][15][0] = 179; original_colors[19][15][1] = 147; original_colors[19][15][2] = 243;
    original_colors[19][16][0] = 182; original_colors[19][16][1] = 142; original_colors[19][16][2] = 242;
    original_colors[19][17][0] = 186; original_colors[19][17][1] = 141; original_colors[19][17][2] = 239;
    original_colors[19][18][0] = 191; original_colors[19][18][1] = 144; original_colors[19][18][2] = 235;
    original_colors[19][19][0] = 196; original_colors[19][19][1] = 147; original_colors[19][19][2] = 228;
    original_colors[19][20][0] = 203; original_colors[19][20][1] = 149; original_colors[19][20][2] = 220;
    original_colors[19][23][0] = 227; original_colors[19][23][1] = 159; original_colors[19][23][2] = 203;
    original_colors[19][24][0] = 234; original_colors[19][24][1] = 161; original_colors[19][24][2] = 197;
    original_colors[19][25][0] = 241; original_colors[19][25][1] = 163; original_colors[19][25][2] = 195;
    original_colors[20][11][0] = 153; original_colors[20][11][1] = 184; original_colors[20][11][2] = 246;
    original_colors[20][12][0] = 159; original_colors[20][12][1] = 172; original_colors[20][12][2] = 245;
    original_colors[20][13][0] = 165; original_colors[20][13][1] = 162; original_colors[20][13][2] = 245;
    original_colors[20][14][0] = 172; original_colors[20][14][1] = 154; original_colors[20][14][2] = 243;
    original_colors[20][15][0] = 179; original_colors[20][15][1] = 147; original_colors[20][15][2] = 243;
    original_colors[20][16][0] = 182; original_colors[20][16][1] = 142; original_colors[20][16][2] = 242;
    original_colors[20][17][0] = 186; original_colors[20][17][1] = 141; original_colors[20][17][2] = 239;
    original_colors[20][18][0] = 191; original_colors[20][18][1] = 144; original_colors[20][18][2] = 235;
    original_colors[20][19][0] = 196; original_colors[20][19][1] = 147; original_colors[20][19][2] = 228;
    original_colors[20][20][0] = 203; original_colors[20][20][1] = 149; original_colors[20][20][2] = 220;
    original_colors[20][21][0] = 212; original_colors[20][21][1] = 153; original_colors[20][21][2] = 215;
    original_colors[20][22][0] = 220; original_colors[20][22][1] = 157; original_colors[20][22][2] = 208;
    original_colors[20][23][0] = 227; original_colors[20][23][1] = 159; original_colors[20][23][2] = 203;
    original_colors[20][24][0] = 234; original_colors[20][24][1] = 161; original_colors[20][24][2] = 197;
    original_colors[20][25][0] = 241; original_colors[20][25][1] = 163; original_colors[20][25][2] = 195;
    original_colors[21][10][0] = 149; original_colors[21][10][1] = 194; original_colors[21][10][2] = 246;
    original_colors[21][11][0] = 153; original_colors[21][11][1] = 183; original_colors[21][11][2] = 246;
    original_colors[21][12][0] = 158; original_colors[21][12][1] = 172; original_colors[21][12][2] = 245;
    original_colors[21][13][0] = 165; original_colors[21][13][1] = 163; original_colors[21][13][2] = 245;
    original_colors[21][14][0] = 173; original_colors[21][14][1] = 155; original_colors[21][14][2] = 244;
    original_colors[21][15][0] = 178; original_colors[21][15][1] = 147; original_colors[21][15][2] = 242;
    original_colors[21][18][0] = 191; original_colors[21][18][1] = 144; original_colors[21][18][2] = 235;
    original_colors[21][19][0] = 196; original_colors[21][19][1] = 147; original_colors[21][19][2] = 228;
    original_colors[21][20][0] = 203; original_colors[21][20][1] = 149; original_colors[21][20][2] = 220;
    original_colors[21][21][0] = 212; original_colors[21][21][1] = 153; original_colors[21][21][2] = 215;
    original_colors[21][22][0] = 220; original_colors[21][22][1] = 157; original_colors[21][22][2] = 208;
    original_colors[21][23][0] = 227; original_colors[21][23][1] = 159; original_colors[21][23][2] = 203;
    original_colors[21][24][0] = 234; original_colors[21][24][1] = 161; original_colors[21][24][2] = 197;
    original_colors[21][25][0] = 241; original_colors[21][25][1] = 163; original_colors[21][25][2] = 195;
    original_colors[22][8][0] = 145; original_colors[22][8][1] = 209; original_colors[22][8][2] = 247;
    original_colors[22][9][0] = 146; original_colors[22][9][1] = 202; original_colors[22][9][2] = 247;
    original_colors[22][10][0] = 149; original_colors[22][10][1] = 194; original_colors[22][10][2] = 246;
    original_colors[22][11][0] = 153; original_colors[22][11][1] = 183; original_colors[22][11][2] = 246;
    original_colors[22][12][0] = 158; original_colors[22][12][1] = 172; original_colors[22][12][2] = 245;
    original_colors[22][13][0] = 165; original_colors[22][13][1] = 163; original_colors[22][13][2] = 245;
    original_colors[22][20][0] = 203; original_colors[22][20][1] = 149; original_colors[22][20][2] = 220;
    original_colors[22][21][0] = 212; original_colors[22][21][1] = 153; original_colors[22][21][2] = 215;
    original_colors[22][22][0] = 220; original_colors[22][22][1] = 157; original_colors[22][22][2] = 208;
    original_colors[22][23][0] = 227; original_colors[22][23][1] = 159; original_colors[22][23][2] = 203;
    original_colors[22][24][0] = 234; original_colors[22][24][1] = 161; original_colors[22][24][2] = 197;
    original_colors[23][9][0] = 147; original_colors[23][9][1] = 202; original_colors[23][9][2] = 247;
    original_colors[23][10][0] = 149; original_colors[23][10][1] = 193; original_colors[23][10][2] = 245;
    original_colors[23][11][0] = 153; original_colors[23][11][1] = 183; original_colors[23][11][2] = 245;
    original_colors[23][21][0] = 212; original_colors[23][21][1] = 153; original_colors[23][21][2] = 215;
    original_colors[23][22][0] = 218; original_colors[23][22][1] = 156; original_colors[23][22][2] = 209;
    original_colors[23][23][0] = 227; original_colors[23][23][1] = 159; original_colors[23][23][2] = 203;
}

// Calculate the brightness based on the distance from the flash center line
float calculate_flash_brightness(int y, int x, int flash_pos) {
    // Distance from pixel to the diagonal flash line
    // The diagonal flash line is represented by the equation: y = x - flash_pos
    // Distance from point (x,y) to line y = x - flash_pos is:
    // |y - x + flash_pos| / sqrt(2)
    float distance = fabsf((float)y - (float)x + (float)flash_pos) / 1.414f;
    
    // Calculate brightness - full brightness at the center, fading to edges
    if (distance < FLASH_WIDTH) {
        // Cosine brightness falloff for a more natural light effect
        return cosf(distance * 3.14159f / (2.0f * FLASH_WIDTH));
    }
    
    return 0.0f; // No brightness outside flash width
}

// Update matrix with diagonal flash animation
void update_matrix() {
    // Clear grid (set all to dark background)
    memset(grid, 0, sizeof(grid));
    
    // Apply original colors to points at reduced brightness
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (mask[y][x]) {
                // Apply original colors
                RGB adjusted = adjust_brightness_saturation(
                    original_colors[y][x][0], 
                    original_colors[y][x][1], 
                    original_colors[y][x][2]
                );
                grid[y][x][0] = adjusted.r;
                grid[y][x][1] = adjusted.g;
                grid[y][x][2] = adjusted.b;
            }
        }
    }
    
    // Update flash position
    flash_position += ANIMATION_SPEED;
    
    // Reset flash position when it exits the screen
    if (flash_position > WIDTH + HEIGHT + FLASH_WIDTH) {
        flash_position = 0; // Start from (0,0)
    }
    
    // Apply flash effect to all pixels in the mask
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (mask[y][x]) {
                // Calculate brightness based on distance from the flash line
                float brightness = calculate_flash_brightness(y, x, flash_position);
                
                if (brightness > 0.0f) {
                    // Brighten the original color
                    float brighten_factor = 1.0f + brightness * 1.5f; // Max 2.5x brightness
                    
                    // Get original adjusted color
                    RGB adjusted = adjust_brightness_saturation(
                        original_colors[y][x][0], 
                        original_colors[y][x][1], 
                        original_colors[y][x][2]
                    );
                    
                    // Increase brightness
                    uint16_t r = (uint16_t)(adjusted.r * brighten_factor);
                    uint16_t g = (uint16_t)(adjusted.g * brighten_factor);
                    uint16_t b = (uint16_t)(adjusted.b * brighten_factor);
                    
                    // Clamp to valid range
                    grid[y][x][0] = (r > 255) ? 255 : (uint8_t)r;
                    grid[y][x][1] = (g > 255) ? 255 : (uint8_t)g;
                    grid[y][x][2] = (b > 255) ? 255 : (uint8_t)b;
                }
            }
        }
    }
    
    // Render to LED matrix (simple row-major layout)
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int led_index = y * WIDTH + x; // Simple row-major layout
            RGB color = color_correct(grid[y][x][0], grid[y][x][1], grid[y][x][2]);
            led_strip_set_pixel(led_strip, led_index, color.r, color.g, color.b);
        }
    }
    led_strip_refresh(led_strip);
}
