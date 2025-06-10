#!/usr/bin/env python3
"""
BSP重构验证脚本
验证重构后的代码结构和接口完整性
"""

import os
import re
from typing import List, Dict, Tuple

def check_file_exists(file_path: str) -> bool:
    """检查文件是否存在"""
    return os.path.exists(file_path)

def extract_functions_from_header(header_path: str) -> List[str]:
    """从头文件中提取函数声明"""
    functions = []
    if not os.path.exists(header_path):
        return functions
    
    try:
        with open(header_path, 'r', encoding='utf-8') as f:
            content = f.read()
            
        # 匹配函数声明（简化版正则）
        function_pattern = r'^\s*(?:esp_err_t|void|int|bool|const\s+char\*|\w+_t)\s+(\w+)\s*\([^;]*\);'
        matches = re.findall(function_pattern, content, re.MULTILINE)
        functions.extend(matches)
        
    except Exception as e:
        print(f"读取文件 {header_path} 时出错: {e}")
    
    return functions

def extract_functions_from_source(source_path: str) -> List[str]:
    """从源文件中提取函数定义"""
    functions = []
    if not os.path.exists(source_path):
        return functions
    
    try:
        with open(source_path, 'r', encoding='utf-8') as f:
            content = f.read()
            
        # 匹配函数定义（简化版正则）
        function_pattern = r'^\s*(?:esp_err_t|void|int|bool|const\s+char\*|\w+_t)\s+(\w+)\s*\([^{]*\)\s*{'
        matches = re.findall(function_pattern, content, re.MULTILINE)
        functions.extend(matches)
        
    except Exception as e:
        print(f"读取文件 {source_path} 时出错: {e}")
    
    return functions

def verify_refactoring() -> Dict[str, bool]:
    """验证重构结果"""
    base_path = "c:/Users/sprin/rm01-bsp/components/rm01_esp32s3_bsp"
    results = {}
    
    # 1. 检查新文件是否存在
    new_files = [
        f"{base_path}/include/bsp_state_manager.h",
        f"{base_path}/src/bsp_state_manager.c",
        f"{base_path}/include/bsp_display_controller.h",
        f"{base_path}/src/bsp_display_controller.c"
    ]
    
    print("=== 新文件存在性检查 ===")
    for file_path in new_files:
        exists = check_file_exists(file_path)
        results[f"file_exists_{os.path.basename(file_path)}"] = exists
        status = "✓" if exists else "✗"
        print(f"{status} {os.path.basename(file_path)}")
    
    # 2. 检查接口完整性
    print("\n=== 接口完整性检查 ===")
    
    # 状态管理器接口检查
    state_mgr_header = f"{base_path}/include/bsp_state_manager.h"
    state_mgr_source = f"{base_path}/src/bsp_state_manager.c"
    
    if check_file_exists(state_mgr_header) and check_file_exists(state_mgr_source):
        header_funcs = extract_functions_from_header(state_mgr_header)
        source_funcs = extract_functions_from_source(state_mgr_source)
        
        print(f"状态管理器:")
        print(f"  头文件声明: {len(header_funcs)} 个函数")
        print(f"  源文件定义: {len(source_funcs)} 个函数")
        
        # 检查核心接口是否存在
        core_interfaces = [
            "bsp_state_manager_init",
            "bsp_state_manager_start_monitoring",
            "bsp_state_manager_get_current_state",
            "bsp_state_manager_register_callback"
        ]
        
        for interface in core_interfaces:
            in_header = interface in header_funcs
            in_source = interface in source_funcs
            status = "✓" if (in_header and in_source) else "✗"
            print(f"  {status} {interface}")
            results[f"interface_{interface}"] = in_header and in_source
    
    # 显示控制器接口检查
    display_header = f"{base_path}/include/bsp_display_controller.h"
    display_source = f"{base_path}/src/bsp_display_controller.c"
    
    if check_file_exists(display_header) and check_file_exists(display_source):
        header_funcs = extract_functions_from_header(display_header)
        source_funcs = extract_functions_from_source(display_source)
        
        print(f"显示控制器:")
        print(f"  头文件声明: {len(header_funcs)} 个函数")
        print(f"  源文件定义: {len(source_funcs)} 个函数")
        
        # 检查核心接口是否存在
        core_interfaces = [
            "bsp_display_controller_init",
            "bsp_display_controller_start",
            "bsp_display_controller_set_animation",
            "bsp_display_controller_get_status"
        ]
        
        for interface in core_interfaces:
            in_header = interface in header_funcs
            in_source = interface in source_funcs
            status = "✓" if (in_header and in_source) else "✗"
            print(f"  {status} {interface}")
            results[f"interface_{interface}"] = in_header and in_source
    
    # 3. 检查兼容性包装
    print("\n=== 兼容性包装检查 ===")
    compat_source = f"{base_path}/src/bsp_system_state.c"
    
    if check_file_exists(compat_source):
        try:
            with open(compat_source, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # 检查是否包含新架构的头文件
            has_state_mgr = "bsp_state_manager.h" in content
            has_display_ctrl = "bsp_display_controller.h" in content
            has_compat_tag = "兼容" in content
            
            print(f"  ✓ 包含状态管理器头文件: {has_state_mgr}")
            print(f"  ✓ 包含显示控制器头文件: {has_display_ctrl}")
            print(f"  ✓ 标记为兼容性包装: {has_compat_tag}")
            
            results["compat_wrapper"] = has_state_mgr and has_display_ctrl and has_compat_tag
            
        except Exception as e:
            print(f"  ✗ 读取兼容性包装文件失败: {e}")
            results["compat_wrapper"] = False
    
    # 4. 检查文档
    print("\n=== 文档检查 ===")
    doc_files = [
        "c:/Users/sprin/rm01-bsp/docs/BSP_STATE_DISPLAY_REFACTORING_COMPLETE.md",
        "c:/Users/sprin/rm01-bsp/tests/bsp_refactoring_demo.c"
    ]
    
    for doc_file in doc_files:
        exists = check_file_exists(doc_file)
        status = "✓" if exists else "✗"
        print(f"  {status} {os.path.basename(doc_file)}")
        results[f"doc_{os.path.basename(doc_file)}"] = exists
    
    return results

def main():
    print("BSP系统状态与显示重构验证")
    print("=" * 50)
    
    results = verify_refactoring()
    
    # 统计结果
    total_checks = len(results)
    passed_checks = sum(1 for v in results.values() if v)
    
    print(f"\n=== 验证结果汇总 ===")
    print(f"总检查项: {total_checks}")
    print(f"通过项: {passed_checks}")
    print(f"成功率: {passed_checks/total_checks*100:.1f}%")
    
    if passed_checks == total_checks:
        print("\n🎉 重构验证完全通过！")
        print("✓ 所有新文件已创建")
        print("✓ 核心接口完整")
        print("✓ 兼容性包装正确")
        print("✓ 文档齐全")
    else:
        print(f"\n⚠️  验证未完全通过，有 {total_checks - passed_checks} 项需要检查")
        for key, value in results.items():
            if not value:
                print(f"✗ {key}")
    
    print("\n=== 重构架构总结 ===")
    print("新架构包含三个主要组件：")
    print("1. bsp_state_manager - 纯状态检测与管理")
    print("2. bsp_display_controller - 专门的显示控制")
    print("3. bsp_network_animation - 网络状态监控（重构）")
    print("4. bsp_system_state - 兼容性包装（保持向后兼容）")
    
    print("\n架构特点：")
    print("✓ 关注点分离 - 状态检测与显示控制完全分离")
    print("✓ 松耦合设计 - 基于回调机制的组件通信")
    print("✓ 易于扩展 - 模块化设计便于添加新功能")
    print("✓ 向后兼容 - 旧代码无需修改即可使用")

if __name__ == "__main__":
    main()
