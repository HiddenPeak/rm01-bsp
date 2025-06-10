# 网络模块接口标准化计划

## 当前接口分析

### network_monitor.h 接口分类：

#### 1. 🎯 **核心接口（保持不变）**
```c
// 生命周期管理
void nm_init(void);
void nm_start_monitoring(void);
void nm_stop_monitoring(void);

// 状态查询
nm_status_t nm_get_status(const char* ip);
bool nm_get_target_info(const char* ip, nm_target_t* target_info);
void nm_get_all_status(void);

// 回调注册
void nm_register_status_change_callback(nm_status_change_cb_t callback, void* arg);
```

#### 2. 🔧 **配置接口（标准化前缀）**
```c
// 当前接口
void nm_enable_fast_monitoring(bool enable);
void nm_enable_adaptive_monitoring(bool enable); 
void nm_enable_network_quality_monitoring(bool enable);
void nm_enable_concurrent_monitoring(bool enable);

// 建议标准化为 nm_config_* 前缀
void nm_config_fast_mode(bool enable);
void nm_config_adaptive_mode(bool enable);
void nm_config_quality_monitoring(bool enable);
void nm_config_concurrent_mode(bool enable);
```

#### 3. 📊 **性能接口（标准化前缀）**
```c
// 当前接口
void nm_get_performance_stats(nm_performance_stats_t* stats);
void nm_get_performance_metrics(nm_performance_metrics_t* metrics);
void nm_reset_performance_stats(void);
void nm_reset_performance_metrics(void);

// 建议标准化为 nm_perf_* 前缀
void nm_perf_get_stats(nm_performance_stats_t* stats);
void nm_perf_get_metrics(nm_performance_metrics_t* metrics);
void nm_perf_reset_stats(void);
void nm_perf_reset_metrics(void);
```

#### 4. 🔄 **兼容性接口（保持兼容）**
```c
// BSP兼容接口 - 保持现有命名
const network_target_t* nm_get_network_targets(void);
void nm_start_network_monitor(void);
void nm_stop_network_monitor(void);
void nm_get_network_status(void);
```

#### 5. ⚡ **高性能接口（保持现有）**
```c
// 无锁接口 - 性能优化
nm_status_t nm_get_status_lockfree(const char* ip);
const nm_target_t* nm_get_targets_readonly(void);
```

## 建议的标准化策略

### 阶段1：新增标准化接口（保持兼容）
- 添加 `nm_config_*` 系列接口
- 添加 `nm_perf_*` 系列接口 
- 保留原有接口，标记为deprecated

### 阶段2：逐步迁移
- 更新BSP内部使用的接口
- 更新示例代码
- 添加迁移指南

### 阶段3：清理旧接口
- 移除deprecated接口
- 清理兼容代码

## 命名规范

### 前缀规则
- `nm_` - 网络监控核心功能
- `nm_config_` - 配置管理
- `nm_perf_` - 性能统计
- `nm_diag_` - 诊断功能

### 动词规则
- `get/set` - 获取/设置数据
- `enable/disable` - 启用/禁用功能
- `start/stop` - 启动/停止服务
- `reset/clear` - 重置/清空数据

### 参数规则
- bool类型用于enable/disable
- 指针用于输出参数
- const用于只读参数

## 实施优先级

1. **高优先级**：标准化配置接口命名
2. **中优先级**：标准化性能接口命名
3. **低优先级**：清理deprecated接口
