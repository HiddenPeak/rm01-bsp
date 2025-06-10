#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Jetson监控数据查询测试脚本
测试各种Prometheus查询是否能正确返回数据
"""

import requests
import json
import sys

def test_prometheus_query(base_url, query, description):
    """测试单个Prometheus查询"""
    print(f"\n=== 测试查询: {description} ===")
    print(f"查询语句: {query}")
    
    try:
        # 构造完整的查询URL
        url = f"{base_url}?query={requests.utils.quote(query)}"
        print(f"请求URL: {url}")
        
        # 发送请求
        response = requests.get(url, timeout=10)
        print(f"HTTP状态码: {response.status_code}")
        
        if response.status_code == 200:
            data = response.json()
            print(f"响应状态: {data.get('status', 'unknown')}")
            
            if data.get('status') == 'success':
                result = data.get('data', {}).get('result', [])
                print(f"结果数量: {len(result)}")
                
                if result:
                    for i, item in enumerate(result[:3]):  # 显示前3个结果
                        metric = item.get('metric', {})
                        value = item.get('value', [None, None])
                        print(f"  结果 {i+1}:")
                        print(f"    标签: {metric}")
                        print(f"    值: {value[1] if len(value) > 1 else 'N/A'}")
                        
                    if len(result) > 3:
                        print(f"    ... 还有 {len(result) - 3} 个结果")
                else:
                    print("  没有找到匹配的数据")
            else:
                print(f"查询失败: {data}")
        else:
            print(f"请求失败: {response.text}")
            
    except Exception as e:
        print(f"查询出错: {e}")

def main():
    """主函数"""
    print("Jetson监控数据查询测试")
    print("=" * 50)
    
    # Prometheus API基础URL
    prometheus_url = "http://10.10.99.99:59100/api/v1/query"
    
    # 要测试的查询
    queries = [
        # 温度查询
        ("temperature_C{statistic=\"cpu\"}", "Jetson CPU温度"),
        ("temperature_C{statistic=\"gpu\"}", "Jetson GPU温度"),
        ("temperature_C", "所有温度传感器"),
        
        # GPU使用率查询  
        ("gpu_utilization_percentage_Hz{nvidia_gpu=\"freq\",statistic=\"gpu\"}", "GPU当前频率"),
        ("gpu_utilization_percentage_Hz{nvidia_gpu=\"max_freq\",statistic=\"gpu\"}", "GPU最大频率"),
        ("gpu_utilization_percentage_Hz", "所有GPU频率数据"),
        
        # 内存查询
        ("ram_kB{statistic=\"total\"}", "内存总量"),
        ("ram_kB{statistic=\"used\"}", "内存使用量"),
        ("ram_kB", "所有内存数据"),
        
        # N305温度查询（从N305的Prometheus获取）
        ("node_hwmon_temp_celsius{chip=\"platform_coretemp_0\",sensor=\"temp1\"}", "N305 CPU温度"),
        ("node_hwmon_temp_celsius", "所有N305温度数据"),
    ]
    
    # 测试每个查询
    success_count = 0
    total_count = len(queries)
    
    for query, description in queries:
        test_prometheus_query(prometheus_url, query, description)
        # 简单检查是否成功（可以改进）
        success_count += 1
    
    # 总结
    print(f"\n" + "=" * 50)
    print(f"测试完成: {success_count}/{total_count} 个查询已测试")
    print("请查看上面的输出来验证数据格式是否正确")

if __name__ == "__main__":
    main()
