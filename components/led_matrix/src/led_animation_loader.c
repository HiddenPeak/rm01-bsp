#include "led_animation_loader.h"
#include "led_animation.h"
#include "bsp_storage.h"
#include "esp_log.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "LED_ANIM_LOADER";

// 动画配置参数
#define MAX_ANIMATIONS 10
#define MAX_POINTS_PER_ANIMATION 200
#define MAX_FILE_SIZE (64 * 1024) // 64KB最大文件大小

// Bresenham直线算法
static void draw_line(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    int x = x1, y = y1;
    
    while (true) {
        led_animation_set_point(x, y, r, g, b);
        
        if (x == x2 && y == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

// 解析单个点
static esp_err_t parse_point(cJSON *point_json) {
    if (!cJSON_IsObject(point_json)) {
        ESP_LOGE(TAG, "点不是有效的JSON对象");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 获取点类型，默认为"point"
    cJSON *type_json = cJSON_GetObjectItem(point_json, "type");
    const char *type = "point"; // 默认类型
    if (cJSON_IsString(type_json)) {
        type = type_json->valuestring;
    }
    
    // 获取颜色
    cJSON *r_json = cJSON_GetObjectItem(point_json, "r");
    cJSON *g_json = cJSON_GetObjectItem(point_json, "g");
    cJSON *b_json = cJSON_GetObjectItem(point_json, "b");
    
    if (!cJSON_IsNumber(r_json) || !cJSON_IsNumber(g_json) || !cJSON_IsNumber(b_json)) {
        ESP_LOGE(TAG, "颜色值无效");
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t r = (uint8_t)r_json->valueint;
    uint8_t g = (uint8_t)g_json->valueint;
    uint8_t b = (uint8_t)b_json->valueint;
    
    if (strcmp(type, "point") == 0) {
        // 单点
        cJSON *x_json = cJSON_GetObjectItem(point_json, "x");
        cJSON *y_json = cJSON_GetObjectItem(point_json, "y");
        
        if (!cJSON_IsNumber(x_json) || !cJSON_IsNumber(y_json)) {
            ESP_LOGE(TAG, "点坐标无效");
            return ESP_ERR_INVALID_ARG;
        }
        
        int x = x_json->valueint;
        int y = y_json->valueint;
        
        led_animation_set_point(x, y, r, g, b);
        
    } else if (strcmp(type, "line") == 0) {
        // 直线
        cJSON *x1_json = cJSON_GetObjectItem(point_json, "x1");
        cJSON *y1_json = cJSON_GetObjectItem(point_json, "y1");
        cJSON *x2_json = cJSON_GetObjectItem(point_json, "x2");
        cJSON *y2_json = cJSON_GetObjectItem(point_json, "y2");
        
        if (!cJSON_IsNumber(x1_json) || !cJSON_IsNumber(y1_json) ||
            !cJSON_IsNumber(x2_json) || !cJSON_IsNumber(y2_json)) {
            ESP_LOGE(TAG, "直线坐标无效");
            return ESP_ERR_INVALID_ARG;
        }
        
        int x1 = x1_json->valueint;
        int y1 = y1_json->valueint;
        int x2 = x2_json->valueint;
        int y2 = y2_json->valueint;
        
        draw_line(x1, y1, x2, y2, r, g, b);
        
    } else {
        ESP_LOGW(TAG, "未知的点类型: %s", type);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    return ESP_OK;
}

// 解析单个动画
static esp_err_t parse_animation(cJSON *animation_json) {
    if (!cJSON_IsObject(animation_json)) {
        ESP_LOGE(TAG, "动画不是有效的JSON对象");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 获取动画名称
    cJSON *name_json = cJSON_GetObjectItem(animation_json, "name");
    const char *name = "未命名动画";
    if (cJSON_IsString(name_json)) {
        name = name_json->valuestring;
    }
    
    ESP_LOGI(TAG, "解析动画: %s", name);
    
    // 获取点数组
    cJSON *points_json = cJSON_GetObjectItem(animation_json, "points");
    if (!cJSON_IsArray(points_json)) {
        ESP_LOGE(TAG, "动画点不是有效的数组");
        return ESP_ERR_INVALID_ARG;
    }
    
    int points_count = cJSON_GetArraySize(points_json);
    ESP_LOGI(TAG, "动画包含 %d 个点", points_count);
    
    // 限制点数量
    if (points_count > MAX_POINTS_PER_ANIMATION) {
        ESP_LOGW(TAG, "动画点数量 (%d) 超过限制 (%d)，将忽略多余的点", 
                points_count, MAX_POINTS_PER_ANIMATION);
        points_count = MAX_POINTS_PER_ANIMATION;
    }
    
    // 清除之前的动画点
    led_animation_clear_points();
    
    // 解析每个点
    int parsed_points = 0;
    for (int i = 0; i < points_count; i++) {
        cJSON *point_json = cJSON_GetArrayItem(points_json, i);
        if (parse_point(point_json) == ESP_OK) {
            parsed_points++;
        }
    }
    
    ESP_LOGI(TAG, "成功解析 %d 个点", parsed_points);
    return ESP_OK;
}

// 从JSON文件加载动画
esp_err_t load_animation_from_json(const char *filename) {
    ESP_LOGI(TAG, "从JSON文件加载动画: %s", filename);
    
    // 检查SD卡是否挂载
    if (!bsp_storage_sdcard_is_mounted()) {
        ESP_LOGE(TAG, "SD卡未挂载，无法加载动画");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 检查文件是否存在
    struct stat file_stat;
    if (stat(filename, &file_stat) != 0) {
        ESP_LOGE(TAG, "文件不存在: %s", filename);
        return ESP_ERR_NOT_FOUND;
    }
    
    // 检查文件大小
    if (file_stat.st_size > MAX_FILE_SIZE) {
        ESP_LOGE(TAG, "文件太大: %ld 字节 (最大 %d 字节)", file_stat.st_size, MAX_FILE_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 打开文件
    FILE *file = fopen(filename, "r");
    if (!file) {
        ESP_LOGE(TAG, "无法打开文件: %s", filename);
        return ESP_ERR_INVALID_ARG;
    }
    
    // 分配内存读取文件
    char *json_buffer = malloc(file_stat.st_size + 1);
    if (!json_buffer) {
        ESP_LOGE(TAG, "无法分配内存 (%ld 字节)", file_stat.st_size + 1);
        fclose(file);
        return ESP_ERR_NO_MEM;
    }
    
    // 读取文件内容
    size_t read_size = fread(json_buffer, 1, file_stat.st_size, file);
    fclose(file);
    
    if (read_size != file_stat.st_size) {
        ESP_LOGE(TAG, "读取文件失败，期望 %ld 字节，实际读取 %d 字节", 
                file_stat.st_size, read_size);
        free(json_buffer);
        return ESP_ERR_INVALID_SIZE;
    }
    
    json_buffer[file_stat.st_size] = '\0'; // 确保字符串结束
    
    ESP_LOGI(TAG, "成功读取文件 %ld 字节", file_stat.st_size);
    
    // 解析JSON
    cJSON *root = cJSON_Parse(json_buffer);
    free(json_buffer);
    
    if (!root) {
        const char *error_ptr = cJSON_GetErrorPtr();
        ESP_LOGE(TAG, "JSON解析失败: %s", error_ptr ? error_ptr : "未知错误");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 获取动画数组
    cJSON *animations = cJSON_GetObjectItem(root, "animations");
    if (!cJSON_IsArray(animations)) {
        ESP_LOGE(TAG, "根对象中没有animations数组");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    
    int animations_count = cJSON_GetArraySize(animations);
    ESP_LOGI(TAG, "文件包含 %d 个动画", animations_count);
    
    if (animations_count == 0) {
        ESP_LOGW(TAG, "文件中没有动画");
        cJSON_Delete(root);
        return ESP_ERR_NOT_FOUND;
    }
    
    // 限制动画数量
    if (animations_count > MAX_ANIMATIONS) {
        ESP_LOGW(TAG, "动画数量 (%d) 超过限制 (%d)，将只加载第一个动画", 
                animations_count, MAX_ANIMATIONS);
        animations_count = 1; // 只加载第一个动画
    }
    
    // 目前只加载第一个动画
    cJSON *animation = cJSON_GetArrayItem(animations, 0);
    esp_err_t result = parse_animation(animation);
    
    cJSON_Delete(root);
    
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "动画加载成功");
    } else {
        ESP_LOGE(TAG, "动画加载失败");
    }
    
    return result;
}

// 检查动画文件是否存在
bool animation_file_exists(const char *filename) {
    if (!bsp_storage_sdcard_is_mounted()) {
        return false;
    }
    
    struct stat file_stat;
    return (stat(filename, &file_stat) == 0);
}
