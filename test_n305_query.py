#!/usr/bin/env python3
"""
测试N305 Prometheus查询API
验证JSON响应格式和数据解析
"""

import requests
import json
import urllib.parse

def test_n305_temperature_query():
    """测试N305温度查询API"""
    
    base_url = "http://10.10.99.99:59100/api/v1/query"
    
    # 测试不同的温度查询
    queries = [
        'node_hwmon_temp_celsius{chip="platform_coretemp_0",sensor="temp1"}',  # CPU封装温度
        'node_hwmon_temp_celsius{chip="platform_coretemp_0",sensor="temp2"}',  # CPU核心温度
        'node_hwmon_temp_celsius',  # 所有温度传感器
        'up{job="node_exporter_n305"}',  # 测试连接性
    ]
    
    print("=== N305 Prometheus查询API测试 ===")
    print(f"基础URL: {base_url}")
    print()
    
    for i, query in enumerate(queries, 1):
        print(f"查询 {i}/{len(queries)}: {query}")
        
        try:
            # 构建完整URL
            full_url = f"{base_url}?query={urllib.parse.quote(query)}"
            print(f"完整URL: {full_url}")
            
            # 发送请求
            response = requests.get(full_url, timeout=5)
            print(f"HTTP状态码: {response.status_code}")
            
            if response.status_code == 200:
                # 解析JSON响应
                data = response.json()
                print(f"响应状态: {data.get('status', '未知')}")
                
                if data.get('status') == 'success':
                    result_data = data.get('data', {})
                    results = result_data.get('result', [])
                    
                    print(f"结果数量: {len(results)}")
                    
                    if results:
                        for j, result in enumerate(results):
                            metric = result.get('metric', {})
                            value_array = result.get('value', [])
                            
                            if len(value_array) >= 2:
                                timestamp = value_array[0]
                                temp_value = value_array[1]
                                
                                chip = metric.get('chip', '未知')
                                sensor = metric.get('sensor', '未知')
                                
                                print(f"  结果 {j+1}: chip={chip}, sensor={sensor}, 温度={temp_value}°C")
                            else:
                                print(f"  结果 {j+1}: 数据格式异常")
                    else:
                        print("  没有找到结果数据")
                else:
                    print(f"查询失败: {data}")
            else:
                print(f"HTTP请求失败: {response.text}")
                
        except requests.exceptions.RequestException as e:
            print(f"网络错误: {e}")
        except json.JSONDecodeError as e:
            print(f"JSON解析错误: {e}")
        except Exception as e:
            print(f"其他错误: {e}")
        
        print("-" * 50)
        print()

def test_best_temperature_query():
    """测试找到最佳的温度查询"""
    
    base_url = "http://10.10.99.99:59100/api/v1/query"
    query = 'node_hwmon_temp_celsius{chip="platform_coretemp_0",sensor="temp1"}'
    
    print("=== 测试最佳温度查询 ===")
    print(f"查询: {query}")
    
    try:
        full_url = f"{base_url}?query={urllib.parse.quote(query)}"
        response = requests.get(full_url, timeout=5)
        
        if response.status_code == 200:
            data = response.json()
            
            if data.get('status') == 'success':
                results = data.get('data', {}).get('result', [])
                
                if results:
                    result = results[0]
                    value_array = result.get('value', [])
                    
                    if len(value_array) >= 2:
                        temp_value = float(value_array[1])
                        print(f"✅ 成功获取N305 CPU温度: {temp_value}°C")
                        print(f"这个查询适合ESP32S3使用")
                        return True
                    
        print("❌ 无法获取有效的温度数据")
        return False
        
    except Exception as e:
        print(f"❌ 查询失败: {e}")
        return False

if __name__ == "__main__":
    test_n305_temperature_query()
    test_best_temperature_query()
