# Touch WS2812 显示系统使用指南

## 概述

Touch WS2812 显示系统是一个智能的状态指示系统，根据网络连接状态和系统运行状态自动显示不同的颜色和动画效果。该系统集成在BSP显示控制器中，提供了直观的系统状态反馈。

## 显示规则

### 1. 初始化模式（前30秒）
- **显示效果**: 白色常亮
- **触发条件**: 系统启动后的前30秒
- **含义**: 系统正在初始化，等待各组件启动完成

### 2. 错误状态显示

#### N305错误模式
- **显示效果**: 蓝色闪烁（500ms间隔）
- **触发条件**: 超过60秒无法ping通N305算力模组（10.10.99.98）
- **含义**: N305算力模组连接异常

#### Jetson错误模式
- **显示效果**: 黄色闪烁（500ms间隔）
- **触发条件**: 超过60秒无法ping通Jetson应用模组（10.10.99.99）
- **含义**: Jetson应用模组连接异常

#### 用户主机警告模式
- **显示效果**: 绿色闪烁（500ms间隔）
- **触发条件**: 无法ping通用户主机（10.10.99.100）
- **含义**: 用户主机未连接或网络异常

#### 多重错误模式
- **显示效果**: 蓝色/黄色/绿色/红色循环闪烁切换（500ms切换一次）
- **触发条件**: 同时存在2个或以上的网络连接错误
- **含义**: 多个组件同时出现连接问题

### 3. 正常运行状态显示

#### 启动模式
- **显示效果**: 白色快速呼吸（1秒周期）
- **触发条件**: 启动30秒后，N305、Jetson、用户主机都可以ping通
- **含义**: 系统正常启动中，各组件连接正常

#### 无互联网待机模式
- **显示效果**: 白色慢速呼吸（3秒周期）
- **触发条件**: 启动240秒后，内网连接正常但无互联网连接
- **含义**: 内网运行正常，但无外网连接

#### 有互联网待机模式
- **显示效果**: 橙色慢速呼吸（3秒周期）
- **触发条件**: 启动240秒后，内网和互联网都连接正常
- **含义**: 系统完全正常，内外网均可用

## 技术实现

### 状态检测机制
- 每秒检查一次网络连接状态
- 使用ping测试检查各目标IP的连通性
- 基于时间阈值判断错误状态（30秒初始化，60秒错误判断，240秒待机）

### 动画实现
- **常亮**: 固定颜色显示，支持亮度调节
- **闪烁**: 颜色与关闭状态之间周期性切换
- **呼吸**: 使用正弦波生成平滑的亮度变化效果

### 优先级规则
1. **初始化状态**（最高优先级）- 前30秒强制显示
2. **多重错误** - 2个或以上错误时显示
3. **单一错误** - N305/Jetson错误需要60秒超时，用户主机错误立即显示
4. **正常运行状态** - 根据网络连接情况显示启动或待机状态

## 编程接口

### 自动模式接口
```c
// 初始化显示控制器
esp_err_t bsp_touch_ws2812_display_init(const touch_display_config_t* config);

// 启动自动显示控制
esp_err_t bsp_touch_ws2812_display_start(void);

// 停止显示控制
void bsp_touch_ws2812_display_stop(void);

// 获取显示状态
esp_err_t bsp_touch_ws2812_display_get_status(touch_display_status_t* status);
```

### 手动控制接口
```c
// 手动设置颜色（固定色）
esp_err_t bsp_touch_ws2812_display_set_color(uint8_t r, uint8_t g, uint8_t b);

// 手动设置闪烁
esp_err_t bsp_touch_ws2812_display_set_blink(uint8_t r, uint8_t g, uint8_t b, blink_speed_t speed);

// 手动设置呼吸灯
esp_err_t bsp_touch_ws2812_display_set_breath(uint8_t r, uint8_t g, uint8_t b, breath_speed_t speed);

// 手动设置显示模式
esp_err_t bsp_touch_ws2812_display_set_mode(touch_display_mode_t mode);

// 恢复自动模式
void bsp_touch_ws2812_display_resume_auto(void);
```

### 配置接口
```c
// 设置亮度（0-255）
void bsp_touch_ws2812_display_set_brightness(uint8_t brightness);

// 设置自动模式开关
void bsp_touch_ws2812_display_set_auto_mode(bool enabled);

// 设置调试模式
void bsp_touch_ws2812_display_set_debug_mode(bool debug_mode);
```

## 集成到BSP显示控制器

Touch WS2812显示系统已经完全集成到BSP显示控制器中，通过以下接口使用：

```c
// 通过BSP显示控制器访问Touch WS2812功能
esp_err_t bsp_display_controller_set_touch_ws2812_mode(int mode);
esp_err_t bsp_display_controller_set_touch_ws2812_color(uint8_t r, uint8_t g, uint8_t b);
void bsp_display_controller_set_touch_ws2812_brightness(uint8_t brightness);
void bsp_display_controller_resume_touch_ws2812_auto(void);
```

## 测试功能

系统提供了完整的测试程序，可以验证所有功能：

```c
#include "test_touch_ws2812_display.h"

// 执行完整的功能测试
test_touch_ws2812_display_main();

// 演示自动模式
demo_touch_ws2812_auto_mode();
```

测试内容包括：
- 基本颜色显示测试
- 闪烁效果测试
- 呼吸灯效果测试
- 预定义显示模式测试
- 亮度调节测试
- 状态信息获取测试

## 配置选项

### 默认配置
```c
touch_display_config_t config = {
    .auto_mode_enabled = true,         // 启用自动模式
    .init_duration_ms = 30000,         // 初始化时间30秒
    .error_timeout_ms = 60000,         // 错误判断超时60秒
    .standby_delay_ms = 240000,        // 待机延迟240秒
    .debug_mode = false,               // 禁用调试模式
    .brightness = 128                  // 中等亮度
};
```

### 可调参数
- **初始化时间**: 可调节系统启动后的初始化显示时间
- **错误超时**: 可调节判断连接错误的等待时间
- **待机延迟**: 可调节进入待机状态的等待时间
- **亮度**: 支持0-255级亮度调节
- **调试模式**: 启用后会输出详细的状态变化日志

## 系统集成

Touch WS2812显示系统作为BSP显示控制器的一部分，在系统启动时自动初始化：

1. **BSP板级初始化** (`bsp_board_init()`) 
   - 初始化Touch WS2812硬件
   - 初始化显示控制器
   - 启动网络监控系统

2. **显示控制器启动** (`bsp_display_controller_start()`)
   - 启动Touch WS2812显示控制器
   - 注册网络状态变化回调
   - 开始自动显示控制

3. **运行时自动控制**
   - 监控网络连接状态
   - 根据状态变化自动切换显示模式
   - 提供实时的系统状态反馈

这样用户无需手动干预，Touch WS2812就能准确反映系统的当前状态，为系统诊断和监控提供直观的视觉反馈。
