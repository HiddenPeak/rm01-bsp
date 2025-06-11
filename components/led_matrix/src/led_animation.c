#include "led_animation.h"
#include "led_matrix.h"
#include "led_color.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <math.h>

static const char *TAG = "LED_ANIMATION";

// 动画配置常量
#define MAX_ANIMATIONS_STORAGE 10

// 单个动画数据结构
typedef struct {
    char name[64];  // 动画名称
    uint8_t mask[LED_MATRIX_HEIGHT][LED_MATRIX_WIDTH]; // 掩码，标记哪些像素应该被照亮
    uint8_t original_colors[LED_MATRIX_HEIGHT][LED_MATRIX_WIDTH][3]; // 每个点的原始颜色
    bool is_valid; // 动画是否有效
} animation_data_t;

// 动画系统数据
static animation_data_t animations[MAX_ANIMATIONS_STORAGE]; // 存储的动画数组
static int current_animation_index = 0; // 当前播放的动画索引
static int loaded_animations_count = 0; // 已加载的动画数量
static int flash_position = 0; // 闪光位置（从 0,0 开始）
static bool animation_running = true; // 动画是否正在运行
static uint8_t animation_speed = ANIMATION_SPEED; // 动画速度

// 初始化动画系统
void led_animation_init(void) {
    // 清空所有动画数据
    memset(animations, 0, sizeof(animations));
    for (int i = 0; i < MAX_ANIMATIONS_STORAGE; i++) {
        animations[i].is_valid = false;
    }
    
    current_animation_index = 0;
    loaded_animations_count = 0;
    flash_position = 0;
    animation_running = true;
    animation_speed = ANIMATION_SPEED;
    
    ESP_LOGI(TAG, "动画系统初始化完成");
}

// 获取当前动画的数据指针
static animation_data_t* get_current_animation(void) {
    if (loaded_animations_count == 0 || current_animation_index >= loaded_animations_count) {
        return NULL;
    }
    return &animations[current_animation_index];
}

// 获取当前动画的掩码
__attribute__((unused)) static uint8_t (*get_current_mask(void))[LED_MATRIX_WIDTH] {
    animation_data_t* current = get_current_animation();
    if (current == NULL) {
        return NULL;
    }
    return current->mask;
}

// 获取当前动画的颜色数组
__attribute__((unused)) static uint8_t (*get_current_colors(void))[LED_MATRIX_WIDTH][3] {
    animation_data_t* current = get_current_animation();
    if (current == NULL) {
        return NULL;
    }
    return current->original_colors;
}

// 设置动画点位置和颜色
void led_animation_set_point(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    // 边界检查
    if (x < 0 || x >= LED_MATRIX_WIDTH || y < 0 || y >= LED_MATRIX_HEIGHT) {
        return;
    }
    
    animation_data_t* current = get_current_animation();
    if (current == NULL) {
        return;
    }
    
    // 设置掩码和原始颜色
    current->mask[y][x] = 1;
    current->original_colors[y][x][0] = r;
    current->original_colors[y][x][1] = g;
    current->original_colors[y][x][2] = b;
}

// 更新动画点的颜色
void led_animation_update_point(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    // 边界检查
    if (x < 0 || x >= LED_MATRIX_WIDTH || y < 0 || y >= LED_MATRIX_HEIGHT) {
        return;
    }
    
    animation_data_t* current = get_current_animation();
    if (current == NULL) {
        return;
    }
    
    // 仅更新颜色，不改变掩码
    current->original_colors[y][x][0] = r;
    current->original_colors[y][x][1] = g;
    current->original_colors[y][x][2] = b;
}

// 清除所有动画点
void led_animation_clear_points(void) {
    animation_data_t* current = get_current_animation();
    if (current == NULL) {
        return;
    }
    
    memset(current->mask, 0, sizeof(current->mask));
    memset(current->original_colors, 0, sizeof(current->original_colors));
}

// 计算闪光亮度（基于到闪光中心线的距离）
static float calculate_flash_brightness(int y, int x, int flash_pos) {
    // 到对角线闪光线的距离
    // 对角线闪光线由方程表示：y = x - flash_pos
    // 点(x,y)到线y = x - flash_pos的距离为：
    // |y - x + flash_pos| / sqrt(2)
    float distance = fabsf((float)y - (float)x + (float)flash_pos) / 1.414f;
    
    // 计算亮度 - 中心处为全亮度，向边缘逐渐衰减
    if (distance < FLASH_WIDTH) {
        // 余弦亮度衰减，让光线效果更自然
        return cosf(distance * 3.14159f / (2.0f * FLASH_WIDTH));
    }
    
    return 0.0f; // 闪光宽度外无亮度
}

// 更新并渲染当前动画
void led_animation_update(void) {
    // 如果动画没有运行，不更新
    if (!animation_running) {
        return;
    }
    
    animation_data_t* current = get_current_animation();
    if (current == NULL) {
        // 没有可用动画，清空显示
        for (int y = 0; y < LED_MATRIX_HEIGHT; y++) {
            for (int x = 0; x < LED_MATRIX_WIDTH; x++) {
                led_matrix_set_pixel(x, y, 0, 0, 0);
            }
        }
        led_matrix_refresh();
        return;
    }
    
    // 清空网格
    for (int y = 0; y < LED_MATRIX_HEIGHT; y++) {
        for (int x = 0; x < LED_MATRIX_WIDTH; x++) {
            if (current->mask[y][x]) {
                // 应用原始颜色（经过亮度和饱和度调整）
                rgb_t adjusted = adjust_brightness_saturation(
                    current->original_colors[y][x][0], 
                    current->original_colors[y][x][1], 
                    current->original_colors[y][x][2]
                );
                led_matrix_set_pixel(x, y, adjusted.r, adjusted.g, adjusted.b);
            } else {
                led_matrix_set_pixel(x, y, 0, 0, 0);
            }
        }
    }
    
    // 更新闪光位置
    flash_position += animation_speed;
    
    // 当闪光离开屏幕时重置位置
    if (flash_position > LED_MATRIX_WIDTH + LED_MATRIX_HEIGHT + FLASH_WIDTH) {
        flash_position = 0; // 从(0,0)开始
    }
    
    // 应用闪光效果到掩码中的所有像素
    for (int y = 0; y < LED_MATRIX_HEIGHT; y++) {
        for (int x = 0; x < LED_MATRIX_WIDTH; x++) {
            if (current->mask[y][x]) {
                // 根据到闪光线的距离计算亮度
                float brightness = calculate_flash_brightness(y, x, flash_position);
                
                if (brightness > 0.0f) {
                    // 增强原始颜色的亮度
                    float brighten_factor = 1.0f + brightness * 1.5f; // 最大2.5倍亮度
                    
                    // 获取原始调整后的颜色
                    rgb_t adjusted = adjust_brightness_saturation(
                        current->original_colors[y][x][0], 
                        current->original_colors[y][x][1], 
                        current->original_colors[y][x][2]
                    );
                    
                    // 增加亮度
                    uint16_t r = (uint16_t)(adjusted.r * brighten_factor);
                    uint16_t g = (uint16_t)(adjusted.g * brighten_factor);
                    uint16_t b = (uint16_t)(adjusted.b * brighten_factor);
                    
                    // 限制到有效范围
                    r = (r > 255) ? 255 : r;
                    g = (g > 255) ? 255 : g;
                    b = (b > 255) ? 255 : b;
                    
                    // 设置像素
                    led_matrix_set_pixel(x, y, (uint8_t)r, (uint8_t)g, (uint8_t)b);
                }
            }
        }
    }
    
    // 刷新矩阵显示
    led_matrix_refresh();
}

// 暂停/继续动画
void led_animation_set_running(bool running) {
    animation_running = running;
}

// 检查动画是否运行中
bool led_animation_is_running(void) {
    return animation_running;
}

// 设置动画速度
void led_animation_set_speed(uint8_t speed) {
    animation_speed = speed;
}

// 获取动画速度
uint8_t led_animation_get_speed(void) {
    return animation_speed;
}

// ========== 多动画管理功能 ==========

// 创建新动画槽位
int led_animation_create_new(const char* name) {
    if (loaded_animations_count >= MAX_ANIMATIONS_STORAGE) {
        ESP_LOGE(TAG, "动画存储已满，无法创建新动画");
        return -1;
    }
    
    int index = loaded_animations_count;
    animation_data_t* new_anim = &animations[index];
    
    // 清空动画数据
    memset(new_anim, 0, sizeof(animation_data_t));
    
    // 设置动画名称
    if (name != NULL) {
        strncpy(new_anim->name, name, sizeof(new_anim->name) - 1);
        new_anim->name[sizeof(new_anim->name) - 1] = '\0';
    } else {
        snprintf(new_anim->name, sizeof(new_anim->name), "动画%d", index);
    }
    
    new_anim->is_valid = true;
    loaded_animations_count++;
    
    ESP_LOGI(TAG, "创建新动画: %s (索引: %d)", new_anim->name, index);
    return index;
}

// 选择当前播放的动画
esp_err_t led_animation_select(int animation_index) {
    if (animation_index < 0 || animation_index >= loaded_animations_count) {
        ESP_LOGE(TAG, "动画索引无效: %d (范围: 0-%d)", animation_index, loaded_animations_count - 1);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!animations[animation_index].is_valid) {
        ESP_LOGE(TAG, "动画无效: 索引 %d", animation_index);
        return ESP_ERR_INVALID_STATE;
    }
    
    current_animation_index = animation_index;
    flash_position = 0; // 重置闪光位置
    
    ESP_LOGI(TAG, "切换到动画: %s (索引: %d)", animations[animation_index].name, animation_index);
    return ESP_OK;
}

// 获取当前动画索引
int led_animation_get_current_index(void) {
    return current_animation_index;
}

// 获取已加载的动画数量
int led_animation_get_count(void) {
    return loaded_animations_count;
}

// 获取指定动画的名称
const char* led_animation_get_name(int animation_index) {
    if (animation_index < 0 || animation_index >= loaded_animations_count) {
        return NULL;
    }
    
    if (!animations[animation_index].is_valid) {
        return NULL;
    }
    
    return animations[animation_index].name;
}

// 切换到下一个动画
esp_err_t led_animation_next(void) {
    if (loaded_animations_count == 0) {
        ESP_LOGW(TAG, "没有可用的动画");
        return ESP_ERR_INVALID_STATE;
    }
    
    int next_index = (current_animation_index + 1) % loaded_animations_count;
    return led_animation_select(next_index);
}

// 切换到上一个动画
esp_err_t led_animation_previous(void) {
    if (loaded_animations_count == 0) {
        ESP_LOGW(TAG, "没有可用的动画");
        return ESP_ERR_INVALID_STATE;
    }
    
    int prev_index = (current_animation_index - 1 + loaded_animations_count) % loaded_animations_count;
    return led_animation_select(prev_index);
}

// 删除指定动画
esp_err_t led_animation_delete(int animation_index) {
    if (animation_index < 0 || animation_index >= loaded_animations_count) {
        ESP_LOGE(TAG, "动画索引无效: %d", animation_index);
        return ESP_ERR_INVALID_ARG;
    }
    
    // 标记动画为无效
    animations[animation_index].is_valid = false;
    
    // 如果删除的是当前动画，切换到下一个有效动画
    if (animation_index == current_animation_index) {
        // 寻找下一个有效动画
        bool found = false;
        for (int i = 0; i < loaded_animations_count; i++) {
            if (animations[i].is_valid) {
                current_animation_index = i;
                found = true;
                break;
            }
        }
        
        if (!found) {
            // 没有有效动画了
            current_animation_index = 0;
            ESP_LOGW(TAG, "删除最后一个动画，切换到空状态");
        }
    }
    
    ESP_LOGI(TAG, "删除动画索引: %d", animation_index);
    return ESP_OK;
}

// 清除所有动画
void led_animation_clear_all(void) {
    memset(animations, 0, sizeof(animations));
    for (int i = 0; i < MAX_ANIMATIONS_STORAGE; i++) {
        animations[i].is_valid = false;
    }
    
    current_animation_index = 0;
    loaded_animations_count = 0;
    flash_position = 0;
    
    ESP_LOGI(TAG, "清除所有动画");
}
