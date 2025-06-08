# RM01-BSP - 控制系统

基于ESP32-S3的高性能控制器，集成网络监控、电源管理和动画控制功能。

## 🎯 项目概述

RM01-BSP是一个先进的嵌入式控制系统，主要功能包括：
- **智能LED矩阵控制** - 支持32x32像素矩阵的实时动画显示
- **高级网络监控** - 并发监控多个网络目标，1秒快速响应
- **电源管理系统** - XSP16芯片集成，实时监控电压、电流、功率
- **状态驱动动画** - 根据网络状态和系统负载自动切换LED动画

## 🚀 核心特性

### 网络监控系统
- ✅ 并发ping监控（4个目标IP）
- ✅ 1秒快速监控模式
- ✅ 事件驱动状态通知
- ✅ 性能统计和网络质量监控
- ✅ 自适应监控间隔调整

### LED动画系统
- ✅ JSON配置的多动画系统
- ✅ 9种系统状态动画
- ✅ 硬编码动画回退机制
- ✅ 32x32像素矩阵支持

### 电源管理
- ✅ XSP16电源芯片UART通信
- ✅ 实时电压、电流、功率监控
- ✅ 基于功耗的负载检测（>50W触发）

## 📁 项目结构

```
rm01-bsp/
├── components/
│   ├── led_matrix/              # LED矩阵控制模块
│   ├── rm01_esp32s3_bsp/        # BSP硬件抽象层
│   └── ...
├── main/
│   ├── hello_world_main.c       # 主程序
│   ├── system_state_controller.c # 系统状态控制
│   ├── network_animation_controller.c # 网络动画联动
│   └── ...
├── README_POWER_CHIP.md         # 电源芯片文档
└── main/README_ANIMATION.md     # 动画系统文档
```

## 🔧 快速开始

### 环境准备
```bash
# 安装ESP-IDF v5.1+
. $HOME/esp/esp-idf/export.sh

# 克隆项目
git clone <repository-url>
cd rm01-bsp
```

### 编译和烧录
```bash
# 配置目标芯片
idf.py set-target esp32s3

# 编译项目
idf.py build

# 烧录和监控
idf.py flash monitor
```

## 📊 系统状态

项目支持9种智能状态：
1. **DEMO** - 演示模式
2. **STARTUP** - 启动状态
3. **ALL_CONNECTED** - 全部连接
4. **PARTIAL_CONNECTED** - 部分连接
5. **USER_DISCONNECTED** - 用户断开
6. **INTERNET_ONLY** - 仅互联网
7. **ISOLATED** - 完全隔离
8. **HIGH_TEMPERATURE** - 高温警告
9. **COMPUTING** - 计算负载状态

## 🌐 Web监控界面

访问设备IP地址查看实时网络监控界面：
- 实时网络状态显示
- 响应时间统计
- 丢包率监控
- 自动刷新

## 📖 详细文档

- [系统状态控制器文档](main/README_SYSTEM_STATE.md) - 完整实现状态报告
- [LED动画系统文档](main/README_ANIMATION.md) - 动画配置和使用
- [电源系统文档](README_POWER_CHIP.md) - XSP16电源芯片集成

## 🛠️ 开发说明

### 支持的目标
- ESP32-S3 (主要支持)
- ESP32 (基础支持)

### 关键依赖
- ESP-IDF v5.1+
- LED Strip驱动
- FreeRTOS
- lwIP网络栈

## 📝 版本信息

- **当前版本**: v1.0
- **最后更新**: 2025-01-27
- **支持芯片**: ESP32-S3

## 🆘 故障排除

### 常见问题
1. **LED矩阵不亮** - 检查GPIO9连接和电源
2. **网络监控失败** - 确认网络配置和IP地址
3. **烧录失败** - 检查串口连接和波特率设置

### 技术支持
如遇问题，请检查串口输出日志：
```bash
idf.py monitor
```

## 📄 许可证

本项目遵循ESP-IDF许可证。

We will get back to you as soon as possible.
