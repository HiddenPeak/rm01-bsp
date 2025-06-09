#!/usr/bin/env python3
"""
BSP优化策略选择和应用工具
帮助用户根据需求选择最适合的优化策略
"""

import os
import sys
import subprocess
from pathlib import Path

class BSPOptimizationManager:
    def __init__(self, project_root):
        self.project_root = Path(project_root)
        self.tools_dir = self.project_root / "tools"
        
    def show_optimization_options(self):
        """显示优化选项"""
        print("=== BSP初始化优化策略选择 ===")
        print()
        print("当前状态: 第一阶段优化已完成 (44.6秒 → 36.1秒)")
        print("可选的第二阶段优化策略:")
        print()
        
        strategies = [
            {
                "id": 1,
                "name": "渐进式优化",
                "description": "在现有代码基础上逐步优化，风险低",
                "target_time": "24-28秒",
                "improvement": "25-33%",
                "complexity": "低",
                "risk": "低",
                "features": [
                    "异步Web服务器启动",
                    "基础并行硬件初始化", 
                    "简单智能等待机制",
                    "部分延迟服务启动"
                ]
            },
            {
                "id": 2,
                "name": "完整并行优化",
                "description": "全面重构初始化流程，获得最大性能提升",
                "target_time": "15-20秒",
                "improvement": "45-58%",
                "complexity": "中",
                "risk": "中",
                "features": [
                    "完整并行初始化框架",
                    "多阶段异步启动",
                    "事件驱动同步机制",
                    "全面性能监控"
                ]
            },
            {
                "id": 3,
                "name": "极速启动模式",
                "description": "只启动最关键服务，实现超快启动",
                "target_time": "8-12秒",
                "improvement": "65-75%",
                "complexity": "高",
                "risk": "中",
                "features": [
                    "分层服务启动",
                    "按需功能加载",
                    "最小化初始化",
                    "延迟非关键服务"
                ]
            }
        ]
        
        for strategy in strategies:
            print(f"{strategy['id']}. {strategy['name']}")
            print(f"   描述: {strategy['description']}")
            print(f"   目标时间: {strategy['target_time']}")
            print(f"   性能改进: {strategy['improvement']}")
            print(f"   复杂度: {strategy['complexity']} | 风险: {strategy['risk']}")
            print(f"   特性:")
            for feature in strategy['features']:
                print(f"     - {feature}")
            print()
        
        return strategies
    
    def get_user_choice(self, strategies):
        """获取用户选择"""
        while True:
            try:
                choice = input(f"请选择优化策略 (1-{len(strategies)}) 或 'q' 退出: ").strip()
                
                if choice.lower() == 'q':
                    return None
                
                choice_id = int(choice)
                if 1 <= choice_id <= len(strategies):
                    return choice_id
                else:
                    print(f"请输入 1-{len(strategies)} 之间的数字")
                    
            except ValueError:
                print("请输入有效的数字")
    
    def apply_optimization_strategy(self, strategy_id):
        """应用选择的优化策略"""
        print(f"\n=== 应用优化策略 {strategy_id} ===")
        
        if strategy_id == 1:
            self._apply_progressive_optimization()
        elif strategy_id == 2:
            self._apply_full_parallel_optimization()
        elif strategy_id == 3:
            self._apply_fast_startup_optimization()
    
    def _apply_progressive_optimization(self):
        """应用渐进式优化"""
        print("正在应用渐进式优化...")
        
        # 运行第二阶段优化器
        optimizer_script = self.tools_dir / "bsp_phase2_optimizer.py"
        if optimizer_script.exists():
            print("运行BSP第二阶段优化器...")
            result = subprocess.run([sys.executable, str(optimizer_script)], 
                                  cwd=str(self.project_root))
            if result.returncode == 0:
                print("✓ 渐进式优化应用成功")
            else:
                print("✗ 优化应用过程中出现错误")
        
        # 修改配置以启用渐进式优化
        self._update_config_for_progressive()
        
    def _apply_full_parallel_optimization(self):
        """应用完整并行优化"""
        print("正在应用完整并行优化...")
        
        # 启用并行初始化框架
        self._update_config_for_parallel()
        
        # 集成并行初始化代码
        self._integrate_parallel_framework()
        
        print("✓ 完整并行优化配置完成")
        
    def _apply_fast_startup_optimization(self):
        """应用极速启动优化"""
        print("正在应用极速启动优化...")
        
        # 配置极速启动模式
        self._update_config_for_fast_startup()
        
        # 修改主初始化函数
        self._modify_main_for_fast_startup()
        
        print("✓ 极速启动优化配置完成")
        
    def _update_config_for_progressive(self):
        """更新配置以支持渐进式优化"""
        config_updates = {
            "CONFIG_BSP_ENABLE_PHASE2_OPTIMIZATION": "y",
            "CONFIG_BSP_STARTUP_MODE_VALUE": "1",
            "CONFIG_BSP_ASYNC_WEBSERVER_TIMEOUT_MS": "5000",
            "CONFIG_BSP_PARALLEL_HARDWARE_TIMEOUT_MS": "5000",
            "CONFIG_BSP_SMART_WAIT_CHECK_INTERVAL_MS": "100",
            "CONFIG_BSP_DELAYED_SERVICES_DELAY_MS": "3000",
            "CONFIG_BSP_ENABLE_PARALLEL_INIT_FRAMEWORK": "n"
        }
        self._apply_config_updates(config_updates)
        
    def _update_config_for_parallel(self):
        """更新配置以支持完整并行优化"""
        config_updates = {
            "CONFIG_BSP_ENABLE_PHASE2_OPTIMIZATION": "y",
            "CONFIG_BSP_STARTUP_MODE_VALUE": "2", 
            "CONFIG_BSP_ENABLE_PARALLEL_INIT_FRAMEWORK": "y",
            "CONFIG_BSP_PARALLEL_INIT_PHASE1_TIMEOUT_MS": "3000",
            "CONFIG_BSP_PARALLEL_INIT_PHASE2_TIMEOUT_MS": "4000",
            "CONFIG_BSP_PARALLEL_INIT_PHASE3_TIMEOUT_MS": "5000",
            "CONFIG_BSP_ENABLE_PERFORMANCE_MONITORING": "y",
            "CONFIG_BSP_PERFORMANCE_REPORT_DETAILED": "y"
        }
        self._apply_config_updates(config_updates)
        
    def _update_config_for_fast_startup(self):
        """更新配置以支持极速启动"""
        config_updates = {
            "CONFIG_BSP_ENABLE_PHASE2_OPTIMIZATION": "y",
            "CONFIG_BSP_STARTUP_MODE_VALUE": "3",
            "CONFIG_BSP_ENABLE_WS2812_TESTING": "n",
            "CONFIG_BSP_ENABLE_POWER_CHIP_MONITORING": "n",
            "CONFIG_BSP_INIT_DELAY_NETWORK_MS": "100",
            "CONFIG_BSP_INIT_DELAY_LED_MATRIX_MS": "50",
            "CONFIG_BSP_INIT_DELAY_SYSTEM_MONITOR_MS": "500",
            "CONFIG_BSP_DELAYED_SERVICES_DELAY_MS": "5000"
        }
        self._apply_config_updates(config_updates)
        
    def _apply_config_updates(self, config_updates):
        """应用配置更新"""
        print("更新项目配置...")
        
        sdkconfig_path = self.project_root / "sdkconfig"
        
        # 读取现有配置
        config_lines = []
        if sdkconfig_path.exists():
            with open(sdkconfig_path, 'r', encoding='utf-8') as f:
                config_lines = f.readlines()
        
        # 应用更新
        for key, value in config_updates.items():
            updated = False
            for i, line in enumerate(config_lines):
                if line.startswith(f"{key}="):
                    config_lines[i] = f"{key}={value}\n"
                    updated = True
                    break
            
            if not updated:
                config_lines.append(f"{key}={value}\n")
        
        # 写回配置文件
        with open(sdkconfig_path, 'w', encoding='utf-8') as f:
            f.writelines(config_lines)
        
        print(f"✓ 已更新 {len(config_updates)} 个配置项")
        
    def _integrate_parallel_framework(self):
        """集成并行初始化框架"""
        print("集成并行初始化框架到构建系统...")
        
        # 检查并行初始化文件是否存在
        parallel_header = self.project_root / "components" / "rm01_esp32s3_bsp" / "include" / "bsp_parallel_init.h"
        parallel_source = self.project_root / "components" / "rm01_esp32s3_bsp" / "src" / "bsp_parallel_init.c"
        
        if parallel_header.exists() and parallel_source.exists():
            print("✓ 并行初始化框架文件已存在")
        else:
            print("⚠ 并行初始化框架文件缺失，请运行工具创建")
            
    def _modify_main_for_fast_startup(self):
        """修改主函数以支持极速启动"""
        print("配置主函数以支持极速启动...")
        
        main_file = self.project_root / "main" / "main.c"
        if main_file.exists():
            with open(main_file, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # 检查是否已经修改过
            if "bsp_board_init_fast_mode" not in content:
                # 添加快速启动选项的注释
                fast_startup_comment = '''
    // BSP极速启动模式选择:
    // bsp_board_init();                    // 标准模式 (36.1秒)
    // bsp_board_init_phase2_optimized();  // 优化模式 (15-20秒) 
    // bsp_board_init_fast_mode();         // 极速模式 (8-12秒)
'''
                
                # 在bsp_board_init调用前添加注释
                if "bsp_board_init();" in content:
                    content = content.replace("bsp_board_init();", 
                                            fast_startup_comment + "    bsp_board_init();")
                    
                    with open(main_file, 'w', encoding='utf-8') as f:
                        f.write(content)
                    
                    print("✓ 已在main.c中添加启动模式选择注释")
            else:
                print("✓ main.c已包含启动模式选择")
        
    def generate_build_instructions(self, strategy_id):
        """生成构建说明"""
        print(f"\n=== 构建和测试说明 ===")
        
        strategy_names = {
            1: "渐进式优化",
            2: "完整并行优化", 
            3: "极速启动优化"
        }
        
        print(f"已配置: {strategy_names.get(strategy_id, '未知策略')}")
        print()
        print("下一步操作:")
        print("1. 重新配置项目:")
        print("   idf.py menuconfig")
        print("   (可选: 在 'RM01 ESP32-S3 BSP Configuration' 中调整设置)")
        print()
        print("2. 清理并重新构建:")
        print("   idf.py clean")
        print("   idf.py build")
        print()
        print("3. 烧录和监控:")
        print("   idf.py flash monitor")
        print()
        print("4. 观察优化效果:")
        
        expected_times = {
            1: "24-28秒",
            2: "15-20秒", 
            3: "8-12秒"
        }
        
        print(f"   预期初始化时间: {expected_times.get(strategy_id, '未知')}")
        print("   查看详细性能报告")
        print("   验证所有功能正常工作")
        print()
        
        if strategy_id == 3:
            print("极速启动模式特别说明:")
            print("- 某些非关键服务将延迟启动")
            print("- 部分功能可能在启动后几秒才可用")
            print("- 如需完整功能，请使用完整并行优化模式")
            print()
        
        print("如遇问题:")
        print("- 检查串口输出中的错误信息")
        print("- 查看性能监控报告")
        print("- 必要时回退到上一个工作版本")

def main():
    project_root = r"c:\Users\sprin\rm01-bsp"
    manager = BSPOptimizationManager(project_root)
    
    print("BSP优化策略管理工具")
    print("="*50)
    print("帮助您选择和应用最适合的BSP初始化优化策略")
    print()
    
    # 显示选项
    strategies = manager.show_optimization_options()
    
    # 获取用户选择
    choice = manager.get_user_choice(strategies)
    
    if choice is None:
        print("退出优化工具")
        return
    
    # 应用选择的策略
    manager.apply_optimization_strategy(choice)
    
    # 生成构建说明
    manager.generate_build_instructions(choice)
    
    print("\n" + "="*50)
    print("优化策略配置完成！请按照上述说明进行构建和测试。")

if __name__ == "__main__":
    main()
