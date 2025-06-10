# LED Matrix Logo Display Controller 使用指南

## 概述

LED Matrix Logo Display Controller 是一个独立的模块，专门负责在32x32 LED矩阵上显示Logo动画。它与系统状态显示完全分离，专注于Logo展示功能。

## 架构特点

### 职责分离
- **LED Matrix**: 专门显示Logo动画（由LED Matrix Logo Display Controller管理）
- **Touch WS2812**: 显示系统状态和网络状态
- **Board WS2812**: 显示设备监控数据（温度、功率等）

### 独立性
- 独立的配置文件管理
- 独立的定时器系统
- 独立的显示模式控制
- 不依赖系统状态

## 主要功能

### 显示模式

1. **关闭模式** (`LOGO_DISPLAY_MODE_OFF`)
   - 关闭所有LED显示

2. **单一显示模式** (`LOGO_DISPLAY_MODE_SINGLE`) 
   - 显示单个Logo
   - 可配置显示时长

3. **序列显示模式** (`LOGO_DISPLAY_MODE_SEQUENCE`)
   - 按顺序显示多个Logo
   - 支持循环播放

4. **定时切换模式** (`LOGO_DISPLAY_MODE_TIMED_SWITCH`)
   - 定时自动切换Logo
   - 可配置切换间隔

5. **随机显示模式** (`LOGO_DISPLAY_MODE_RANDOM`)
   - 随机选择Logo显示
   - 可配置随机间隔范围

## API 接口

### 初始化和控制
```c
// 初始化Logo显示控制器
esp_err_t led_matrix_logo_display_init(const logo_display_config_t* config);

// 启动Logo显示服务
esp_err_t led_matrix_logo_display_start(void);

// 停止Logo显示服务
void led_matrix_logo_display_stop(void);

// 检查是否已初始化
bool led_matrix_logo_display_is_initialized(void);

// 检查是否正在运行
bool led_matrix_logo_display_is_running(void);
```

### 显示模式控制
```c
// 设置显示模式
esp_err_t led_matrix_logo_display_set_mode(logo_display_mode_t mode);

// 获取当前显示模式
logo_display_mode_t led_matrix_logo_display_get_mode(void);

// 显示指定Logo
esp_err_t led_matrix_logo_display_show_logo(const char* logo_name);

// 停止当前显示
esp_err_t led_matrix_logo_display_stop_current(void);
```

### 配置管理
```c
// 加载配置文件
esp_err_t led_matrix_logo_display_load_config(const char* config_file);

// 重新加载Logo文件
esp_err_t led_matrix_logo_display_reload_logos(void);

// 设置/获取亮度
esp_err_t led_matrix_logo_display_set_brightness(uint8_t brightness);
uint8_t led_matrix_logo_display_get_brightness(void);

// 设置显示时长
esp_err_t led_matrix_logo_display_set_duration(uint32_t duration_ms);

// 启用/禁用调试模式
void led_matrix_logo_display_set_debug(bool enable);
```

### 状态查询
```c
// 获取状态信息
esp_err_t led_matrix_logo_display_get_status(logo_display_status_t* status);

// 打印状态信息
void led_matrix_logo_display_print_status(void);

// 获取Logo数量
int led_matrix_logo_display_get_logo_count(void);

// 获取Logo名称列表
esp_err_t led_matrix_logo_display_get_logo_names(char** names, int max_count);
```

## 配置文件格式

### 主配置文件 (logo_config.json)
```json
{
  "led_matrix_logo_display": {
    "version": "1.0",
    "default_mode": "single",
    "timing_config": {
      "single_display_duration_ms": 5000,
      "sequence_interval_ms": 3000,
      "timed_switch_interval_ms": 10000,
      "random_min_interval_ms": 5000,
      "random_max_interval_ms": 15000
    },
    "logos": [
      {
        "name": "startup_logo",
        "filename": "startup.json",
        "description": "系统启动Logo"
      },
      {
        "name": "company_logo", 
        "filename": "company.json",
        "description": "公司标识Logo"
      }
    ],
    "display_sequences": [
      {
        "name": "boot_sequence",
        "logos": ["startup_logo", "company_logo"],
        "loop": false
      }
    ]
  }
}
```

### Logo文件格式 (startup.json)
```json
{
  "logo": {
    "name": "startup_logo",
    "version": "1.0",
    "matrix_size": {
      "width": 32,
      "height": 32
    },
    "animation": {
      "type": "static",
      "duration_ms": 5000,
      "brightness": 128,
      "color_correction": {
        "enabled": true,
        "white_point": {
          "r": 42,
          "g": 28, 
          "b": 19
        }
      }
    },
    "pixels": [
      {
        "x": 10,
        "y": 10,
        "r": 255,
        "g": 255,
        "b": 255,
        "comment": "中心亮点"
      }
    ]
  }
}
```

## TF卡文件结构

推荐的TF卡文件结构：
```
/sdcard/
├── logo_config.json          # 主配置文件
├── logos/                     # Logo文件目录
│   ├── startup.json          # 启动Logo
│   ├── company.json          # 公司Logo
│   ├── product.json          # 产品Logo
│   └── custom.json           # 自定义Logo
└── sequences/                 # 序列配置目录
    ├── boot_sequence.json    # 启动序列
    └── demo_sequence.json    # 演示序列
```

## 使用示例

### 基本使用
```c
// 初始化
led_matrix_logo_display_init(NULL);  // 使用默认配置

// 启动服务
led_matrix_logo_display_start();

// 设置为单一显示模式
led_matrix_logo_display_set_mode(LOGO_DISPLAY_MODE_SINGLE);

// 显示指定Logo
led_matrix_logo_display_show_logo("startup_logo");
```

### 高级使用
```c
// 自定义配置
logo_display_config_t config = {
    .default_mode = LOGO_DISPLAY_MODE_SEQUENCE,
    .single_display_duration_ms = 3000,
    .sequence_interval_ms = 2000,
    .brightness = 200,
    .auto_load_config = true,
    .debug_mode = true
};

// 使用自定义配置初始化
led_matrix_logo_display_init(&config);

// 加载特定配置文件
led_matrix_logo_display_load_config("/sdcard/custom_config.json");

// 设置为随机显示模式
led_matrix_logo_display_set_mode(LOGO_DISPLAY_MODE_RANDOM);
```

## 与BSP集成

在BSP初始化过程中，LED Matrix Logo Display Controller会自动集成：

```c
// BSP初始化时自动调用
bsp_board_init() {
    // ... 其他初始化 ...
    
    // 初始化LED Matrix Logo Display Controller
    ret = led_matrix_logo_display_init(NULL);
    if (ret == ESP_OK) {
        // 启动Logo显示服务
        led_matrix_logo_display_start();
    }
    
    // ... 继续其他初始化 ...
}
```

## 调试和监控

### 启用调试模式
```c
led_matrix_logo_display_set_debug(true);
```

### 查看状态
```c
// 打印详细状态
led_matrix_logo_display_print_status();

// 获取状态结构
logo_display_status_t status;
led_matrix_logo_display_get_status(&status);
ESP_LOGI(TAG, "当前模式: %d", status.current_mode);
ESP_LOGI(TAG, "当前Logo: %s", status.current_logo_name);
ESP_LOGI(TAG, "显示时间: %lu ms", status.display_duration);
```

## 注意事项

1. **文件路径**: 确保TF卡正确挂载，文件路径使用 `/sdcard/` 前缀
2. **内存使用**: JSON文件解析会占用内存，建议Logo文件不超过64KB
3. **颜色校准**: 使用推荐的白点参考值 (R:42, G:28, B:19)
4. **显示时长**: 合理设置显示时长，避免过于频繁的切换
5. **错误处理**: 检查API返回值，处理文件加载失败等异常情况

## 故障排除

### 常见问题

1. **Logo不显示**
   - 检查TF卡是否正确挂载
   - 检查JSON文件格式是否正确
   - 检查文件路径是否正确

2. **颜色显示异常**
   - 检查颜色校准设置
   - 调整亮度设置
   - 检查像素坐标是否在矩阵范围内

3. **切换不正常**
   - 检查定时器配置
   - 检查显示模式设置
   - 查看调试日志输出

### 调试命令
```c
// 重新加载配置
led_matrix_logo_display_load_config("/sdcard/logo_config.json");

// 重新加载Logo文件
led_matrix_logo_display_reload_logos();

// 手动显示测试Logo
led_matrix_logo_display_show_logo("test_logo");
```
