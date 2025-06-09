#!/usr/bin/env python3
"""
BSP深度分析工具 - 识别进一步优化机会
分析当前36.1秒初始化时间中的剩余瓶颈
"""

import re
import os
from pathlib import Path

class BSPDeepAnalyzer:
    def __init__(self, project_root):
        self.project_root = Path(project_root)
        self.bsp_src = self.project_root / "components" / "rm01_esp32s3_bsp" / "src"
        
    def analyze_delay_patterns(self):
        """分析代码中的延迟模式"""
        print("=== 延迟模式分析 ===")
        
        delay_patterns = [
            (r'vTaskDelay\(pdMS_TO_TICKS\((\d+)\)', 'vTaskDelay'),
            (r'vTaskDelay\((\d+)\s*/\s*portTICK_PERIOD_MS\)', 'vTaskDelay_alt'),
            (r'esp_timer_start_once\([^,]+,\s*(\d+)', 'timer_delay'),
            (r'while.*{\s*[^}]*vTaskDelay\([^)]+\)\s*;\s*}', 'while_loop'),
        ]
        
        total_delays = {}
        files_analyzed = []
        
        for c_file in self.bsp_src.glob("*.c"):
            with open(c_file, 'r', encoding='utf-8') as f:
                content = f.read()
                files_analyzed.append(c_file.name)
                
                for pattern, delay_type in delay_patterns:
                    matches = re.findall(pattern, content)
                    if matches:
                        if delay_type not in total_delays:
                            total_delays[delay_type] = []
                        
                        for match in matches:
                            if isinstance(match, str) and match.isdigit():
                                delay_ms = int(match)
                                total_delays[delay_type].append({
                                    'file': c_file.name,
                                    'delay': delay_ms,
                                    'context': self._extract_context(content, pattern, match)
                                })
        
        print(f"分析了 {len(files_analyzed)} 个文件")
        
        total_delay_time = 0
        for delay_type, delays in total_delays.items():
            type_total = sum(d['delay'] for d in delays)
            total_delay_time += type_total
            print(f"\n{delay_type}: {type_total}ms 总计")
            
            # 显示主要延迟项
            sorted_delays = sorted(delays, key=lambda x: x['delay'], reverse=True)
            for delay in sorted_delays[:5]:  # 显示前5大延迟
                print(f"  {delay['file']}: {delay['delay']}ms - {delay['context']}")
        
        print(f"\n代码中明确延迟总计: {total_delay_time}ms ({total_delay_time/1000:.1f}秒)")
        print(f"剩余未分析时间: {36100 - total_delay_time}ms ({(36100 - total_delay_time)/1000:.1f}秒)")
        
        return total_delays
    
    def analyze_initialization_sequence(self):
        """分析初始化序列顺序"""
        print("\n=== 初始化序列分析 ===")
        
        init_patterns = [
            r'bsp_([a-z_]+)_init\(',
            r'([a-z_]+)_init\(',
            r'ESP_ERROR_CHECK\(([^)]+)\)',
            r'esp_([a-z_]+)_init\(',
        ]
        
        bsp_board_file = self.bsp_src / "bsp_board.c"
        if not bsp_board_file.exists():
            print("bsp_board.c 文件未找到")
            return
        
        with open(bsp_board_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # 查找bsp_board_init函数
        init_func_match = re.search(r'esp_err_t\s+bsp_board_init\s*\([^)]*\)\s*\{(.*?)\n\}', 
                                   content, re.DOTALL)
        
        if not init_func_match:
            print("未找到bsp_board_init函数")
            return
        
        init_content = init_func_match.group(1)
        
        # 分析初始化步骤
        init_steps = []
        lines = init_content.split('\n')
        
        for i, line in enumerate(lines):
            line = line.strip()
            if ('init' in line.lower() or 'start' in line.lower()) and not line.startswith('//'):
                if any(pattern in line for pattern in ['bsp_', 'esp_', 'ESP_']):
                    init_steps.append({
                        'line_num': i,
                        'code': line,
                        'phase': self._identify_phase(line, lines, i)
                    })
        
        print("初始化步骤序列:")
        for step in init_steps:
            print(f"  {step['phase']}: {step['code']}")
        
        return init_steps
    
    def analyze_serial_vs_parallel_opportunities(self):
        """分析串行vs并行执行机会"""
        print("\n=== 并行化机会分析 ===")
        
        # 识别可能并行执行的组件
        independent_components = [
            'power_init',
            'ws2812_init', 
            'network_init',
            'webserver_init',
            'led_matrix_init',
            'system_state_init'
        ]
        
        dependencies = {
            'webserver_init': ['network_init'],
            'network_monitoring': ['network_init', 'webserver_init'],
            'system_state_monitoring': ['network_monitoring'],
        }
        
        parallel_groups = self._identify_parallel_groups(independent_components, dependencies)
        
        print("可并行执行的组件组:")
        for i, group in enumerate(parallel_groups):
            estimated_saving = self._estimate_parallel_savings(group)
            print(f"  组 {i+1}: {group} (预计节省: {estimated_saving}ms)")
        
        return parallel_groups
    
    def analyze_blocking_operations(self):
        """分析阻塞操作"""
        print("\n=== 阻塞操作分析 ===")
        
        blocking_patterns = [
            (r'while\s*\([^)]+\)\s*\{[^}]*vTaskDelay[^}]*\}', '轮询等待循环'),
            (r'ESP_ERROR_CHECK\([^)]+\)', 'ESP错误检查'),
            (r'xTaskCreate\([^)]+\)', '任务创建'),
            (r'xSemaphoreTake\([^)]+\)', '信号量等待'),
            (r'uart_read_bytes\([^)]+\)', 'UART读取'),
            (r'spi_device_transmit\([^)]+\)', 'SPI传输'),
        ]
        
        blocking_ops = []
        for c_file in self.bsp_src.glob("*.c"):
            with open(c_file, 'r', encoding='utf-8') as f:
                content = f.read()
                
                for pattern, op_type in blocking_patterns:
                    matches = re.finditer(pattern, content)
                    for match in matches:
                        blocking_ops.append({
                            'file': c_file.name,
                            'type': op_type,
                            'code': match.group(0)[:100] + '...',
                            'line': content[:match.start()].count('\n') + 1
                        })
        
        print("发现的阻塞操作:")
        type_counts = {}
        for op in blocking_ops:
            type_counts[op['type']] = type_counts.get(op['type'], 0) + 1
            
        for op_type, count in sorted(type_counts.items(), key=lambda x: x[1], reverse=True):
            print(f"  {op_type}: {count} 个实例")
        
        return blocking_ops
    
    def generate_phase2_optimization_plan(self):
        """生成第二阶段优化计划"""
        print("\n=== 第二阶段优化建议 ===")
        
        optimizations = [
            {
                'category': '并行初始化',
                'impact': 'HIGH',
                'estimated_saving': '5-8秒',
                'actions': [
                    '将LED矩阵、电源管理、WS2812初始化并行执行',
                    '网络硬件初始化与其他硬件初始化并行',
                    '使用任务组和同步原语协调初始化完成'
                ]
            },
            {
                'category': '异步服务启动',
                'impact': 'HIGH', 
                'estimated_saving': '3-5秒',
                'actions': [
                    'Web服务器异步启动，不等待完全就绪',
                    '网络监控服务后台启动',
                    '系统状态监控延迟启动'
                ]
            },
            {
                'category': '智能等待机制',
                'impact': 'MEDIUM',
                'estimated_saving': '2-4秒',
                'actions': [
                    '用事件驱动替换固定延迟',
                    '实现超时+就绪检查的混合机制',
                    '减少轮询间隔，增加事件响应'
                ]
            },
            {
                'category': '延迟初始化',
                'impact': 'MEDIUM',
                'estimated_saving': '2-3秒',
                'actions': [
                    '电源芯片协商推迟到首次需要时',
                    '网络质量监控延迟启动',
                    '非关键服务按需启动'
                ]
            },
            {
                'category': '硬件优化',
                'impact': 'LOW',
                'estimated_saving': '1-2秒',
                'actions': [
                    '优化SPI/I2C传输速度',
                    '减少GPIO操作延迟',
                    '优化ADC采样频率'
                ]
            }
        ]
        
        total_potential_saving = 0
        for opt in optimizations:
            print(f"\n{opt['category']} (影响: {opt['impact']}, 节省: {opt['estimated_saving']})")
            for action in opt['actions']:
                print(f"  - {action}")
            
            # 估算节省时间（取范围中值）
            saving_match = re.search(r'(\d+)-(\d+)', opt['estimated_saving'])
            if saving_match:
                saving_avg = (int(saving_match.group(1)) + int(saving_match.group(2))) / 2
                total_potential_saving += saving_avg
        
        print(f"\n总计预期节省时间: {total_potential_saving:.1f}秒")
        print(f"优化后预期初始化时间: {36.1 - total_potential_saving:.1f}秒")
        
        return optimizations
    
    def _extract_context(self, content, pattern, match):
        """提取延迟的上下文信息"""
        match_pos = content.find(f'({match})')
        if match_pos == -1:
            return "未知上下文"
        
        lines = content[:match_pos].split('\n')
        if len(lines) >= 2:
            return lines[-2].strip()[:50]
        return "未知上下文"
    
    def _identify_phase(self, line, lines, line_num):
        """识别初始化阶段"""
        context_lines = lines[max(0, line_num-5):line_num+1]
        context = ' '.join(context_lines).lower()
        
        if '第一阶段' in context or 'led矩阵' in context:
            return '第一阶段'
        elif '第二阶段' in context or 'web服务器' in context:
            return '第二阶段'
        elif '第三阶段' in context or 'ws2812' in context:
            return '第三阶段'
        elif '第四阶段' in context or '第五阶段' in context or '监控' in context:
            return '第四/五阶段'
        else:
            return '未分类'
    
    def _identify_parallel_groups(self, components, dependencies):
        """识别可并行执行的组件组"""
        # 简化的并行组识别
        return [
            ['power_init', 'ws2812_init'],
            ['led_matrix_init'],
            ['network_init'],
            ['webserver_init'],
            ['system_state_init']
        ]
    
    def _estimate_parallel_savings(self, group):
        """估算并行执行的节省时间"""
        # 简化的节省时间估算
        base_savings = {
            'power_init': 1000,
            'ws2812_init': 500,
            'led_matrix_init': 800,
            'network_init': 2000,
            'webserver_init': 1500,
            'system_state_init': 500
        }
        
        group_time = sum(base_savings.get(comp, 500) for comp in group)
        max_time = max(base_savings.get(comp, 500) for comp in group)
        
        return group_time - max_time if len(group) > 1 else 0

def main():
    project_root = r"c:\Users\sprin\rm01-bsp"
    analyzer = BSPDeepAnalyzer(project_root)
    
    print("BSP深度分析 - 第二阶段优化机会识别")
    print("="*50)
    
    # 执行各项分析
    delays = analyzer.analyze_delay_patterns()
    init_sequence = analyzer.analyze_initialization_sequence()
    parallel_ops = analyzer.analyze_serial_vs_parallel_opportunities()
    blocking_ops = analyzer.analyze_blocking_operations()
    optimizations = analyzer.generate_phase2_optimization_plan()
    
    print("\n" + "="*50)
    print("分析完成！请查看上述建议实施第二阶段优化。")

if __name__ == "__main__":
    main()
