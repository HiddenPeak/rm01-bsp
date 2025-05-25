#include "bsp_power.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BSP_POWER";

// ADC全局句柄
adc_oneshot_unit_handle_t adc1_handle = NULL;
adc_oneshot_unit_handle_t adc2_handle = NULL;
adc_cali_handle_t adc1_cali_handle = NULL;
adc_cali_handle_t adc2_cali_handle = NULL;

void bsp_power_init(void) {
    ESP_LOGI(TAG, "初始化电源管理模块");
    
    // 初始化各个子模块
    bsp_orin_init();
    bsp_lpn100_init();
    bsp_voltage_init();
    
    ESP_LOGI(TAG, "电源管理模块初始化完成");
}

void bsp_orin_init(void) {
    ESP_LOGI(TAG, "初始化ORIN电源控制");
    
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
    
    ESP_LOGI(TAG, "ORIN电源控制初始化完成");
}

void bsp_lpn100_init(void) {
    ESP_LOGI(TAG, "初始化LPN100电源控制");
    
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
    // 拉低100ms启动
    gpio_set_level(BSP_LPN100_POWER_PIN, 0); // 按下电源按钮
    vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms
    gpio_set_level(BSP_LPN100_POWER_PIN, 1); // 默认高电平
    
    ESP_LOGI(TAG, "LPN100 PWR_BTN引脚初始化为高电平，避免清空BIOS");
}

void bsp_voltage_init(void) {
    ESP_LOGI(TAG, "初始化电压监测ADC");
    
    // ADC初始化 主电源 (ADC2)
    adc_oneshot_unit_init_cfg_t init_config2 = {
        .unit_id = ADC_UNIT_2,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config2, &adc2_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC2初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    adc_oneshot_chan_cfg_t chan_config2 = {
        .atten = ADC_ATTEN_DB_12,  // 0~3.3V
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_oneshot_config_channel(adc2_handle, ADC_CHANNEL_7, &chan_config2); // GPIO18
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC2通道配置失败: %s", esp_err_to_name(ret));
        return;
    }

    // 线性拟合校准 ADC2
    adc_cali_curve_fitting_config_t cali_config2 = {
        .unit_id = ADC_UNIT_2,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config2, &adc2_cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC2校准失败: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "ADC2校准成功");
    }

    // ADC初始化 辅助电源 (ADC1)
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ret = adc_oneshot_new_unit(&init_config1, &adc1_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC1初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    adc_oneshot_chan_cfg_t chan_config1 = {
        .atten = ADC_ATTEN_DB_12,  // 0~3.3V
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_7, &chan_config1); // GPIO8
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC1通道配置失败: %s", esp_err_to_name(ret));
        return;
    }

    // 线性拟合校准 ADC1
    adc_cali_curve_fitting_config_t cali_config1 = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config1, &adc1_cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC1校准失败: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "ADC1校准成功");
    }
    
    ESP_LOGI(TAG, "电压监测ADC初始化完成");
}

float bsp_get_main_voltage(void) {
    if (adc2_handle == NULL || adc2_cali_handle == NULL) {
        ESP_LOGE(TAG, "主电源ADC未初始化");
        return 0.0;
    }
    
    int raw;
    int voltage_mv;
    esp_err_t ret = adc_oneshot_read(adc2_handle, ADC_CHANNEL_7, &raw);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取主电源ADC失败: %s", esp_err_to_name(ret));
        return 0.0;
    }
    
    ret = adc_cali_raw_to_voltage(adc2_cali_handle, raw, &voltage_mv);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "主电源ADC校准失败: %s", esp_err_to_name(ret));
        return 0.0;
    }
    
    float voltage = (voltage_mv / 1000.0) * VOLTAGE_RATIO;
    ESP_LOGD(TAG, "主电源 ADC raw: %d, cali: %d mV, final: %.2f V", raw, voltage_mv, voltage);
    return voltage;
}

float bsp_get_aux_12v_voltage(void) {
    if (adc1_handle == NULL || adc1_cali_handle == NULL) {
        ESP_LOGE(TAG, "辅助电源ADC未初始化");
        return 0.0;
    }
    
    int raw;
    int voltage_mv;
    esp_err_t ret = adc_oneshot_read(adc1_handle, ADC_CHANNEL_7, &raw); // GPIO8
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取辅助电源ADC失败: %s", esp_err_to_name(ret));
        return 0.0;
    }
    
    ret = adc_cali_raw_to_voltage(adc1_cali_handle, raw, &voltage_mv);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "辅助电源ADC校准失败: %s", esp_err_to_name(ret));
        return 0.0;
    }
    
    float voltage = (voltage_mv / 1000.0) * VOLTAGE_RATIO;
    ESP_LOGD(TAG, "辅助电源 ADC raw: %d, cali: %d mV, final: %.2f V", raw, voltage_mv, voltage);
    return voltage;
}

void bsp_lpn100_power_toggle(void) {
    // 按下电源按钮不超过300毫秒，足够触发电源操作但不会清空BIOS
    // LPN100的PWR_BTN按下超过5秒会清空BIOS
    ESP_LOGI(TAG, "LPN100电源按钮按下，时间控制在300ms以内，避免清空BIOS");
    gpio_set_level(BSP_LPN100_POWER_PIN, 0);
    vTaskDelay(300 / portTICK_PERIOD_MS); // 300ms，安全时间
    gpio_set_level(BSP_LPN100_POWER_PIN, 1);
}

void bsp_orin_reset(void) {
    ESP_LOGI(TAG, "ORIN模块复位");
    gpio_set_level(BSP_ORIN_RESET_PIN, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms复位脉冲
    gpio_set_level(BSP_ORIN_RESET_PIN, 1);
}

void bsp_orin_power_control(bool enable) {
    ESP_LOGI(TAG, "ORIN电源控制: %s", enable ? "开启" : "关闭");
    gpio_set_level(BSP_ORIN_POWER_PIN, enable ? 0 : 1); // 低电平有效
}

void bsp_lpn100_reset(void) {
    ESP_LOGI(TAG, "LPN100模块复位");
    gpio_set_level(BSP_LPN100_RESET_PIN, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms复位脉冲
    gpio_set_level(BSP_LPN100_RESET_PIN, 1);
}

esp_err_t bsp_get_power_status(float *main_voltage, float *aux_voltage) {
    if (main_voltage == NULL || aux_voltage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *main_voltage = bsp_get_main_voltage();
    *aux_voltage = bsp_get_aux_12v_voltage();
    
    ESP_LOGI(TAG, "电源状态 - 主电源: %.2fV, 辅助电源: %.2fV", *main_voltage, *aux_voltage);
    
    return ESP_OK;
}
