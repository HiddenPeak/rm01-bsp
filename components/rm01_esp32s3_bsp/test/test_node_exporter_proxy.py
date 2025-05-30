#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Node Exporter代理测试脚本
此脚本用于测试ESP32S3 Node Exporter代理功能的性能和正确性
"""

import requests
import time
import json
import argparse
from rich.console import Console
from rich.table import Table

# 创建控制台对象用于彩色输出
console = Console()

def test_node_exporter_direct(host="10.10.99.99", port=9100):
    """直接从Node Exporter获取指标"""
    url = f"http://{host}:{port}/metrics"
    console.print(f"[bold blue]正在测试Node Exporter直接访问: {url}[/bold blue]")
    
    start_time = time.time()
    try:
        response = requests.get(url, timeout=5)
        elapsed = time.time() - start_time
        
        if response.status_code == 200:
            data_size = len(response.text)
            console.print(f"[green]成功![/green] 状态码: {response.status_code}, 数据大小: {data_size} 字节, 耗时: {elapsed:.3f} 秒")
            return {
                "success": True,
                "status_code": response.status_code,
                "data_size": data_size,
                "elapsed": elapsed,
                "raw_data": response.text
            }
        else:
            console.print(f"[red]失败![/red] 状态码: {response.status_code}, 耗时: {elapsed:.3f} 秒")
            return {
                "success": False,
                "status_code": response.status_code,
                "elapsed": elapsed
            }
    except Exception as e:
        elapsed = time.time() - start_time
        console.print(f"[red]错误![/red] {str(e)}, 耗时: {elapsed:.3f} 秒")
        return {
            "success": False,
            "error": str(e),
            "elapsed": elapsed
        }

def test_node_exporter_proxy(esp32_host="192.168.4.1", include_raw=True):
    """通过ESP32S3代理访问Node Exporter指标"""
    url = f"http://{esp32_host}/api/node-exporter"
    if not include_raw:
        url += "?raw=false"
        
    console.print(f"[bold blue]正在测试ESP32S3 Node Exporter代理: {url}[/bold blue]")
    
    start_time = time.time()
    try:
        response = requests.get(url, timeout=10)
        elapsed = time.time() - start_time
        
        if response.status_code == 200:
            try:
                json_data = response.json()
                data_size = len(response.text)
                raw_data_size = len(json_data.get("raw_data", "")) if "raw_data" in json_data else 0
                metrics_size = len(json.dumps(json_data.get("metrics", {})))
                
                console.print(f"[green]成功![/green] 状态码: {response.status_code}")
                console.print(f"总数据大小: {data_size} 字节")
                if "raw_data" in json_data:
                    console.print(f"原始数据大小: {raw_data_size} 字节")
                console.print(f"预处理指标大小: {metrics_size} 字节")
                console.print(f"总耗时: {elapsed:.3f} 秒")
                
                return {
                    "success": True,
                    "status_code": response.status_code,
                    "data_size": data_size,
                    "raw_data_size": raw_data_size,
                    "metrics_size": metrics_size,
                    "elapsed": elapsed,
                    "json_data": json_data
                }
            except json.JSONDecodeError:
                console.print(f"[yellow]警告![/yellow] 响应不是有效的JSON格式")
                return {
                    "success": True,
                    "status_code": response.status_code,
                    "data_size": len(response.text),
                    "elapsed": elapsed,
                    "error": "无效的JSON响应"
                }
        else:
            console.print(f"[red]失败![/red] 状态码: {response.status_code}, 耗时: {elapsed:.3f} 秒")
            return {
                "success": False,
                "status_code": response.status_code,
                "elapsed": elapsed
            }
    except Exception as e:
        elapsed = time.time() - start_time
        console.print(f"[red]错误![/red] {str(e)}, 耗时: {elapsed:.3f} 秒")
        return {
            "success": False,
            "error": str(e),
            "elapsed": elapsed
        }

def compare_results(direct_result, proxy_result, proxy_result_no_raw=None):
    """比较直接访问和代理访问的结果"""
    if not direct_result["success"] or not proxy_result["success"]:
        console.print("[red]无法比较结果，有测试失败[/red]")
        return
    
    table = Table(title="测试结果比较")
    
    table.add_column("指标", style="cyan")
    table.add_column("直接访问", style="green")
    table.add_column("ESP32S3代理 (含原始数据)", style="yellow")
    if proxy_result_no_raw:
        table.add_column("ESP32S3代理 (仅预处理数据)", style="blue")
    
    # 添加数据
    table.add_row(
        "响应时间 (秒)",
        f"{direct_result['elapsed']:.3f}",
        f"{proxy_result['elapsed']:.3f}",
        f"{proxy_result_no_raw['elapsed']:.3f}" if proxy_result_no_raw else "-"
    )
    
    table.add_row(
        "数据大小 (字节)",
        f"{direct_result['data_size']}",
        f"{proxy_result['data_size']}",
        f"{proxy_result_no_raw['data_size']}" if proxy_result_no_raw else "-"
    )
    
    if proxy_result_no_raw:
        reduction = (1 - proxy_result_no_raw['data_size'] / direct_result['data_size']) * 100
        table.add_row(
            "数据减少比例",
            "-",
            f"{(1 - proxy_result['data_size'] / direct_result['data_size']) * 100:.2f}%",
            f"{reduction:.2f}%"
        )
    else:
        table.add_row(
            "数据减少比例",
            "-",
            f"{(1 - proxy_result['data_size'] / direct_result['data_size']) * 100:.2f}%",
            "-"
        )
    
    console.print(table)

def validate_metrics(result):
    """验证指标数据的准确性"""
    if not result["success"] or "json_data" not in result:
        console.print("[red]无法验证指标，数据获取失败[/red]")
        return
    
    console.print("[bold blue]验证指标数据...[/bold blue]")
    
    metrics = result["json_data"].get("metrics", {})
    
    # 验证基本指标是否存在
    required_metrics = [
        "hostname", "sysname", "release",
        "memory_total", "memory_free", "memory_usage_percent",
        "cpu_user_percent", "cpu_idle_percent",
        "disk_total", "disk_free"
    ]
    
    missing_metrics = [metric for metric in required_metrics if metric not in metrics]
    
    if missing_metrics:
        console.print(f"[yellow]缺少以下指标: {', '.join(missing_metrics)}[/yellow]")
    else:
        console.print("[green]所有必需指标都存在![/green]")
    
    # 验证指标值是否合理
    if "memory_usage_percent" in metrics and not (0 <= metrics["memory_usage_percent"] <= 100):
        console.print(f"[red]内存使用率异常: {metrics['memory_usage_percent']}%[/red]")
    
    if "cpu_idle_percent" in metrics and not (0 <= metrics["cpu_idle_percent"] <= 100):
        console.print(f"[red]CPU空闲率异常: {metrics['cpu_idle_percent']}%[/red]")
    
    if "disk_usage_percent" in metrics and not (0 <= metrics["disk_usage_percent"] <= 100):
        console.print(f"[red]磁盘使用率异常: {metrics['disk_usage_percent']}%[/red]")
    
    # 打印验证通过的关键指标值
    if "hostname" in metrics:
        console.print(f"主机名: {metrics['hostname']}")
    
    if "sysname" in metrics and "release" in metrics:
        console.print(f"操作系统: {metrics['sysname']} {metrics['release']}")
    
    if "memory_total" in metrics and "memory_free" in metrics:
        total_mb = metrics["memory_total"] / (1024 * 1024)
        free_mb = metrics["memory_free"] / (1024 * 1024)
        console.print(f"内存: 总计 {total_mb:.1f} MB, 空闲 {free_mb:.1f} MB")
    
    if "cpu_user_percent" in metrics and "cpu_system_percent" in metrics:
        console.print(f"CPU: 用户空间 {metrics['cpu_user_percent']:.1f}%, 系统空间 {metrics['cpu_system_percent']:.1f}%")

def main():
    parser = argparse.ArgumentParser(description="测试ESP32S3 Node Exporter代理功能")
    parser.add_argument("--node-exporter", default="10.10.99.99", help="Node Exporter主机地址")
    parser.add_argument("--node-port", type=int, default=9100, help="Node Exporter端口")
    parser.add_argument("--esp32", default="192.168.4.1", help="ESP32S3主机地址")
    args = parser.parse_args()
    
    console.print("[bold]===== Node Exporter代理功能测试 =====[/bold]")
    console.print(f"Node Exporter: {args.node_exporter}:{args.node_port}")
    console.print(f"ESP32S3: {args.esp32}")
    console.print("")
    
    # 测试直接访问Node Exporter
    direct_result = test_node_exporter_direct(args.node_exporter, args.node_port)
    console.print("")
    
    # 测试通过ESP32S3代理访问（包含原始数据）
    proxy_result = test_node_exporter_proxy(args.esp32, include_raw=True)
    console.print("")
    
    # 测试通过ESP32S3代理访问（不包含原始数据，仅预处理指标）
    proxy_result_no_raw = test_node_exporter_proxy(args.esp32, include_raw=False)
    console.print("")
    
    # 比较结果
    compare_results(direct_result, proxy_result, proxy_result_no_raw)
    console.print("")
    
    # 验证指标
    validate_metrics(proxy_result_no_raw or proxy_result)

if __name__ == "__main__":
    main()
