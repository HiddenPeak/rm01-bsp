#!/usr/bin/env python3
"""
动画数据验证工具
验证 led_animation_export.c 中的硬编码数据是否与 example_animation.json 完全一致
"""

import json
import re
import os

def load_json_animations(json_file):
    """从JSON文件加载动画数据"""
    try:
        with open(json_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
        return data.get('animations', [])
    except Exception as e:
        print(f"加载JSON文件失败: {e}")
        return []

def extract_c_animation_data(c_file):
    """从C文件中提取动画数据"""
    try:
        with open(c_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        animations = {}
        
        # 查找动画定义列表
        definition_pattern = r'animation_definitions\[\]\s*=\s*\{([^}]+)\}'
        definition_match = re.search(definition_pattern, content, re.MULTILINE | re.DOTALL)
        
        if not definition_match:
            print("错误: 无法找到动画定义列表")
            return {}
        
        # 解析每个动画定义
        definition_content = definition_match.group(1)
        animation_entries = re.findall(r'\{"([^"]+)",\s*(\w+)_points,', definition_content)
        
        for name, var_prefix in animation_entries:
            # 查找对应的数组定义
            array_pattern = f'{var_prefix}_points\\[\\]\\s*=\\s*\\{{([^}}]+)\\}};'
            array_match = re.search(array_pattern, content, re.MULTILINE | re.DOTALL)
            
            if array_match:
                array_content = array_match.group(1)
                # 提取所有点数据
                point_pattern = r'\{(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\}'
                points = []
                for match in re.finditer(point_pattern, array_content):
                    x, y, r, g, b = map(int, match.groups())
                    points.append({'x': x, 'y': y, 'r': r, 'g': g, 'b': b})
                
                animations[name] = points
        
        return animations
        
    except Exception as e:
        print(f"解析C文件失败: {e}")
        return {}

def compare_animations(json_animations, c_animations):
    """比较JSON和C中的动画数据"""
    print("开始验证动画数据一致性...")
    print("=" * 50)
    
    # 检查动画数量
    json_names = [anim['name'] for anim in json_animations]
    c_names = list(c_animations.keys())
    
    print(f"JSON动画数量: {len(json_names)}")
    print(f"C代码动画数量: {len(c_names)}")
    
    if len(json_names) != len(c_names):
        print("❌ 动画数量不匹配!")
        return False
    
    # 检查每个动画
    all_match = True
    for anim in json_animations:
        name = anim['name']
        json_points = [p for p in anim['points'] if p.get('type') == 'point']
        
        if name not in c_animations:
            print(f"❌ 动画 '{name}' 在C代码中不存在")
            all_match = False
            continue
            
        c_points = c_animations[name]
        
        print(f"\n检查动画: {name}")
        print(f"  JSON点数: {len(json_points)}")
        print(f"  C代码点数: {len(c_points)}")
        
        if len(json_points) != len(c_points):
            print(f"  ❌ 点数量不匹配!")
            all_match = False
            continue
        
        # 比较每个点
        points_match = True
        for i, (json_point, c_point) in enumerate(zip(json_points, c_points)):
            json_data = {
                'x': json_point['x'],
                'y': json_point['y'],
                'r': json_point['r'],
                'g': json_point['g'],
                'b': json_point['b']
            }
            
            if json_data != c_point:
                print(f"  ❌ 第{i+1}个点不匹配:")
                print(f"    JSON: {json_data}")
                print(f"    C代码: {c_point}")
                points_match = False
                all_match = False
                break
        
        if points_match:
            print(f"  ✅ 所有点数据匹配")
    
    print("\n" + "=" * 50)
    if all_match:
        print("🎉 验证成功！所有动画数据完全一致")
        return True
    else:
        print("❌ 验证失败！存在数据不一致")
        return False

def main():
    # 文件路径
    script_dir = os.path.dirname(os.path.abspath(__file__))
    base_dir = os.path.dirname(script_dir)
    json_file = os.path.join(base_dir, "main", "example_animation.json")
    c_file = os.path.join(base_dir, "components", "led_matrix", "src", "led_animation_export.c")
    
    print("动画数据一致性验证工具")
    print(f"JSON文件: {json_file}")
    print(f"C文件: {c_file}")
    print()
    
    # 检查文件是否存在
    if not os.path.exists(json_file):
        print(f"错误: JSON文件不存在: {json_file}")
        return 1
    
    if not os.path.exists(c_file):
        print(f"错误: C文件不存在: {c_file}")
        return 1
    
    # 加载数据
    json_animations = load_json_animations(json_file)
    if not json_animations:
        print("错误: 无法加载JSON动画数据")
        return 1
    
    c_animations = extract_c_animation_data(c_file)
    if not c_animations:
        print("错误: 无法提取C代码动画数据")
        return 1
    
    # 比较数据
    if compare_animations(json_animations, c_animations):
        return 0
    else:
        return 1

if __name__ == "__main__":
    import sys
    sys.exit(main())
