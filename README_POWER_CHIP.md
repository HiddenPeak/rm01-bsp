# 电源芯片UART通信功能

## 功能概述

本功能为ESP32S3 BSP添加了通过UART读取连接在GPIO47引脚上的电源芯片信息的能力。电源芯片通过UART串口输出电源供应信息，包括电压、电流、功率和温度数据。

## 硬件连接

- **UART RX引脚**: GPIO47 
- **UART TX引脚**: 不使用（设为-1）
- **波特率**: 9600 bps（可在`bsp_power.h`中修改`BSP_POWER_UART_BAUDRATE`）
- **数据位**: 8位
- **停止位**: 1位
- **校验位**: 无

## 支持的数据格式

电源芯片数据解析器支持以下两种格式：

### 格式1（推荐）
```
V:12.34,I:1.23,P:15.16,T:25.6\r\n
```
- V: 电压值（伏特）
- I: 电流值（安培）  
- P: 功率值（瓦特）
- T: 温度值（摄氏度）

### 格式2
```
12.34V 1.23A 15.16W 25.6C\r\n
```

## API接口

### 初始化函数
```c
void bsp_power_chip_uart_init(void);
```
- 初始化UART通信和启动后台监控任务
- 在`bsp_power_init()`中自动调用

### 数据读取函数
```c
esp_err_t bsp_get_power_chip_data(bsp_power_chip_data_t *data);
```
- 主动读取电源芯片数据（1秒超时）
- 返回值：ESP_OK成功，ESP_ERR_TIMEOUT超时，其他错误码

### 获取缓存数据
```c
const bsp_power_chip_data_t* bsp_get_latest_power_chip_data(void);
```
- 获取后台任务缓存的最新数据（每2秒更新）
- 返回值：有效数据指针或NULL（数据无效/过期）

### 停止监控
```c
void bsp_power_chip_monitor_stop(void);
```
- 停止后台监控任务并释放UART资源

## 数据结构

```c
typedef struct {
    float voltage;          // 电压值 (V)
    float current;          // 电流值 (A)
    float power;           // 功率值 (W)
    float temperature;     // 温度值 (°C)
    uint32_t timestamp;    // 时间戳
    bool valid;            // 数据有效性
} bsp_power_chip_data_t;
```

## 使用示例

### 主动读取数据
```c
bsp_power_chip_data_t chip_data;
esp_err_t ret = bsp_get_power_chip_data(&chip_data);

if (ret == ESP_OK) {
    printf("电压: %.2fV, 电流: %.3fA, 功率: %.2fW, 温度: %.1f°C\n", 
           chip_data.voltage, chip_data.current, 
           chip_data.power, chip_data.temperature);
}
```

### 获取缓存数据
```c
const bsp_power_chip_data_t* latest_data = bsp_get_latest_power_chip_data();
if (latest_data != NULL) {
    printf("最新数据 - 电压: %.2fV\n", latest_data->voltage);
}
```

### 综合电源状态
```c
// 获取主电源、辅助电源和电源芯片状态
show_power_system_status();
```

## 配置说明

在`components/rm01_esp32s3_bsp/include/bsp_power.h`中可以修改以下配置：

```c
#define BSP_POWER_UART_PORT      UART_NUM_1    // UART端口
#define BSP_POWER_UART_RX_PIN    47            // 接收引脚
#define BSP_POWER_UART_BAUDRATE  9600          // 波特率
```

## 后台监控任务

系统会自动启动一个后台任务：
- **任务名称**: `power_chip_monitor`
- **更新频率**: 每2秒读取一次数据
- **栈大小**: 4096字节
- **优先级**: 5
- **数据过期时间**: 5秒（超过5秒未更新则标记为无效）

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
- `main/power_chip_test.c` - 测试实现
- `main/power_chip_test.h` - 测试头文件

测试程序会：
- 演示主动读取和缓存数据获取两种方式
- 定期显示电源系统综合状态
- 提供详细的日志输出用于调试

## 注意事项

1. GPIO47必须专用于电源芯片UART通信
2. 电源芯片数据格式必须与解析器匹配
3. 后台任务会持续运行，注意系统资源管理
4. 数据有效性检查基于时间戳，确保系统时间正确
