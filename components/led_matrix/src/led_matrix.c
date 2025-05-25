#include "led_matrix.h"
#include "led_animation.h"
#include "led_color.h"
#include "led_strip.h"
#include "esp_log.h"
#include <string.h>

// 添加TF卡和动画相关头文件
#include "bsp_storage.h"
#include "led_animation_demo.h"
#include "led_animation_export.h"
#include "led_animation_loader.h"

static const char *TAG = "LED_MATRIX";

// 前向声明
static void init_animation_from_storage(void);

// 矩阵LED数据
static led_strip_handle_t led_strip;
static uint8_t led_grid[LED_MATRIX_HEIGHT][LED_MATRIX_WIDTH][3]; // RGB网格
static bool matrix_enabled = true;

// 初始化LED矩阵
void led_matrix_init(void) {
    ESP_LOGI(TAG, "初始化LED矩阵 (%dx%d)", LED_MATRIX_WIDTH, LED_MATRIX_HEIGHT);
    
    // 配置LED带
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_MATRIX_GPIO_PIN,
        .max_leds = LED_MATRIX_NUM_LEDS,
        .led_model = LED_MODEL_WS2812,
    };
    
    // 配置RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    
    // 创建LED带设备
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED矩阵初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    
    // 清空网格和LED
    led_matrix_clear();
    
    // 初始化动画系统
    led_animation_init();
    
    // 初始化动画数据（从TF卡加载或使用示例）
    init_animation_from_storage();
    
    ESP_LOGI(TAG, "LED矩阵初始化完成");
}

// 清除所有LED
void led_matrix_clear(void) {
    // 清空内部网格数据
    memset(led_grid, 0, sizeof(led_grid));
    
    // 清空LED带
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
}

// 设置单个像素
void led_matrix_set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    // 边界检查
    if (x < 0 || x >= LED_MATRIX_WIDTH || y < 0 || y >= LED_MATRIX_HEIGHT) {
        return;
    }
    
    // 设置网格中的像素值
    led_grid[y][x][0] = r;
    led_grid[y][x][1] = g;
    led_grid[y][x][2] = b;
}

// 获取单个像素
void led_matrix_get_pixel(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    // 边界检查
    if (x < 0 || x >= LED_MATRIX_WIDTH || y < 0 || y >= LED_MATRIX_HEIGHT) {
        *r = *g = *b = 0;
        return;
    }
    
    // 获取网格中的像素值
    *r = led_grid[y][x][0];
    *g = led_grid[y][x][1];
    *b = led_grid[y][x][2];
}

// 更新显示（刷新整个矩阵）
void led_matrix_refresh(void) {
    // 如果矩阵被禁用，不执行刷新
    if (!matrix_enabled) {
        return;
    }
    
    // 将网格数据映射到LED带上
    for (int y = 0; y < LED_MATRIX_HEIGHT; y++) {
        for (int x = 0; x < LED_MATRIX_WIDTH; x++) {
            int led_index = y * LED_MATRIX_WIDTH + x; // 简单的行优先布局
            
            // 应用颜色校准
            rgb_t color = color_correct(led_grid[y][x][0], led_grid[y][x][1], led_grid[y][x][2]);
            
            // 设置LED像素
            led_strip_set_pixel(led_strip, led_index, color.r, color.g, color.b);
        }
    }
    
    // 刷新LED带
    led_strip_refresh(led_strip);
}

// 填充全部
void led_matrix_fill(uint8_t r, uint8_t g, uint8_t b) {
    // 填充网格
    for (int y = 0; y < LED_MATRIX_HEIGHT; y++) {
        for (int x = 0; x < LED_MATRIX_WIDTH; x++) {
            led_grid[y][x][0] = r;
            led_grid[y][x][1] = g;
            led_grid[y][x][2] = b;
        }
    }
}

// 动画更新（将在动画模块中实现的包装器）
void led_matrix_update_animation(void) {
    if (matrix_enabled) {
        led_animation_update();
    }
}

// 启用/禁用矩阵更新
void led_matrix_set_enabled(bool enabled) {
    matrix_enabled = enabled;
    
    // 如果禁用，立即清空矩阵
    if (!enabled) {
        led_strip_clear(led_strip);
        led_strip_refresh(led_strip);
    }
}

// 检查矩阵是否启用
bool led_matrix_is_enabled(void) {
    return matrix_enabled;
}

// 简单测试模式
void led_matrix_test(void) {
    ESP_LOGI(TAG, "运行LED矩阵测试");
    
    // 测试1：依次点亮白色
    for (int i = 0; i < LED_MATRIX_NUM_LEDS; i++) {
        led_strip_set_pixel(led_strip, i, 64, 64, 64); // 白色
        led_strip_refresh(led_strip);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
    
    // 测试2：红绿蓝测试
    led_matrix_fill(64, 0, 0); // 红色
    led_matrix_refresh();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    led_matrix_fill(0, 64, 0); // 绿色
    led_matrix_refresh();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    led_matrix_fill(0, 0, 64); // 蓝色
    led_matrix_refresh();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    // 清空
    led_matrix_clear();
    ESP_LOGI(TAG, "LED矩阵测试完成");
}

// 从存储设备初始化动画数据
static void init_animation_from_storage(void) {
    ESP_LOGI(TAG, "开始初始化动画数据");
    
    // 挂载TF卡
    esp_err_t mount_ret = bsp_storage_sdcard_mount(MOUNT_POINT);
    if (mount_ret == ESP_OK) {
        ESP_LOGI(TAG, "TF卡挂载成功");
        
        // 检查是否存在动画文件
        if (animation_file_exists(ANIMATION_FILE_PATH)) {
            ESP_LOGI(TAG, "发现动画文件，正在加载...");
            esp_err_t load_ret = load_animation_from_json(ANIMATION_FILE_PATH);
            if (load_ret == ESP_OK) {
                ESP_LOGI(TAG, "从TF卡成功加载动画");
                return; // 成功加载，直接返回
            } else {
                ESP_LOGW(TAG, "动画文件加载失败，使用内置示例动画");
            }
        } else {
            ESP_LOGI(TAG, "未找到动画文件，导出示例动画并使用");
            // 导出示例动画到TF卡
            initialize_animation_demo();
            esp_err_t export_ret = export_animation_to_json(ANIMATION_FILE_PATH);
            if (export_ret == ESP_OK) {
                ESP_LOGI(TAG, "成功导出示例动画到TF卡：%s", ANIMATION_FILE_PATH);
            } else {
                ESP_LOGW(TAG, "导出示例动画失败");
            }
            return; // 已经初始化了示例动画
        }
    } else {
        ESP_LOGW(TAG, "TF卡挂载失败，使用内置示例动画");
    }
    
    // 如果TF卡挂载失败或动画加载失败，使用内置示例动画
    initialize_animation_demo();
    ESP_LOGI(TAG, "动画数据初始化完成");
}
