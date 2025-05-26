#include "led_matrix.h"
#include "led_animation.h"
#include "led_color.h"
#include "led_strip.h"
#include "esp_log.h"
#include <string.h>
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

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

// 添加互斥锁保护LED strip访问
static SemaphoreHandle_t led_strip_mutex = NULL;

// 强制重置RMT系统
static esp_err_t led_matrix_force_reset_rmt(void) {
    ESP_LOGI(TAG, "强制重置RMT系统...");
    
    // 如果已存在句柄，强制删除
    if (led_strip != NULL) {
        ESP_LOGI(TAG, "强制删除现有LED strip句柄");
        // 忽略删除错误，强制设为NULL
        led_strip_del(led_strip);
        led_strip = NULL;
    }
    
    // 长延迟确保RMT硬件完全重置
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    return ESP_OK;
}

// 重置RMT系统
static esp_err_t led_matrix_reset_rmt(void) {
    ESP_LOGI(TAG, "重置RMT系统...");
    
    // 如果已存在句柄，先删除
    if (led_strip != NULL) {
        ESP_LOGI(TAG, "删除现有LED strip句柄");
        esp_err_t del_ret = led_strip_del(led_strip);
        if (del_ret != ESP_OK) {
            ESP_LOGW(TAG, "删除LED strip失败: %s", esp_err_to_name(del_ret));
        }
        led_strip = NULL;
        
        // 延迟确保硬件完全释放
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    
    return ESP_OK;
}

// 强健的LED strip创建函数，支持重试
static esp_err_t led_matrix_create_strip_robust(void) {
    const int MAX_RETRIES = 5;
    esp_err_t ret = ESP_FAIL;
    
    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        ESP_LOGI(TAG, "尝试创建LED strip (第%d次)...", attempt + 1);
        
        // 如果不是第一次尝试，执行强制重置
        if (attempt > 0) {
            ESP_LOGI(TAG, "执行强制RMT重置...");
            led_matrix_force_reset_rmt();
        }
        
        // 配置LED带
        led_strip_config_t strip_config = {
            .strip_gpio_num = LED_MATRIX_GPIO_PIN,
            .max_leds = LED_MATRIX_NUM_LEDS,
            .led_model = LED_MODEL_WS2812,
        };
        
        // 配置RMT - 使用更保守的设置
        led_strip_rmt_config_t rmt_config = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = 10 * 1000 * 1000, // 10MHz
            .flags.with_dma = false,
            .mem_block_symbols = 64, // 显式设置内存块大小
        };
        
        // 创建LED带设备
        ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "LED strip创建成功");
            
            // 给RMT驱动充足时间来完全初始化
            vTaskDelay(200 / portTICK_PERIOD_MS);
            
            // 简化测试：只尝试一次基本操作
            esp_err_t test_ret = led_strip_clear(led_strip);
            if (test_ret == ESP_OK) {
                ESP_LOGI(TAG, "LED strip基本功能测试通过");
                return ESP_OK;
            } else {
                ESP_LOGW(TAG, "LED strip清除测试失败: %s", esp_err_to_name(test_ret));
                // 测试失败，删除并重试
                led_strip_del(led_strip);
                led_strip = NULL;
            }
        } else {
            ESP_LOGW(TAG, "创建LED strip失败: %s", esp_err_to_name(ret));
        }
        
        // 重试前等待
        if (attempt < MAX_RETRIES - 1) {
            int delay_ms = (attempt + 1) * 1000; // 递增延迟
            ESP_LOGI(TAG, "等待%dms后重试...", delay_ms);
            vTaskDelay(delay_ms / portTICK_PERIOD_MS);
        }
    }
    
    ESP_LOGE(TAG, "LED strip创建失败，已尝试%d次", MAX_RETRIES);
    return ret;
}

// 初始化LED矩阵
void led_matrix_init(void) {
    ESP_LOGI(TAG, "初始化LED矩阵 (%dx%d)", LED_MATRIX_WIDTH, LED_MATRIX_HEIGHT);
    
    // 创建互斥锁
    if (led_strip_mutex == NULL) {
        led_strip_mutex = xSemaphoreCreateMutex();
        if (led_strip_mutex == NULL) {
            ESP_LOGE(TAG, "创建LED strip互斥锁失败");
            return;
        }
    }
    
    // 暂时禁用矩阵更新，防止动画任务干扰初始化
    matrix_enabled = false;
    
    // 确保led_strip句柄为空，避免重复初始化
    if (led_strip != NULL) {
        ESP_LOGW(TAG, "LED矩阵已经初始化，执行强制重新初始化");
        led_matrix_force_reset_rmt(); // 使用强制重置
    } else {
        // 即使没有现有句柄，也进行强制重置确保RMT干净状态
        ESP_LOGI(TAG, "执行预防性RMT重置");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    
    ESP_LOGI(TAG, "正在配置GPIO %d 用于LED矩阵...", LED_MATRIX_GPIO_PIN);
    
    // 强健的LED strip创建
    esp_err_t ret = led_matrix_create_strip_robust();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED矩阵初始化失败，无法创建LED strip");
        return;
    }
    
    // 清空网格数据
    memset(led_grid, 0, sizeof(led_grid));
    
    // 重新启用矩阵更新
    matrix_enabled = true;
    
    // 初始化动画系统
    led_animation_init();
    
    // 初始化动画数据（从TF卡加载或使用示例）
    init_animation_from_storage();
    
    ESP_LOGI(TAG, "LED矩阵初始化完成");
}

// 带重试和互斥锁保护的LED strip操作
static esp_err_t led_strip_operation_with_retry(const char* operation_name, 
                                                 esp_err_t (*operation)(led_strip_handle_t), 
                                                 int max_retries) {
    if (led_strip == NULL) {
        ESP_LOGE(TAG, "%s失败: LED strip未初始化", operation_name);
        return ESP_ERR_INVALID_STATE;
    }
    
    if (led_strip_mutex == NULL) {
        ESP_LOGE(TAG, "%s失败: 互斥锁未初始化", operation_name);
        return ESP_ERR_INVALID_STATE;
    }
    
    // 获取互斥锁，超时500ms
    if (xSemaphoreTake(led_strip_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        ESP_LOGE(TAG, "%s失败: 无法获取互斥锁", operation_name);
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t ret = ESP_FAIL;
    for (int attempt = 0; attempt < max_retries; attempt++) {
        ret = operation(led_strip);
        if (ret == ESP_OK) {
            if (attempt > 0) {
                ESP_LOGI(TAG, "%s在第%d次重试后成功", operation_name, attempt + 1);
            }
            xSemaphoreGive(led_strip_mutex);
            return ESP_OK;
        }
        
        ESP_LOGW(TAG, "%s失败 (尝试%d/%d): %s", operation_name, attempt + 1, max_retries, esp_err_to_name(ret));
        
        // 如果是RMT通道状态错误，尝试重新初始化
        if (ret == ESP_ERR_INVALID_STATE && attempt < max_retries - 1) {
            ESP_LOGW(TAG, "检测到RMT状态错误，尝试重新初始化LED strip...");
            
            // 重新初始化（在互斥锁保护下）
            led_matrix_reset_rmt();
            esp_err_t reinit_ret = led_matrix_create_strip_robust();
            if (reinit_ret != ESP_OK) {
                ESP_LOGE(TAG, "重新初始化LED strip失败");
                break;
            }
            
            ESP_LOGI(TAG, "LED strip重新初始化成功，继续重试操作");
        } else if (attempt < max_retries - 1) {
            // 其他错误，简单延迟后重试
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
    
    ESP_LOGE(TAG, "%s最终失败: %s", operation_name, esp_err_to_name(ret));
    xSemaphoreGive(led_strip_mutex);
    return ret;
}

// 清除所有LED
void led_matrix_clear(void) {
    // 清空内部网格数据
    memset(led_grid, 0, sizeof(led_grid));
    
    // 检查LED带是否已初始化
    if (led_strip == NULL) {
        ESP_LOGW(TAG, "LED矩阵未初始化，跳过清除操作");
        return;
    }
    
    // 带重试的清除操作
    esp_err_t ret = led_strip_operation_with_retry("LED strip清除", led_strip_clear, 3);
    if (ret == ESP_OK) {
        // 只有clear成功才尝试refresh
        led_strip_operation_with_retry("LED strip刷新", led_strip_refresh, 3);
    }
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
    
    // 检查LED带是否已初始化
    if (led_strip == NULL) {
        ESP_LOGE(TAG, "LED矩阵未初始化，无法刷新");
        return;
    }
    
    // 检查互斥锁
    if (led_strip_mutex == NULL) {
        ESP_LOGE(TAG, "LED strip互斥锁未初始化，无法刷新");
        return;
    }
    
    // 获取互斥锁，超时100ms（较短超时避免动画卡顿）
    if (xSemaphoreTake(led_strip_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "LED矩阵刷新：无法获取互斥锁，跳过本次刷新");
        return;
    }
    
    // 将网格数据映射到LED带上
    bool pixel_error = false;
    for (int y = 0; y < LED_MATRIX_HEIGHT && !pixel_error; y++) {
        for (int x = 0; x < LED_MATRIX_WIDTH && !pixel_error; x++) {
            int led_index = y * LED_MATRIX_WIDTH + x; // 简单的行优先布局
            
            // 应用颜色校准
            rgb_t color = color_correct(led_grid[y][x][0], led_grid[y][x][1], led_grid[y][x][2]);
            
            // 设置LED像素
            esp_err_t ret = led_strip_set_pixel(led_strip, led_index, color.r, color.g, color.b);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "设置像素 [%d,%d] (LED %d) 失败: %s", x, y, led_index, esp_err_to_name(ret));
                pixel_error = true;
            }
        }
    }
    
    // 如果像素设置成功，尝试刷新
    if (!pixel_error) {
        esp_err_t ret = led_strip_refresh(led_strip);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "LED矩阵刷新失败: %s", esp_err_to_name(ret));
        }
    }
    
    // 释放互斥锁
    xSemaphoreGive(led_strip_mutex);
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

// 析构LED矩阵
void led_matrix_deinit(void) {
    if (led_strip != NULL) {
        ESP_LOGI(TAG, "清理LED矩阵资源...");
        led_strip_del(led_strip);
        led_strip = NULL;
    }
}
