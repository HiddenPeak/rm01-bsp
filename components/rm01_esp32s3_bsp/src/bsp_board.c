#include "bsp_board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "BSP";

// WS2812全局句柄
led_strip_handle_t ws2812_onboard_strip;
led_strip_handle_t ws2812_array_strip;

// ADC全局句柄
adc_oneshot_unit_handle_t adc1_handle;
adc_oneshot_unit_handle_t adc2_handle;
adc_cali_handle_t adc1_cali_handle;
adc_cali_handle_t adc2_cali_handle;

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
    gpio_set_level(BSP_LPN100_POWER_PIN, 1); // 默认高电平
}

void bsp_w5500_init(spi_host_device_t host) {
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
    bsp_ws2812_array_init();
    bsp_orin_init();
    bsp_lpn100_init();
    bsp_w5500_init(SPI3_HOST);
    bsp_rtl8367_init();
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
    gpio_set_level(BSP_LPN100_POWER_PIN, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms
    gpio_set_level(BSP_LPN100_POWER_PIN, 1);
}
