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
- ✅ Web界面实时监控

### LED动画系统
- ✅ JSON配置的多动画系统
- ✅ 9种系统状态动画
- ✅ 硬编码动画回退机制
- ✅ 32x32像素矩阵支持
- ✅ 示例动画和模板文件

### 电源管理
- ✅ XSP16电源芯片UART通信
- ✅ 实时电压、电流、功率监控
- ✅ 基于功耗的负载检测（>50W触发）
- ✅ 电源系统状态显示

### 架构设计
- ✅ 清晰的BSP硬件抽象层
- ✅ 组件化模块设计
- ✅ 应用与硬件层分离
- ✅ 代码复用和可维护性

## 📁 项目结构

```
rm01-bsp/
├── components/
│   ├── led_matrix/              # LED矩阵控制模块
│   │   ├── examples/            # LED动画示例和模板
│   │   │   ├── example_animation.json
│   │   │   ├── matrix_template.json
│   │   │   └── README_ANIMATION.md
│   │   ├── include/
│   │   └── src/
│   └── rm01_esp32s3_bsp/        # BSP硬件抽象层
│       ├── include/             # BSP公共头文件
│       │   ├── bsp_system_state.h
│       │   ├── bsp_network_animation.h
│       │   ├── bsp_power_test.h
│       │   └── ...
│       ├── src/                 # BSP核心实现
│       │   ├── bsp_system_state.c      # 系统状态控制
│       │   ├── bsp_network_animation.c # 网络动画联动
│       │   ├── bsp_power_test.c        # 电源芯片测试
│       │   └── ...
│       └── web/                 # Web界面资源
├── docs/                        # 项目文档
│   ├── CODE_ARCHITECTURE_REFACTORING_SUMMARY.md
│   ├── DOCUMENTATION_CLEANUP_SUMMARY.md
│   ├── PROJECT_ARCHITECTURE_REFACTORING_COMPLETE.md
│   ├── PrometheusSetting.md     # Prometheus配置文档
│   ├── README_POWER_CHIP.md     # 电源芯片文档
│   └── README_SYSTEM_STATE.md   # 系统状态文档
├── main/                        # 应用程序入口
│   ├── main.c                   # 主程序（仅应用逻辑）
│   ├── CMakeLists.txt
│   └── idf_component.yml
├── tests/                       # 测试文件
│   ├── pytest_hello_world.py   # ESP-IDF测试
│   ├── test_crc.c              # CRC算法测试
│   └── test_crc_debug.c        # CRC调试程序
├── tools/                       # 开发工具
│   ├── quick_verify.py         # 快速验证工具
│   ├── sync_animation_data.py  # 动画数据同步
│   └── verify_animation_data.py # 动画数据验证
└── build/                       # 构建输出目录
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

### 开发工具
项目提供了一系列开发工具，位于 `tools/` 目录：
```bash
# 快速验证项目配置
python tools/quick_verify.py

# 同步动画数据
python tools/sync_animation_data.py

# 验证动画数据
python tools/verify_animation_data.py
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

## 🏗️ 项目架构

### 分层设计
项目采用现代化的分层架构，经过完整重构：

#### 应用层 (main/)
- 简洁的应用程序入口
- 仅包含业务逻辑
- BSP功能调用接口

#### BSP硬件抽象层 (components/rm01_esp32s3_bsp/)
- 完整的硬件抽象
- 系统状态管理: `bsp_system_state_*`
- 网络动画联动: `bsp_network_animation_*`
- 电源测试功能: `bsp_power_test_*`

#### 组件层 (components/)
- LED矩阵控制组件
- 示例和模板文件
- 可复用的通用组件

### 架构优势
- ✅ **代码复用性**: BSP层可在其他项目中复用
- ✅ **可维护性**: 清晰的职责分离和模块化设计
- ✅ **扩展性**: 组件化架构便于功能扩展
- ✅ **测试性**: 独立的测试目录和工具支持

## 📖 详细文档

项目文档已重新组织到 `docs/` 目录：

### 核心文档
- [系统状态控制器文档](docs/README_SYSTEM_STATE.md) - 完整实现状态报告
- [电源系统文档](docs/README_POWER_CHIP.md) - XSP16电源芯片集成
- [LED动画系统文档](components/led_matrix/examples/README_ANIMATION.md) - 动画配置和使用

### 项目架构文档
- [项目架构重构完成报告](docs/PROJECT_ARCHITECTURE_REFACTORING_COMPLETE.md) - 重构详细说明
- [代码架构重构总结](docs/CODE_ARCHITECTURE_REFACTORING_SUMMARY.md) - 架构变更总结
- [文档清理总结](docs/DOCUMENTATION_CLEANUP_SUMMARY.md) - 文档组织说明

### 配置文档
- [Prometheus配置文档](docs/PrometheusSetting.md) - 监控配置说明

## 🛠️ 开发说明

### 项目架构
项目采用分层架构设计：
- **应用层 (main/)**: 仅包含应用程序逻辑和主程序入口
- **BSP层 (components/rm01_esp32s3_bsp/)**: 硬件抽象层，包含所有硬件相关功能
- **组件层 (components/)**: 通用组件实现，如LED矩阵控制

### 支持的目标
- ESP32-S3 (主要支持)
- ESP32 (基础支持)

### 关键依赖
- ESP-IDF v5.1+
- LED Strip驱动
- FreeRTOS
- lwIP网络栈

### 测试支持
项目包含完整的测试框架，位于 `tests/` 目录：
```bash
# 运行Python测试
pytest tests/pytest_hello_world.py

# 编译和运行C测试
gcc tests/test_crc.c -o test_crc
./test_crc
```

## 📝 版本信息

- **当前版本**: v1.1
- **最后更新**: 2025-06-10
- **支持芯片**: ESP32-S3 (主要), ESP32 (基础)
- **架构版本**: BSP分层架构 v2.0

## 🆘 故障排除

### 常见问题
1. **LED矩阵不亮** - 检查GPIO9连接和电源
2. **网络监控失败** - 确认网络配置和IP地址
3. **烧录失败** - 检查串口连接和波特率设置
4. **BSP初始化失败** - 检查硬件连接和电源供应
5. **动画配置错误** - 检查 `components/led_matrix/examples/` 目录下的示例文件

### 开发工具诊断
```powershell
# 使用项目自带的验证工具
python tools/quick_verify.py

# 验证动画数据完整性
python tools/verify_animation_data.py

# 同步动画配置
python tools/sync_animation_data.py
```

### 技术支持
如遇问题，请检查串口输出日志：
```bash
idf.py monitor
```

### 调试模式
项目包含完整的测试支持：
```powershell
# 运行CRC测试
gcc tests/test_crc.c -o tests/test_crc.exe
./tests/test_crc.exe

# 运行Python测试套件
cd tests
pytest pytest_hello_world.py
```

## 🔄 重构说明

### v1.1 架构重构 (2025-06-10)
项目已完成重大架构重构，主要变更：

#### 代码重新组织
- 将应用层代码从 `main/` 迁移到BSP层 `components/rm01_esp32s3_bsp/`
- 函数重命名统一采用 `bsp_*` 前缀
- 文档集中管理到 `docs/` 目录

#### 新增功能
- 完整的开发工具集 (`tools/`)
- 测试框架支持 (`tests/`)
- LED动画示例和模板

详细重构信息请参阅 [项目架构重构完成报告](docs/PROJECT_ARCHITECTURE_REFACTORING_COMPLETE.md)

## 📄 许可证

本项目遵循MIT许可证

---

**注意**: 本项目已完成v1.1架构重构，建议查看 `docs/` 目录下的重构文档了解详细变更。
