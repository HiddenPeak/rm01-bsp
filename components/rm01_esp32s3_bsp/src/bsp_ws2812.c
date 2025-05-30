#include "bsp_ws2812.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BSP_WS2812";

// WS2812全局句柄数组
static led_strip_handle_t ws2812_handles[BSP_WS2812_MAX] = {NULL};

// WS2812配置信息
typedef struct {
    uint8_t gpio_num;
    uint32_t max_leds;
} ws2812_config_t;

static const ws2812_config_t ws2812_configs[BSP_WS2812_MAX] = {
    [BSP_WS2812_ONBOARD] = {BSP_WS2812_ONBOARD_PIN, BSP_WS2812_ONBOARD_COUNT},
    [BSP_WS2812_TOUCH]   = {BSP_WS2812_Touch_LED_PIN, BSP_WS2812_Touch_LED_COUNT},
};

esp_err_t bsp_ws2812_init(bsp_ws2812_type_t type)
{
    if (type >= BSP_WS2812_MAX) {
        ESP_LOGE(TAG, "Invalid WS2812 type: %d", type);
        return ESP_ERR_INVALID_ARG;
    }

    if (ws2812_handles[type] != NULL) {
        ESP_LOGW(TAG, "WS2812 type %d already initialized", type);
        return ESP_OK;
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = ws2812_configs[type].gpio_num,
        .max_leds = ws2812_configs[type].max_leds,
        .led_model = LED_MODEL_WS2812,
    };
    
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };
    
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &ws2812_handles[type]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WS2812 type %d init failed: %s", type, esp_err_to_name(ret));
        return ret;
    }

    // 初始化后清除所有LED
    led_strip_clear(ws2812_handles[type]);
    
    ESP_LOGI(TAG, "WS2812 type %d initialized successfully (GPIO:%d, LEDs:%ld)", 
             type, ws2812_configs[type].gpio_num, ws2812_configs[type].max_leds);
    
    return ESP_OK;
}

esp_err_t bsp_ws2812_init_all(void)
{
    esp_err_t ret = ESP_OK;
    
    for (int i = 0; i < BSP_WS2812_MAX; i++) {
        esp_err_t init_ret = bsp_ws2812_init((bsp_ws2812_type_t)i);
        if (init_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize WS2812 type %d", i);
            ret = init_ret;
        }
    }
    
    return ret;
}

esp_err_t bsp_ws2812_deinit(bsp_ws2812_type_t type)
{
    if (type >= BSP_WS2812_MAX) {
        ESP_LOGE(TAG, "Invalid WS2812 type: %d", type);
        return ESP_ERR_INVALID_ARG;
    }

    if (ws2812_handles[type] == NULL) {
        ESP_LOGW(TAG, "WS2812 type %d not initialized", type);
        return ESP_OK;
    }

    esp_err_t ret = led_strip_del(ws2812_handles[type]);
    if (ret == ESP_OK) {
        ws2812_handles[type] = NULL;
        ESP_LOGI(TAG, "WS2812 type %d deinitialized", type);
    } else {
        ESP_LOGE(TAG, "Failed to deinitialize WS2812 type %d: %s", type, esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t bsp_ws2812_deinit_all(void)
{
    esp_err_t ret = ESP_OK;
    
    for (int i = 0; i < BSP_WS2812_MAX; i++) {
        esp_err_t deinit_ret = bsp_ws2812_deinit((bsp_ws2812_type_t)i);
        if (deinit_ret != ESP_OK) {
            ret = deinit_ret;
        }
    }
    
    return ret;
}

esp_err_t bsp_ws2812_set_pixel(bsp_ws2812_type_t type, uint32_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    if (type >= BSP_WS2812_MAX) {
        ESP_LOGE(TAG, "Invalid WS2812 type: %d", type);
        return ESP_ERR_INVALID_ARG;
    }

    if (ws2812_handles[type] == NULL) {
        ESP_LOGE(TAG, "WS2812 type %d not initialized", type);
        return ESP_ERR_INVALID_STATE;
    }

    if (index >= ws2812_configs[type].max_leds) {
        ESP_LOGE(TAG, "Index %ld out of range for WS2812 type %d (max: %ld)", 
                 index, type, ws2812_configs[type].max_leds);
        return ESP_ERR_INVALID_ARG;
    }

    return led_strip_set_pixel(ws2812_handles[type], index, red, green, blue);
}

esp_err_t bsp_ws2812_refresh(bsp_ws2812_type_t type)
{
    if (type >= BSP_WS2812_MAX) {
        ESP_LOGE(TAG, "Invalid WS2812 type: %d", type);
        return ESP_ERR_INVALID_ARG;
    }

    if (ws2812_handles[type] == NULL) {
        ESP_LOGE(TAG, "WS2812 type %d not initialized", type);
        return ESP_ERR_INVALID_STATE;
    }

    return led_strip_refresh(ws2812_handles[type]);
}

esp_err_t bsp_ws2812_clear(bsp_ws2812_type_t type)
{
    if (type >= BSP_WS2812_MAX) {
        ESP_LOGE(TAG, "Invalid WS2812 type: %d", type);
        return ESP_ERR_INVALID_ARG;
    }

    if (ws2812_handles[type] == NULL) {
        ESP_LOGE(TAG, "WS2812 type %d not initialized", type);
        return ESP_ERR_INVALID_STATE;
    }

    return led_strip_clear(ws2812_handles[type]);
}

led_strip_handle_t bsp_ws2812_get_handle(bsp_ws2812_type_t type)
{
    if (type >= BSP_WS2812_MAX) {
        ESP_LOGE(TAG, "Invalid WS2812 type: %d", type);
        return NULL;
    }

    return ws2812_handles[type];
}

void bsp_ws2812_onboard_test(void)
{
    if (ws2812_handles[BSP_WS2812_ONBOARD] == NULL) {
        ESP_LOGE(TAG, "Onboard WS2812 not initialized");
        return;
    }

    ESP_LOGI(TAG, "Starting onboard WS2812 test");
    
    for (int i = 0; i < BSP_WS2812_ONBOARD_COUNT; i++) {
        bsp_ws2812_set_pixel(BSP_WS2812_ONBOARD, i, 255, 0, 0); // 红色
        bsp_ws2812_refresh(BSP_WS2812_ONBOARD);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    bsp_ws2812_clear(BSP_WS2812_ONBOARD);
    bsp_ws2812_refresh(BSP_WS2812_ONBOARD);
    
    ESP_LOGI(TAG, "Onboard WS2812 test completed");
}

void bsp_ws2812_touch_test(void)
{
    if (ws2812_handles[BSP_WS2812_TOUCH] == NULL) {
        ESP_LOGE(TAG, "Touch WS2812 not initialized");
        return;
    }

    ESP_LOGI(TAG, "Starting touch WS2812 test");
    
    // 测试触摸传感器上的ws2812灯珠
    // 创建白色呼吸灯效果，持续2500ms
    uint8_t brightness = 0;
    bool increasing = true;
    uint32_t start_time = xTaskGetTickCount();
    uint32_t duration_ticks = pdMS_TO_TICKS(1000);
    
    while ((xTaskGetTickCount() - start_time) < duration_ticks) {
        // 设置所有LED为当前亮度的白色
        for (int i = 0; i < BSP_WS2812_Touch_LED_COUNT; i++) {
            bsp_ws2812_set_pixel(BSP_WS2812_TOUCH, i, brightness, brightness, brightness);
        }
        bsp_ws2812_refresh(BSP_WS2812_TOUCH);
        
        // 更新亮度
        if (increasing) {
            brightness += 5;
            if (brightness >= 250) increasing = false;
        } else {
            brightness -= 5;
            if (brightness <= 5) increasing = true;
        }
        
        vTaskDelay(20 / portTICK_PERIOD_MS); // 控制呼吸速度
    }
    
    bsp_ws2812_clear(BSP_WS2812_TOUCH);
    bsp_ws2812_refresh(BSP_WS2812_TOUCH);
    
    ESP_LOGI(TAG, "Touch WS2812 test completed");
}
