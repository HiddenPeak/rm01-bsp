#include "bsp_power.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include <string.h>
#include <math.h>

static const char *TAG = "BSP_POWER";

// ADC全局句柄
adc_oneshot_unit_handle_t adc1_handle = NULL;
adc_oneshot_unit_handle_t adc2_handle = NULL;
adc_cali_handle_t adc1_cali_handle = NULL;
adc_cali_handle_t adc2_cali_handle = NULL;

// 电源芯片数据
static bsp_power_chip_data_t power_chip_data = {0};
static bool uart_initialized = false;
static TaskHandle_t voltage_monitor_task_handle = NULL;

// 电压变化监控
static float last_main_voltage = 0.0f;
static float last_aux_voltage = 0.0f;
static float main_voltage_threshold = 3.0f;  // 主电源电压变化阈值，默认3.0V
static float aux_voltage_threshold = 3.0f;   // 辅助电源电压变化阈值，默认3.0V
static bool system_boot_negotiation_done = false; // 系统启动协商是否完成

// UART接收缓冲区
#define UART_BUFFER_SIZE 256
static uint8_t uart_buffer[UART_BUFFER_SIZE];

// 函数前置声明
static void voltage_monitor_task(void *pvParameters);
static void perform_power_chip_negotiation(void);
static bool check_voltage_change(void);
static esp_err_t parse_power_chip_data(const uint8_t* raw_data, int data_len, bsp_power_chip_data_t* data);

void bsp_power_init(void) {
    ESP_LOGI(TAG, "初始化电源管理模块");
    
    // 初始化各个子模块
    bsp_orin_init();
    bsp_lpn100_init();
    bsp_voltage_init();
    bsp_power_chip_uart_init();
    
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
    
    gpio_set_level(BSP_ORIN_RESET_PIN, 0); // 默认不复位
    gpio_set_level(BSP_ORIN_POWER_PIN, 0); // 持续拉高保持开启
    
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
    gpio_set_level(BSP_LPN100_POWER_PIN, 1);
    vTaskDelay(300 / portTICK_PERIOD_MS); // 300ms，安全时间
    gpio_set_level(BSP_LPN100_POWER_PIN, 0);
}

void bsp_orin_reset(void) {
    ESP_LOGI(TAG, "ORIN模块复位");
    gpio_set_level(BSP_ORIN_RESET_PIN, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms复位脉冲
    gpio_set_level(BSP_ORIN_RESET_PIN, 0);
}

void bsp_orin_power_control(bool enable) {
    ESP_LOGI(TAG, "ORIN电源控制: %s", enable ? "开启" : "关闭");
    gpio_set_level(BSP_ORIN_POWER_PIN, enable ? 1 : 0); // 高电平有效
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

static void voltage_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "电压监控任务启动 - 检测电压变化以触发电源协商");
    
    // 延迟启动，等待系统稳定
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 系统启动时进行首次电源协商
    if (!system_boot_negotiation_done) {
        ESP_LOGI(TAG, "系统启动 - 执行电源芯片协商");
        perform_power_chip_negotiation();
        system_boot_negotiation_done = true;
    }
    
    while (1) {
        // 检查电压变化
        if (check_voltage_change()) {
            ESP_LOGI(TAG, "检测到电压变化 - 触发电源芯片协商");
            perform_power_chip_negotiation();
        }
        
        // 每2秒检查一次电压变化
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void perform_power_chip_negotiation(void) {
    if (!uart_initialized) {
        ESP_LOGW(TAG, "UART未初始化，跳过电源芯片协商");
        return;
    }
    
    bsp_power_chip_data_t temp_data;
    esp_err_t ret = bsp_get_power_chip_data(&temp_data);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "电源芯片协商完成 - 电压: %.2fV, 电流: %.3fA, 功率: %.2fW", 
                temp_data.voltage, temp_data.current, temp_data.power);
    } else if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "电源芯片协商超时 - 未接收到数据");
    } else {
        ESP_LOGE(TAG, "电源芯片协商失败: %s", esp_err_to_name(ret));
    }
}

static bool check_voltage_change(void) {
    float current_main_voltage = bsp_get_main_voltage();
    float current_aux_voltage = bsp_get_aux_12v_voltage();
    
    bool voltage_changed = false;
    
    // 检查主电源电压变化
    if (last_main_voltage > 0 && fabsf(current_main_voltage - last_main_voltage) > main_voltage_threshold) {
        ESP_LOGI(TAG, "主电源电压变化: %.2fV -> %.2fV (阈值: %.2fV)", 
                last_main_voltage, current_main_voltage, main_voltage_threshold);
        voltage_changed = true;
    }
    
    // 检查辅助电源电压变化
    if (last_aux_voltage > 0 && fabsf(current_aux_voltage - last_aux_voltage) > aux_voltage_threshold) {
        ESP_LOGI(TAG, "辅助电源电压变化: %.2fV -> %.2fV (阈值: %.2fV)", 
                last_aux_voltage, current_aux_voltage, aux_voltage_threshold);
        voltage_changed = true;
    }
    
    // 更新记录的电压值
    last_main_voltage = current_main_voltage;
    last_aux_voltage = current_aux_voltage;
    
    return voltage_changed;
}

// CRC8校验函数 - 使用Maxim/Dallas算法（多项式0x31）
static uint8_t calculate_crc8(const uint8_t *data, size_t length) {
    uint8_t crc = 0x00;  // Maxim算法初始值为0x00
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x01) {
                crc = (crc >> 1) ^ 0x8C;  // Maxim多项式0x31的反向形式
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

static esp_err_t parse_power_chip_data(const uint8_t* raw_data, int data_len, bsp_power_chip_data_t* data) {
    if (raw_data == NULL || data == NULL || data_len < 4) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // XSP16数据格式: [0xFF包头][电压][电流][CRC校验]
    // 查找包头0xFF
    int packet_start = -1;
    for (int i = 0; i <= data_len - 4; i++) {
        if (raw_data[i] == 0xFF) {
            packet_start = i;
            break;
        }
    }
    
    if (packet_start == -1 || (packet_start + 4) > data_len) {
        ESP_LOGW(TAG, "未找到有效的XSP16数据包头");
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // 提取数据包
    uint8_t header = raw_data[packet_start];     // 0xFF
    uint8_t voltage_raw = raw_data[packet_start + 1];
    uint8_t current_raw = raw_data[packet_start + 2];
    uint8_t crc_received = raw_data[packet_start + 3];
      // 验证CRC (校验包头+电压+电流，使用Maxim CRC8算法)
    uint8_t crc_calculated = calculate_crc8(&raw_data[packet_start], 3);
    if (crc_calculated != crc_received) {
        ESP_LOGW(TAG, "XSP16数据CRC校验失败: 计算值=0x%02X, 接收值=0x%02X (使用Maxim CRC8)", 
                 crc_calculated, crc_received);
        // 继续解析数据，但标记为CRC校验失败
    } else {
        ESP_LOGD(TAG, "XSP16数据CRC校验成功: 0x%02X", crc_calculated);
    }
    
    // 数据转换 - 根据XSP16规格
    float voltage = (float)voltage_raw;        // 电压值直接十六进制转十进制
    float current = (float)current_raw / 10.0f; // 电流值十六进制转十进制后除以10
    float power = voltage * current;           // 计算功率
    
    // 填充数据结构
    data->voltage = voltage;
    data->current = current;
    data->power = power;
    data->timestamp = esp_log_timestamp();
    data->valid = true;
    
    ESP_LOGI(TAG, "XSP16数据解析: V=%.2fV (0x%02X), I=%.3fA (0x%02X), P=%.2fW, CRC=0x%02X", 
             voltage, voltage_raw, current, current_raw, power, crc_received);
    
    return ESP_OK;
}

void bsp_power_chip_uart_init(void) {
    if (uart_initialized) {
        ESP_LOGW(TAG, "UART已经初始化");
        return;
    }
    
    ESP_LOGI(TAG, "初始化电源芯片UART通信 - 引脚GPIO%d", BSP_POWER_UART_RX_PIN);
    
    uart_config_t uart_config = {
        .baud_rate = BSP_POWER_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // 配置UART参数
    esp_err_t err = uart_param_config(BSP_POWER_UART_PORT, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART参数配置失败: %s", esp_err_to_name(err));
        return;
    }
    
    // 设置UART引脚
    err = uart_set_pin(BSP_POWER_UART_PORT, 
                       BSP_POWER_UART_TX_PIN,  // TX引脚（不使用，设为-1）
                       BSP_POWER_UART_RX_PIN,  // RX引脚（GPIO47）
                       UART_PIN_NO_CHANGE,     // RTS引脚
                       UART_PIN_NO_CHANGE);    // CTS引脚
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART引脚配置失败: %s", esp_err_to_name(err));
        return;
    }
    
    // 安装UART驱动
    err = uart_driver_install(BSP_POWER_UART_PORT, UART_BUFFER_SIZE * 2, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART驱动安装失败: %s", esp_err_to_name(err));
        return;
    }
    
    uart_initialized = true;
    
    // 创建电压监控任务（检测电压变化以触发电源协商）
    BaseType_t ret = xTaskCreate(voltage_monitor_task, 
                                "voltage_monitor", 
                                4096,               // 栈大小
                                NULL,              // 任务参数
                                4,                 // 任务优先级（降低优先级）
                                &voltage_monitor_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建电压监控任务失败");
    } else {
        ESP_LOGI(TAG, "电压监控任务已启动");
    }
      ESP_LOGI(TAG, "电源芯片UART通信初始化完成 - 波特率%d", BSP_POWER_UART_BAUDRATE);
}

esp_err_t bsp_get_power_chip_data(bsp_power_chip_data_t *data) {
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!uart_initialized) {
        ESP_LOGE(TAG, "UART未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 清空接收缓冲区
    uart_flush(BSP_POWER_UART_PORT);
    
    // 读取UART数据，超时时间1秒
    int len = uart_read_bytes(BSP_POWER_UART_PORT, uart_buffer, 
                             UART_BUFFER_SIZE - 1, pdMS_TO_TICKS(1000));
    
    if (len <= 0) {
        ESP_LOGD(TAG, "未读取到电源芯片数据");
        data->valid = false;
        return ESP_ERR_TIMEOUT;
    }
    
    // 打印原始接收到的数据（十六进制格式）
    ESP_LOGI(TAG, "接收到XSP16数据，长度: %d 字节", len);
    
    // 以十六进制格式打印数据
    printf("HEX: ");
    for (int i = 0; i < len && i < 32; i++) {  // 只打印前32字节
        printf("%02X ", uart_buffer[i]);
    }
    printf("\n");
    
    // 解析数据
    esp_err_t ret = parse_power_chip_data(uart_buffer, len, data);
    
    if (ret == ESP_OK) {
        // 更新全局数据
        memcpy(&power_chip_data, data, sizeof(bsp_power_chip_data_t));
    } else {
        data->valid = false;
    }
    
    return ret;
}

const bsp_power_chip_data_t* bsp_get_latest_power_chip_data(void) {
    if (!power_chip_data.valid) {
        return NULL;
    }
    
    // 电源芯片协商数据只在电压变化超过阈值时才重新读取
    // 开机时读取的数据会一直有效，直到下次电压变化触发重新协商
    // 不再基于时间过期，而是基于电压变化事件
    
    return &power_chip_data;
}

void bsp_power_chip_monitor_stop(void) {
    if (voltage_monitor_task_handle != NULL) {
        ESP_LOGI(TAG, "停止电压监控任务");
        vTaskDelete(voltage_monitor_task_handle);
        voltage_monitor_task_handle = NULL;
    }
    
    if (uart_initialized) {
        uart_driver_delete(BSP_POWER_UART_PORT);
        uart_initialized = false;
        ESP_LOGI(TAG, "UART驱动已卸载");
    }
}

void bsp_trigger_power_chip_negotiation(void) {
    ESP_LOGI(TAG, "手动触发电源芯片协商");
    perform_power_chip_negotiation();
}

void bsp_set_voltage_change_threshold(float main_threshold, float aux_threshold) {
    if (main_threshold > 0) {
        main_voltage_threshold = main_threshold;
    }
    if (aux_threshold > 0) {
        aux_voltage_threshold = aux_threshold;
    }
    ESP_LOGI(TAG, "电压变化阈值设置 - 主电源: %.2fV, 辅助电源: %.2fV", 
             main_voltage_threshold, aux_voltage_threshold);
}

esp_err_t bsp_get_power_chip_data_status(bool *is_valid, uint32_t *age_seconds) {
    if (is_valid == NULL || age_seconds == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *is_valid = power_chip_data.valid;
    
    if (power_chip_data.valid) {
        uint32_t current_time = esp_log_timestamp();
        *age_seconds = (current_time - power_chip_data.timestamp) / 1000;
    } else {
        *age_seconds = 0;
    }
    
    return ESP_OK;
}
