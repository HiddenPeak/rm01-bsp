#!/usr/bin/env python3
"""
动画数据同步工具
从 example_animation.json 中提取数据并更新 led_animation_export.c 中的硬编码动画定义
确保 C 代码与 JSON 数据完全一致
"""

import json
import os
import sys
from typing import List, Dict, Any

def load_json_animations(json_file: str) -> List[Dict[str, Any]]:
    """从JSON文件加载动画数据"""
    try:
        with open(json_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
        return data.get('animations', [])
    except Exception as e:
        print(f"加载JSON文件失败: {e}")
        return []

def generate_c_animation_array(animation: Dict[str, Any]) -> tuple[str, int]:
    """为单个动画生成C数组定义"""
    name = animation['name']
    points = animation['points']
    
    # 只处理点类型的数据，跳过line类型
    point_data = [p for p in points if p.get('type') == 'point']
    
    safe_name = name.replace('动画', 'animation').replace('中', 'zhong').replace('错误', 'error').replace('警告', 'warning').replace('计算', 'computing')
    if name == "示例动画":
        safe_name = "demo_animation"
    elif name == "启动中":
        safe_name = "startup_animation"
    elif name == "链接错误":
        safe_name = "error_animation"
    elif name == "高温警告":
        safe_name = "high_temp_warning_animation"
    elif name == "计算中":
        safe_name = "computing_animation"
    
    c_code = f"// {name}数据\n"
    c_code += f"static const animation_point_t {safe_name}_points[] = {{\n"
    
    # 按行分组点数据以便于阅读
    points_by_row = {}
    for point in point_data:
        y = point['y']
        if y not in points_by_row:
            points_by_row[y] = []
        points_by_row[y].append(point)
    
    # 按行输出
    for y in sorted(points_by_row.keys()):
        row_points = points_by_row[y]
        # 按x坐标排序
        row_points.sort(key=lambda p: p['x'])
        
        # 添加行注释
        c_code += f"    // 第{y}行\n"
        
        # 每行最多8个点，超过则换行
        line_points = []
        for point in row_points:
            point_str = f"{{{point['x']}, {point['y']}, {point['r']}, {point['g']}, {point['b']}}}"
            line_points.append(point_str)
            
            if len(line_points) >= 8:
                c_code += "    " + ", ".join(line_points) + ",\n"
                line_points = []
        
        # 输出剩余的点
        if line_points:
            c_code += "    " + ", ".join(line_points) + ",\n"
        
        c_code += "\n"
    
    # 移除最后的逗号和换行
    c_code = c_code.rstrip(',\n') + "\n"
    c_code += "};\n\n"
    
    return c_code, len(point_data)

def generate_full_c_code(animations: List[Dict[str, Any]]) -> str:
    """生成完整的C代码"""
    
    header = '''#include "led_animation_export.h"
#include "led_animation_demo.h"
#include "esp_log.h"
#include "bsp_storage.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "LED_ANIM_EXPORT";

// 动画点数据结构
typedef struct {
    int x;
    int y;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} animation_point_t;

// 动画定义结构
typedef struct {
    const char *name;
    const animation_point_t *points;
    int point_count;
} animation_definition_t;

'''
    
    c_code = header
    definition_entries = []
    
    # 生成每个动画的数组定义
    for animation in animations:
        anim_code, point_count = generate_c_animation_array(animation)
        c_code += anim_code
        
        name = animation['name']
        safe_name = name.replace('动画', 'animation').replace('中', 'zhong').replace('错误', 'error').replace('警告', 'warning').replace('计算', 'computing')
        if name == "示例动画":
            safe_name = "demo_animation"
        elif name == "启动中":
            safe_name = "startup_animation"
        elif name == "链接错误":
            safe_name = "error_animation"
        elif name == "高温警告":
            safe_name = "high_temp_warning_animation"
        elif name == "计算中":
            safe_name = "computing_animation"
        
        definition_entries.append(f'    {{"{name}", {safe_name}_points, sizeof({safe_name}_points) / sizeof({safe_name}_points[0])}}')
    
    # 生成动画定义列表
    c_code += "// 动画定义列表\n"
    c_code += "static const animation_definition_t animation_definitions[] = {\n"
    c_code += ",\n".join(definition_entries)
    c_code += "\n};\n\n"
    
    # 添加功能函数
    functions = '''// 获取内置动画数量
int get_builtin_animation_count(void) {
    return sizeof(animation_definitions) / sizeof(animation_definitions[0]);
}

// 获取指定动画的名称
const char* get_builtin_animation_name(int index) {
    int count = get_builtin_animation_count();
    if (index < 0 || index >= count) {
        return NULL;
    }
    return animation_definitions[index].name;
}

// 获取指定动画的点数量
int get_builtin_animation_point_count(int index) {
    int count = get_builtin_animation_count();
    if (index < 0 || index >= count) {
        return -1;
    }
    return animation_definitions[index].point_count;
}

// 导出所有动画为JSON文件
esp_err_t export_animation_to_json(const char *filename) {
    ESP_LOGI(TAG, "导出所有动画到JSON文件: %s", filename);
    
    // 检查SD卡是否挂载
    if (!bsp_storage_sdcard_is_mounted()) {
        ESP_LOGE(TAG, "SD卡未挂载，无法导出动画");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 创建JSON对象
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "无法创建JSON根对象");
        return ESP_ERR_NO_MEM;
    }
    
    // 创建动画数组
    cJSON *animations = cJSON_CreateArray();
    if (!animations) {
        ESP_LOGE(TAG, "无法创建动画数组");
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    
    // 遍历所有动画定义
    int total_animations = sizeof(animation_definitions) / sizeof(animation_definitions[0]);
    int total_points = 0;
    
    for (int anim_idx = 0; anim_idx < total_animations; anim_idx++) {
        const animation_definition_t *anim_def = &animation_definitions[anim_idx];
        
        // 创建动画对象
        cJSON *animation = cJSON_CreateObject();
        if (!animation) {
            ESP_LOGE(TAG, "无法创建动画对象 %d", anim_idx);
            continue;
        }
        
        // 设置动画名称
        cJSON_AddStringToObject(animation, "name", anim_def->name);
        
        // 创建点数组
        cJSON *points = cJSON_CreateArray();
        if (!points) {
            ESP_LOGE(TAG, "无法创建点数组");
            cJSON_Delete(animation);
            continue;
        }
        
        // 添加所有点
        for (int i = 0; i < anim_def->point_count; i++) {
            cJSON *point = cJSON_CreateObject();
            if (!point) {
                ESP_LOGE(TAG, "无法创建点对象 %d:%d", anim_idx, i);
                continue;
            }
            
            cJSON_AddStringToObject(point, "type", "point");
            cJSON_AddNumberToObject(point, "x", anim_def->points[i].x);
            cJSON_AddNumberToObject(point, "y", anim_def->points[i].y);
            cJSON_AddNumberToObject(point, "r", anim_def->points[i].r);
            cJSON_AddNumberToObject(point, "g", anim_def->points[i].g);
            cJSON_AddNumberToObject(point, "b", anim_def->points[i].b);
            
            cJSON_AddItemToArray(points, point);
        }
        
        // 添加点数组到动画
        cJSON_AddItemToObject(animation, "points", points);
        
        // 添加动画到动画数组
        cJSON_AddItemToArray(animations, animation);
        
        total_points += anim_def->point_count;
        ESP_LOGI(TAG, "添加动画 '%s'，包含 %d 个点", anim_def->name, anim_def->point_count);
    }
    
    // 添加动画数组到根对象
    cJSON_AddItemToObject(root, "animations", animations);
    
    // 生成JSON字符串
    char *json_string = cJSON_Print(root);
    if (!json_string) {
        ESP_LOGE(TAG, "无法生成JSON字符串");
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    
    // 写入文件
    FILE *file = fopen(filename, "w");
    if (!file) {
        ESP_LOGE(TAG, "无法创建文件: %s", filename);
        free(json_string);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t written = fwrite(json_string, 1, strlen(json_string), file);
    fclose(file);
    
    if (written != strlen(json_string)) {
        ESP_LOGE(TAG, "写入文件失败，期望 %d 字节，实际写入 %d 字节", 
                strlen(json_string), written);
        free(json_string);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_SIZE;
    }
    
    ESP_LOGI(TAG, "成功导出 %d 个动画（共 %d 个点）到文件: %s", 
             total_animations, total_points, filename);
    ESP_LOGI(TAG, "JSON文件大小: %d 字节", strlen(json_string));
    
    // 清理内存
    free(json_string);
    cJSON_Delete(root);
    
    return ESP_OK;
}
'''
    
    c_code += functions
    return c_code

def main():
    print("开始动画数据同步...")
    
    # 文件路径
    script_dir = os.path.dirname(os.path.abspath(__file__))
    base_dir = os.path.dirname(script_dir)
    json_file = os.path.join(base_dir, "main", "example_animation.json")
    c_file = os.path.join(base_dir, "components", "led_matrix", "src", "led_animation_export.c")
    
    print(f"脚本目录: {script_dir}")
    print(f"项目根目录: {base_dir}")
    print(f"JSON文件路径: {json_file}")
    print(f"C文件路径: {c_file}")
    
    # 检查文件是否存在
    if not os.path.exists(json_file):
        print(f"错误: JSON文件不存在: {json_file}")
        return 1
    
    if not os.path.exists(c_file):
        print(f"错误: C文件不存在: {c_file}")
        return 1
    
    # 加载JSON动画数据
    animations = load_json_animations(json_file)
    if not animations:
        print("错误: 无法加载动画数据")
        return 1
    
    print(f"成功加载 {len(animations)} 个动画:")
    for i, anim in enumerate(animations):
        point_count = len([p for p in anim['points'] if p.get('type') == 'point'])
        print(f"  {i+1}. {anim['name']} - {point_count} 个点")
    
    # 生成新的C代码
    new_c_code = generate_full_c_code(animations)
    
    # 备份原文件
    backup_file = c_file + ".backup"
    if os.path.exists(c_file):
        with open(c_file, 'r', encoding='utf-8') as f:
            original_content = f.read()
        with open(backup_file, 'w', encoding='utf-8') as f:
            f.write(original_content)
        print(f"已备份原文件到: {backup_file}")
    
    # 写入新的C代码
    try:
        with open(c_file, 'w', encoding='utf-8') as f:
            f.write(new_c_code)
        print(f"成功更新C文件: {c_file}")
        print("动画数据同步完成！")
        return 0
    except Exception as e:
        print(f"写入C文件失败: {e}")
        # 恢复备份
        if os.path.exists(backup_file):
            with open(backup_file, 'r', encoding='utf-8') as f:
                original_content = f.read()
            with open(c_file, 'w', encoding='utf-8') as f:
                f.write(original_content)
            print("已恢复原文件")
        return 1

if __name__ == "__main__":
    sys.exit(main())
