#!/usr/bin/env python3
"""
BSP第二阶段优化准备状态检查工具
验证所有必要文件和配置是否就绪
"""

import os
from pathlib import Path
import json

class BSPPhase2ReadinessChecker:
    def __init__(self, project_root):
        self.project_root = Path(project_root)
        self.bsp_component = self.project_root / "components" / "rm01_esp32s3_bsp"
        self.tools_dir = self.project_root / "tools"
        self.docs_dir = self.project_root / "docs"
        
        self.check_results = []
        
    def run_comprehensive_check(self):
        """运行全面检查"""
        print("=== BSP第二阶段优化准备状态检查 ===")
        print(f"项目路径: {self.project_root}")
        print()
        
        # 执行各项检查
        self._check_file_structure()
        self._check_optimization_tools()
        self._check_documentation()
        self._check_configuration_files()
        self._check_existing_optimizations()
        
        # 生成总结报告
        self._generate_readiness_report()
        
    def _check_file_structure(self):
        """检查文件结构"""
        print("1. 检查项目文件结构...")
        
        required_files = [
            ("BSP组件源码目录", self.bsp_component / "src"),
            ("BSP组件头文件目录", self.bsp_component / "include"),
            ("主板文件", self.bsp_component / "src" / "bsp_board.c"),
            ("主配置文件", self.bsp_component / "include" / "bsp_config.h"),
            ("网络监控文件", self.bsp_component / "src" / "network_monitor.c"),
            ("Web服务器文件", self.bsp_component / "src" / "bsp_webserver.c"),
        ]
        
        all_good = True
        for name, path in required_files:
            if path.exists():
                print(f"   ✓ {name}: 存在")
                self.check_results.append({"item": name, "status": "PASS", "path": str(path)})
            else:
                print(f"   ✗ {name}: 缺失 - {path}")
                self.check_results.append({"item": name, "status": "FAIL", "path": str(path)})
                all_good = False
        
        if all_good:
            print("   文件结构检查: 通过")
        else:
            print("   文件结构检查: 有文件缺失")
        print()
        
    def _check_optimization_tools(self):
        """检查优化工具"""
        print("2. 检查优化工具...")
        
        optimization_tools = [
            ("第二阶段优化器", "bsp_phase2_optimizer.py"),
            ("优化管理工具", "bsp_optimization_manager.py"),
            ("深度分析工具", "bsp_deep_analysis.py"),
            ("验证工具", "bsp_init_optimization_check.py"),
        ]
        
        all_good = True
        for name, filename in optimization_tools:
            tool_path = self.tools_dir / filename
            if tool_path.exists():
                print(f"   ✓ {name}: 可用")
                self.check_results.append({"item": name, "status": "PASS", "path": str(tool_path)})
            else:
                print(f"   ✗ {name}: 缺失 - {filename}")
                self.check_results.append({"item": name, "status": "FAIL", "path": str(tool_path)})
                all_good = False
        
        # 检查并行初始化框架
        parallel_header = self.bsp_component / "include" / "bsp_parallel_init.h"
        parallel_source = self.bsp_component / "src" / "bsp_parallel_init.c"
        
        if parallel_header.exists() and parallel_source.exists():
            print("   ✓ 并行初始化框架: 已准备")
            self.check_results.append({"item": "并行初始化框架", "status": "PASS"})
        else:
            print("   ⚠ 并行初始化框架: 部分缺失 (高级功能)")
            self.check_results.append({"item": "并行初始化框架", "status": "WARNING"})
        
        if all_good:
            print("   优化工具检查: 通过")
        else:
            print("   优化工具检查: 有工具缺失")
        print()
        
    def _check_documentation(self):
        """检查文档"""
        print("3. 检查优化文档...")
        
        docs = [
            ("第一阶段优化计划", "BSP_INIT_OPTIMIZATION_PLAN.md"),
            ("第二阶段优化计划", "BSP_PHASE2_OPTIMIZATION_PLAN.md"),
            ("实施指南", "BSP_PHASE2_IMPLEMENTATION_GUIDE.md"),
        ]
        
        all_good = True
        for name, filename in docs:
            doc_path = self.docs_dir / filename
            if doc_path.exists():
                print(f"   ✓ {name}: 存在")
                self.check_results.append({"item": name, "status": "PASS", "path": str(doc_path)})
            else:
                print(f"   ✗ {name}: 缺失 - {filename}")
                self.check_results.append({"item": name, "status": "FAIL", "path": str(doc_path)})
                all_good = False
        
        if all_good:
            print("   文档检查: 通过")
        else:
            print("   文档检查: 有文档缺失")
        print()
        
    def _check_configuration_files(self):
        """检查配置文件"""
        print("4. 检查配置文件...")
        
        # 检查Kconfig文件
        kconfig_path = self.bsp_component / "Kconfig"
        if kconfig_path.exists():
            print("   ✓ Kconfig文件: 存在")
            
            # 检查关键配置项
            with open(kconfig_path, 'r', encoding='utf-8') as f:
                kconfig_content = f.read()
            
            key_configs = [
                "BSP_ENABLE_PHASE2_OPTIMIZATION",
                "BSP_STARTUP_MODE",
                "BSP_ASYNC_WEBSERVER_TIMEOUT_MS",
                "BSP_PARALLEL_HARDWARE_TIMEOUT_MS"
            ]
            
            for config in key_configs:
                if config in kconfig_content:
                    print(f"      ✓ {config}: 已定义")
                else:
                    print(f"      ✗ {config}: 未定义")
                    
            self.check_results.append({"item": "Kconfig配置", "status": "PASS"})
        else:
            print("   ✗ Kconfig文件: 缺失")
            self.check_results.append({"item": "Kconfig配置", "status": "FAIL"})
        
        # 检查配置头文件
        config_h_path = self.bsp_component / "include" / "bsp_config.h"
        if config_h_path.exists():
            print("   ✓ bsp_config.h: 存在")
            
            with open(config_h_path, 'r', encoding='utf-8') as f:
                config_content = f.read()
            
            if "BSP_ENABLE_PHASE2_OPTIMIZATION" in config_content:
                print("      ✓ 第二阶段优化配置: 已集成")
            else:
                print("      ⚠ 第二阶段优化配置: 需要更新")
                
            self.check_results.append({"item": "配置头文件", "status": "PASS"})
        else:
            print("   ✗ bsp_config.h: 缺失")
            self.check_results.append({"item": "配置头文件", "status": "FAIL"})
        
        print()
        
    def _check_existing_optimizations(self):
        """检查已有优化"""
        print("5. 检查第一阶段优化状态...")
        
        bsp_board_path = self.bsp_component / "src" / "bsp_board.c"
        if bsp_board_path.exists():
            with open(bsp_board_path, 'r', encoding='utf-8') as f:
                board_content = f.read()
            
            # 检查第一阶段优化标记
            phase1_indicators = [
                ("网络延迟优化", "BSP_INIT_DELAY_NETWORK_MS.*500"),
                ("LED延迟优化", "BSP_INIT_DELAY_LED_MATRIX_MS.*200"),
                ("系统监控延迟优化", "BSP_INIT_DELAY_SYSTEM_MONITOR_MS.*2000"),
                ("WS2812测试优化", "快速验证"),
            ]
            
            import re
            for name, pattern in phase1_indicators:
                if re.search(pattern, board_content):
                    print(f"   ✓ {name}: 已应用")
                else:
                    print(f"   ⚠ {name}: 可能未应用")
            
            # 检查性能统计
            if "bsp_performance_stats_t" in board_content:
                print("   ✓ 性能监控: 已集成")
            else:
                print("   ⚠ 性能监控: 需要加强")
                
            self.check_results.append({"item": "第一阶段优化", "status": "PASS"})
        else:
            print("   ✗ 无法检查bsp_board.c文件")
            self.check_results.append({"item": "第一阶段优化", "status": "FAIL"})
        
        print()
        
    def _generate_readiness_report(self):
        """生成准备状态报告"""
        print("=== 准备状态总结 ===")
        
        pass_count = sum(1 for result in self.check_results if result["status"] == "PASS")
        warning_count = sum(1 for result in self.check_results if result["status"] == "WARNING") 
        fail_count = sum(1 for result in self.check_results if result["status"] == "FAIL")
        total_count = len(self.check_results)
        
        print(f"检查项目总数: {total_count}")
        print(f"通过: {pass_count} | 警告: {warning_count} | 失败: {fail_count}")
        print()
        
        # 计算准备度
        readiness_score = (pass_count + warning_count * 0.5) / total_count * 100
        
        if readiness_score >= 90:
            readiness_level = "优秀"
            readiness_color = "✓"
        elif readiness_score >= 75:
            readiness_level = "良好"
            readiness_color = "⚠"
        elif readiness_score >= 60:
            readiness_level = "基本"
            readiness_color = "⚠"
        else:
            readiness_level = "不足"
            readiness_color = "✗"
        
        print(f"{readiness_color} 第二阶段优化准备度: {readiness_score:.1f}% ({readiness_level})")
        print()
        
        # 提供建议
        if readiness_score >= 75:
            print("建议操作:")
            print("1. 运行优化管理工具选择策略:")
            print("   python tools/bsp_optimization_manager.py")
            print("2. 或直接应用第二阶段优化:")
            print("   python tools/bsp_phase2_optimizer.py")
        else:
            print("建议先完成以下准备工作:")
            
            for result in self.check_results:
                if result["status"] == "FAIL":
                    print(f"   - 修复: {result['item']}")
            
            print("\n完成后再运行第二阶段优化")
        
        print()
          # 输出详细结果到文件
        from datetime import datetime
        report_file = self.project_root / "docs" / "BSP_PHASE2_READINESS_CHECK.json"
        with open(report_file, 'w', encoding='utf-8') as f:
            json.dump({
                "timestamp": datetime.now().isoformat(),
                "readiness_score": readiness_score,
                "readiness_level": readiness_level,
                "summary": {
                    "total": total_count,
                    "pass": pass_count,
                    "warning": warning_count,
                    "fail": fail_count
                },
                "detailed_results": self.check_results
            }, f, indent=2, ensure_ascii=False)
        
        print(f"详细检查结果已保存到: {report_file}")
        
    def get_quick_status(self):
        """获取快速状态"""
        self.run_comprehensive_check()
        
        pass_count = sum(1 for result in self.check_results if result["status"] == "PASS")
        total_count = len(self.check_results)
        
        return pass_count / total_count >= 0.75

def main():
    project_root = r"c:\Users\sprin\rm01-bsp"
    checker = BSPPhase2ReadinessChecker(project_root)
    
    print("BSP第二阶段优化准备状态检查工具")
    print("="*50)
    
    checker.run_comprehensive_check()
    
    print("="*50)
    print("检查完成！")

if __name__ == "__main__":
    main()
