import re

# 读取C文件内容
with open('components/led_matrix/src/led_animation_export.c', 'r', encoding='utf-8') as f:
    content = f.read()

# 查找动画定义列表
pattern = r'animation_definitions\[\]\s*=\s*\{([^}]+)\}'
match = re.search(pattern, content, re.MULTILINE | re.DOTALL)

if match:
    print('找到动画定义列表')
    definitions = match.group(1)
    # 统计动画数量
    count = len(re.findall(r'\{"[^"]+\"', definitions))
    print(f'动画数量: {count}')
    
    # 显示动画名称
    names = re.findall(r'\{"([^"]+)"', definitions)
    for i, name in enumerate(names, 1):
        print(f'{i}. {name}')
else:
    print('未找到动画定义列表')

# 验证JSON中的动画数量
import json
with open('main/example_animation.json', 'r', encoding='utf-8') as f:
    json_data = json.load(f)

json_animations = json_data.get('animations', [])
print(f'\nJSON中的动画数量: {len(json_animations)}')
for i, anim in enumerate(json_animations, 1):
    point_count = len([p for p in anim['points'] if p.get('type') == 'point'])
    print(f'{i}. {anim["name"]} - {point_count} 个点')

print('\n✅ 同步验证完成！')
