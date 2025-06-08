# 网络监控实时性优化方案

## 概述

本次优化主要针对网络监控系统的实时性进行了全面改进，通过减少延迟、提高检测频率、增强回调机制等方式，显著提升了网络状态变化的检测和响应速度。

## 主要优化内容

### 1. 优化Ping配置参数

**优化前：**
- ping包数量：3个
- 包间隔：1000ms
- 超时时间：2000ms
- 任务优先级：2

**优化后：**
- ping包数量：1个（减少66%检测时间）
- 包间隔：100ms（减少90%间隔）
- 超时时间：1000ms（减少50%等待时间）
- 任务优先级：3（提高优先级）

### 2. 优化任务循环时间

**优化前：**
- 单次ping等待：5000ms
- 目标间延迟：2000ms
- 轮次间延迟：10000ms
- 总监控周期：约23秒（4个目标）

**优化后：**
- 单次ping等待：1500ms（减少70%）
- 目标间延迟：200-500ms（减少75-90%）
- 轮次间延迟：动态调整（500-3000ms）
- 总监控周期：约3-8秒（提升65-85%）

### 3. 新增动态监控模式

#### 快速监控模式
- 监控间隔：1000ms
- 目标间延迟：200ms
- 状态报告频率：每5轮一次

#### 普通监控模式
- 监控间隔：3000ms
- 目标间延迟：500ms
- 状态报告频率：每3轮一次

### 4. 增强事件通知机制

#### 新增功能
- 即时状态变化回调
- 事件组通知机制
- 支持等待特定网络事件
- 实时事件位设置

#### 事件类型
```c
#define NM_EVENT_COMPUTING_UP     (1 << 0)
#define NM_EVENT_COMPUTING_DOWN   (1 << 1)
#define NM_EVENT_APPLICATION_UP   (1 << 2)
#define NM_EVENT_APPLICATION_DOWN (1 << 3)
#define NM_EVENT_USER_HOST_UP     (1 << 4)
#define NM_EVENT_USER_HOST_DOWN   (1 << 5)
#define NM_EVENT_INTERNET_UP      (1 << 6)
#define NM_EVENT_INTERNET_DOWN    (1 << 7)
```

## 新增API接口

### 高实时性监控接口

```c
// 启用/禁用快速监控模式
void nm_enable_fast_monitoring(bool enable);

// 设置监控间隔（500ms-60000ms）
void nm_set_monitoring_interval(uint32_t interval_ms);

// 检查是否启用快速模式
bool nm_is_fast_mode_enabled(void);
```

### 事件通知接口

```c
// 获取网络事件组句柄
EventGroupHandle_t nm_get_event_group(void);

// 注册状态变化回调
void nm_register_status_change_callback(nm_status_change_cb_t callback, void* arg);
```

## 使用示例

### 启用快速监控

```c
// 初始化网络监控
nm_init();

// 启用快速监控模式
nm_enable_fast_monitoring(true);

// 设置自定义监控间隔
nm_set_monitoring_interval(800);

// 启动监控
nm_start_monitoring();
```

### 注册状态变化回调

```c
void network_status_callback(uint8_t index, const char* ip, int status, void* arg) {
    ESP_LOGI("CALLBACK", "网络状态变化: 目标%d (%s) 状态=%d", index, ip, status);
}

// 注册回调
nm_register_status_change_callback(network_status_callback, NULL);
```

### 使用事件组等待网络事件

```c
EventGroupHandle_t event_group = nm_get_event_group();

// 等待用户主机连接事件
EventBits_t events = xEventGroupWaitBits(
    event_group,
    NM_EVENT_USER_HOST_UP,
    pdTRUE,  // 清除事件位
    pdFALSE, // 等待指定事件
    pdMS_TO_TICKS(5000)  // 5秒超时
);

if (events & NM_EVENT_USER_HOST_UP) {
    ESP_LOGI("EVENT", "用户主机已连接!");
}
```

## 性能提升对比

| 指标 | 优化前 | 优化后（普通模式） | 优化后（快速模式） | 提升幅度 |
|------|--------|------------------|------------------|----------|
| 检测周期 | 23秒 | 8秒 | 3秒 | 65-87% |
| 状态变化响应 | 最长23秒 | 最长8秒 | 最长3秒 | 65-87% |
| CPU使用率 | 低 | 中等 | 较高 | - |
| 网络负载 | 低 | 中等 | 较高 | - |

## 使用建议

### 场景选择

1. **快速模式适用场景：**
   - 对网络状态变化敏感的应用
   - 需要快速故障检测的系统
   - 实时性要求高的网络监控

2. **普通模式适用场景：**
   - 日常网络状态监控
   - 资源受限的环境
   - 对实时性要求不高的应用

### 配置建议

1. **监控间隔设置：**
   - 快速模式：500-1000ms
   - 普通模式：2000-5000ms
   - 节能模式：10000ms以上

2. **回调函数使用：**
   - 保持回调函数轻量级
   - 避免在回调中执行耗时操作
   - 使用任务队列处理复杂逻辑

## 注意事项

1. **资源消耗：** 快速模式会增加CPU和网络使用率
2. **网络负载：** 频繁ping可能对网络造成负载
3. **稳定性：** 建议在生产环境中先进行充分测试
4. **兼容性：** 保持与原有BSP接口的完全兼容

## 总结

通过本次优化，网络监控系统的实时性得到了显著提升，响应速度提高了65-87%，同时保持了良好的系统稳定性和向后兼容性。新增的动态监控模式和事件通知机制为不同应用场景提供了灵活的配置选项。
