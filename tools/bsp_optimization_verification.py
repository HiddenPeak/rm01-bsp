#!/usr/bin/env python3
"""
BSP 优化验证脚本
用于验证 BSP 优化的完整性和正确性
"""

import os
import sys
import re

def check_file_exists(file_path):
    """检查文件是否存在"""
    if os.path.exists(file_path):
        print(f"✅ 文件存在: {file_path}")
        return True
    else:
        print(f"❌ 文件缺失: {file_path}")
        return False

def check_function_in_file(file_path, function_name):
    """检查文件中是否包含特定函数"""
    if not os.path.exists(file_path):
        return False
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
            if function_name in content:
                print(f"✅ 函数存在: {function_name} in {os.path.basename(file_path)}")
                return True
            else:
                print(f"❌ 函数缺失: {function_name} in {os.path.basename(file_path)}")
                return False
    except Exception as e:
        print(f"❌ 读取文件失败: {file_path}, 错误: {e}")
        return False

def check_config_macro(file_path, macro_name):
    """检查配置文件中是否包含特定宏定义"""
    if not os.path.exists(file_path):
        return False
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
            pattern = f"#define\\s+{macro_name}"
            if re.search(pattern, content):
                print(f"✅ 配置宏存在: {macro_name}")
                return True
            else:
                print(f"❌ 配置宏缺失: {macro_name}")
                return False
    except Exception as e:
        print(f"❌ 读取配置文件失败: {file_path}, 错误: {e}")
        return False

def main():
    """主验证函数"""
    print("BSP 优化验证开始...")
    print("=" * 50)
    
    # 基础路径
    base_path = "c:/Users/sprin/rm01-bsp"
    bsp_src_path = f"{base_path}/components/rm01_esp32s3_bsp/src/bsp_board.c"
    bsp_header_path = f"{base_path}/components/rm01_esp32s3_bsp/include/bsp_board.h"
    bsp_config_path = f"{base_path}/components/rm01_esp32s3_bsp/include/bsp_config.h"
    test_file_path = f"{base_path}/tests/test_bsp_board.c"
    final_report_path = f"{base_path}/docs/BSP_BOARD_OPTIMIZATION_FINAL_REPORT.md"
    
    # 检查文件存在性
    print("\\n1. 检查文件存在性:")
    files_ok = True
    files_ok &= check_file_exists(bsp_src_path)
    files_ok &= check_file_exists(bsp_header_path)
    files_ok &= check_file_exists(bsp_config_path)
    files_ok &= check_file_exists(test_file_path)
    files_ok &= check_file_exists(final_report_path)
    
    # 检查关键函数是否存在
    print("\\n2. 检查性能统计函数:")
    functions_ok = True
    perf_functions = [
        "bsp_board_update_performance_stats",
        "bsp_board_print_performance_stats",
        "bsp_board_reset_performance_stats",
        "bsp_board_increment_network_errors",
        "bsp_board_increment_power_fluctuations",
        "bsp_board_increment_animation_frames"
    ]
    
    for func in perf_functions:
        functions_ok &= check_function_in_file(bsp_src_path, func)
    
    print("\\n3. 检查健康检查函数:")
    health_functions = [
        "bsp_board_health_check",
        "bsp_board_health_check_extended",
        "bsp_board_print_system_info"
    ]
    
    for func in health_functions:
        functions_ok &= check_function_in_file(bsp_src_path, func)
    
    print("\\n4. 检查资源管理函数:")
    resource_functions = [
        "bsp_board_cleanup",
        "bsp_board_is_initialized",
        "bsp_board_error_recovery"
    ]
    
    for func in resource_functions:
        functions_ok &= check_function_in_file(bsp_src_path, func)
    
    print("\\n5. 检查单元测试函数:")
    test_functions = [
        "bsp_board_run_unit_tests",
        "test_bsp_init_cleanup",
        "test_bsp_config_validation",
        "test_bsp_health_check",
        "test_bsp_performance_stats"
    ]
    
    for func in test_functions:
        functions_ok &= check_function_in_file(bsp_src_path, func)
    
    print("\\n6. 检查头文件声明:")
    header_functions = [
        "bsp_board_update_performance_stats",
        "bsp_board_print_performance_stats",
        "bsp_board_health_check_extended"
    ]
    
    for func in header_functions:
        functions_ok &= check_function_in_file(bsp_header_path, func)
    
    print("\\n7. 检查配置宏定义:")
    config_ok = True
    config_macros = [
        "CONFIG_BSP_ENABLE_UNIT_TESTS",
        "CONFIG_BSP_ENABLE_PERFORMANCE_MONITORING",
        "CONFIG_BSP_ENABLE_EXTENDED_HEALTH_CHECK",
        "CONFIG_BSP_HEALTH_CHECK_MIN_FREE_HEAP",
        "CONFIG_BSP_ANIMATION_TASK_STACK_SIZE",
        "CONFIG_BSP_ANIMATION_TASK_PRIORITY"
    ]
    
    for macro in config_macros:
        config_ok &= check_config_macro(bsp_config_path, macro)
    
    # 检查性能统计结构
    print("\\n8. 检查性能统计结构:")
    struct_ok = check_function_in_file(bsp_src_path, "bsp_performance_stats_t")
    
    # 检查测试文件内容
    print("\\n9. 检查测试文件:")
    test_ok = True
    test_cases = [
        "TEST_CASE.*BSP初始化和清理测试",
        "TEST_CASE.*BSP性能统计测试",
        "TEST_CASE.*BSP健康检查测试",
        "TEST_CASE.*BSP错误恢复测试",
        "TEST_CASE.*BSP压力测试",
        "TEST_CASE.*BSP内存泄漏测试"
    ]
    
    if os.path.exists(test_file_path):
        try:
            with open(test_file_path, 'r', encoding='utf-8') as f:
                content = f.read()
                for test_case in test_cases:
                    if re.search(test_case, content):
                        print(f"✅ 测试用例存在: {test_case}")
                    else:
                        print(f"❌ 测试用例缺失: {test_case}")
                        test_ok = False
        except Exception as e:
            print(f"❌ 读取测试文件失败: {e}")
            test_ok = False
    
    # 总结
    print("\\n" + "=" * 50)
    print("验证结果总结:")
    print(f"文件存在性: {'✅ 通过' if files_ok else '❌ 失败'}")
    print(f"函数完整性: {'✅ 通过' if functions_ok else '❌ 失败'}")
    print(f"配置完整性: {'✅ 通过' if config_ok else '❌ 失败'}")
    print(f"结构完整性: {'✅ 通过' if struct_ok else '❌ 失败'}")
    print(f"测试完整性: {'✅ 通过' if test_ok else '❌ 失败'}")
    
    overall_ok = files_ok and functions_ok and config_ok and struct_ok and test_ok
    print(f"\\n总体验证: {'✅ 全部通过' if overall_ok else '❌ 存在问题'}")
    
    if overall_ok:
        print("\\n🎉 BSP 优化验证成功！所有功能都已正确实现。")
        return 0
    else:
        print("\\n⚠️  BSP 优化验证发现问题，请检查上述错误。")
        return 1

if __name__ == "__main__":
    sys.exit(main())
