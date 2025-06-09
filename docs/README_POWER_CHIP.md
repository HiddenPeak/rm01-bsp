# 电源芯片UART通信功能

## 功能概述

本功能为ESP32S3 BSP添加了通过UART读取连接在GPIO47引脚上的电源芯片信息的能力。电源芯片通过UART串口输出电源供应信息，包括电压、电流和功率数据。

**重要更新**: 电源芯片数据作为协商信息，仅在以下情况下触发读取：
- 系统上电时进行首次协商
- 检测到电源电压变化时自动触发协商
- 手动调用触发函数时

这种按需读取的方式提高了系统效率，避免了不必要的持续通信。

## 硬件连接

- **UART RX引脚**: GPIO47 
- **UART TX引脚**: 不使用（设为-1）
- **波特率**: 9600 bps（可在`bsp_power.h`中修改`BSP_POWER_UART_BAUDRATE`）
- **数据位**: 8位
- **停止位**: 1位
- **校验位**: 无

## 支持的数据格式

XSP16电源芯片数据解析器支持以下格式：

### XSP16格式（当前使用）
```
[0xFF包头][电压字节][电流字节][CRC校验]
```
- 包头: 0xFF（固定）
- 电压字节: 十六进制值，直接转换为十进制电压值
- 电流字节: 十六进制值，转换为十进制后除以10得到电流值
- CRC校验: 对前3字节进行CRC8校验

**注意**: XSP16芯片不提供温度数据，仅输出电压、电流和功率信息。

## API接口

### 初始化函数
```c
void bsp_power_chip_uart_init(void);
```
- 初始化UART通信和启动电压监控任务
- 在`bsp_power_init()`中自动调用

### 电源协商触发函数
```c
void bsp_trigger_power_chip_negotiation(void);
```
- 手动触发电源芯片协商读取
- 用于需要立即获取电源协商信息的场景

### 电压变化阈值设置
```c
void bsp_set_voltage_change_threshold(float main_threshold, float aux_threshold);
```
- 设置电压变化监控阈值
- main_threshold: 主电源电压变化阈值(V)，默认0.5V
- aux_threshold: 辅助电源电压变化阈值(V)，默认0.5V

### 数据读取函数
```c
esp_err_t bsp_get_power_chip_data(bsp_power_chip_data_t *data);
```
- 主动读取电源芯片数据（1秒超时）
- 返回值：ESP_OK成功，ESP_ERR_TIMEOUT超时，其他错误码
- **注意**: 这是协商信息，建议仅在必要时调用

### 获取缓存数据
```c
const bsp_power_chip_data_t* bsp_get_latest_power_chip_data(void);
```
- 获取最新的协商数据（由电压变化事件触发更新）
- 返回值：有效数据指针或NULL（数据无效/过期）

### 停止监控
```c
void bsp_power_chip_monitor_stop(void);
```
- 停止电压监控任务并释放UART资源

## 数据结构

```c
typedef struct {
    float voltage;          // 电压值 (V)
    float current;          // 电流值 (A)
    float power;           // 功率值 (W)
    uint32_t timestamp;    // 协商发生时的系统运行时间 (毫秒)
    bool valid;            // 数据有效性
} bsp_power_chip_data_t;
```

## 使用示例

### 设置电压变化阈值
```c
// 设置电压变化检测阈值，避免频繁触发协商
bsp_set_voltage_change_threshold(3.0f, 3.0f); // 3.0V变化才触发协商

// 如需更敏感的检测，可以设置较小的阈值
bsp_set_voltage_change_threshold(1.0f, 1.0f); // 1.0V变化即触发协商
```

### 手动触发电源协商
```c
// 在需要立即获取电源协商信息时调用
bsp_trigger_power_chip_negotiation();
```

### 主动读取协商数据（谨慎使用）
```c
bsp_power_chip_data_t chip_data;
esp_err_t ret = bsp_get_power_chip_data(&chip_data);

if (ret == ESP_OK) {
    printf("协商数据 - 电压: %.2fV, 电流: %.3fA, 功率: %.2fW\n", 
           chip_data.voltage, chip_data.current, chip_data.power);
}
```

### 获取缓存的协商数据
```c
const bsp_power_chip_data_t* latest_data = bsp_get_latest_power_chip_data();
if (latest_data != NULL) {
    printf("最新协商数据 - 电压: %.2fV\n", latest_data->voltage);
}
```

### 综合电源状态
```c
// 获取主电源、辅助电源和电源芯片协商状态
show_power_system_status();
```

## 配置说明

在`components/rm01_esp32s3_bsp/include/bsp_power.h`中可以修改以下配置：

```c
#define BSP_POWER_UART_PORT      UART_NUM_1    // UART端口
#define BSP_POWER_UART_RX_PIN    47            // 接收引脚
#define BSP_POWER_UART_BAUDRATE  9600          // 波特率
```

### 电压变化监控配置
- **默认主电源阈值**: 3.0V
- **默认辅助电源阈值**: 3.0V
- **监控周期**: 每2秒检查一次电压变化
- **数据过期时间**: 30分钟（协商数据超过30分钟未更新则标记为无效）

## 电压变化监控机制

系统会自动启动一个电压监控任务：
- **任务名称**: `voltage_monitor`
- **检查频率**: 每2秒检查一次电压变化
- **栈大小**: 4096字节
- **优先级**: 4
- **触发条件**: 
  - 系统启动时执行首次协商
  - 主电源或辅助电源电压变化超过设定阈值时自动触发协商

## 故障排除

### 常见问题

1. **UART初始化失败**
   - 检查GPIO47是否被其他模块占用
   - 确认UART_NUM_1端口是否可用

2. **数据解析失败**
   - 检查电源芯片输出格式是否匹配
   - 确认波特率设置是否正确
   - 查看串口数据是否包含正确的分隔符

3. **读取超时**
   - 检查电源芯片是否正常工作
   - 确认硬件连接是否正确
   - 验证电源芯片是否按预期发送数据

### 调试建议

1. 启用详细日志查看UART通信状态
2. 使用示例代码`power_chip_test.c`进行功能测试
3. 检查电源芯片数据格式并根据需要修改解析函数

## 测试程序

项目包含完整的测试示例：
- `main/power_chip_test.c` - 基于电压变化触发的协商测试实现
- `main/power_chip_test.h` - 测试头文件

测试程序会：
- 演示电压变化阈值设置
- 展示手动触发协商和缓存数据获取两种方式
- 定期显示电源系统综合状态
- 提供详细的日志输出用于调试协商过程

## 注意事项

1. **协商特性**: 电源芯片数据是协商信息，不需要频繁读取
2. **触发条件**: 仅在系统启动和电压变化时自动触发协商
3. **GPIO占用**: GPIO47必须专用于电源芯片UART通信
4. **数据格式**: 电源芯片数据格式必须与解析器匹配
5. **资源管理**: 电压监控任务会持续运行，注意系统资源管理
6. **时间依赖**: 数据有效性检查基于时间戳，确保系统时间正确
7. **阈值调优**: 根据实际应用场景调整电压变化监控阈值
