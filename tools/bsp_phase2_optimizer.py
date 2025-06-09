#!/usr/bin/env python3
"""
BSP第二阶段优化实施工具
自动化应用优化策略，将初始化时间从36.1秒进一步优化到15-20秒
"""

import os
import re
import shutil
from pathlib import Path
from datetime import datetime

class BSPPhase2Optimizer:
    def __init__(self, project_root):
        self.project_root = Path(project_root)
        self.bsp_src = self.project_root / "components" / "rm01_esp32s3_bsp" / "src"
        self.bsp_include = self.project_root / "components" / "rm01_esp32s3_bsp" / "include"
        self.backup_dir = self.project_root / "backup" / f"phase2_backup_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
        
    def execute_optimization(self):
        """执行第二阶段优化"""
        print("=== BSP第二阶段优化执行 ===")
        print("目标: 从36.1秒优化到15-20秒")
        
        # 创建备份
        self._create_backup()
        
        # 执行各项优化
        self._optimize_async_webserver()
        self._optimize_parallel_hardware_init()
        self._optimize_smart_waiting()
        self._optimize_delayed_services()
        self._add_performance_monitoring()
        
        print("\n=== 第二阶段优化完成 ===")
        print("请重新编译项目并测试性能改进")
        
    def _create_backup(self):
        """创建优化前的备份"""
        print(f"创建备份到: {self.backup_dir}")
        self.backup_dir.mkdir(parents=True, exist_ok=True)
        
        # 备份关键文件
        files_to_backup = [
            "bsp_board.c",
            "bsp_board.h", 
            "bsp_webserver.c",
            "network_monitor.c",
            "bsp_system_state.c"
        ]
        
        for file_name in files_to_backup:
            src_file = self.bsp_src / file_name if file_name.endswith('.c') else self.bsp_include / file_name
            if src_file.exists():
                shutil.copy2(src_file, self.backup_dir / file_name)
                print(f"  备份: {file_name}")
                
    def _optimize_async_webserver(self):
        """优化1: 异步Web服务器启动"""
        print("\n优化1: 实施异步Web服务器启动")
        
        bsp_board_file = self.bsp_src / "bsp_board.c"
        if not bsp_board_file.exists():
            print("  错误: bsp_board.c 文件未找到")
            return
            
        with open(bsp_board_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # 添加异步任务句柄声明
        if "webserver_async_init_handle" not in content:
            # 在文件开头添加异步任务句柄
            include_section = content.find('#include "bsp_webserver.h"')
            if include_section != -1:
                insert_pos = content.find('\n', include_section) + 1
                async_handles = """
// 异步初始化任务句柄
static TaskHandle_t webserver_async_init_handle = NULL;
static TaskHandle_t network_monitor_async_init_handle = NULL;
static EventGroupHandle_t async_init_event_group = NULL;

// 异步初始化事件位
#define WEBSERVER_INIT_DONE_BIT    BIT0
#define NETWORK_MONITOR_INIT_DONE_BIT BIT1
"""
                content = content[:insert_pos] + async_handles + content[insert_pos:]
        
        # 修改Web服务器初始化为异步
        webserver_init_pattern = r'esp_err_t bsp_init_webserver_service\(void\) \{[^}]+esp_err_t ret = bsp_start_webserver\(\);[^}]+\}'
        
        async_webserver_impl = '''esp_err_t bsp_init_webserver_service(void) {
    ESP_LOGI(TAG, "启动Web服务器服务(异步模式)");
    
    // 创建异步初始化事件组
    if (async_init_event_group == NULL) {
        async_init_event_group = xEventGroupCreate();
        if (async_init_event_group == NULL) {
            ESP_LOGE(TAG, "创建异步初始化事件组失败");
            return ESP_ERR_NO_MEM;
        }
    }
    
    BaseType_t ret = xTaskCreate(
        webserver_async_init_task,
        "web_async_init",
        6144,
        NULL,
        3,
        &webserver_async_init_handle
    );
    
    if (ret == pdPASS) {
        ESP_LOGI(TAG, "Web服务器异步初始化任务已启动");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Web服务器异步初始化任务创建失败");
        return ESP_FAIL;
    }
}'''
        
        if re.search(webserver_init_pattern, content, re.DOTALL):
            content = re.sub(webserver_init_pattern, async_webserver_impl, content, flags=re.DOTALL)
        
        # 添加异步任务实现
        if "webserver_async_init_task" not in content:
            async_task_impl = '''
// 异步Web服务器初始化任务
static void webserver_async_init_task(void *pvParameters) {
    ESP_LOGI(TAG, "执行Web服务器异步初始化");
    
    esp_err_t ret = bsp_start_webserver();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Web服务器启动成功 - http://10.10.99.97/");
        xEventGroupSetBits(async_init_event_group, WEBSERVER_INIT_DONE_BIT);
    } else {
        ESP_LOGE(TAG, "Web服务器启动失败: %s", esp_err_to_name(ret));
    }
    
    // 清理任务句柄
    webserver_async_init_handle = NULL;
    vTaskDelete(NULL);
}

// 异步网络监控初始化任务
static void network_monitor_async_init_task(void *pvParameters) {
    ESP_LOGI(TAG, "执行网络监控异步初始化");
    
    // 等待网络硬件就绪
    vTaskDelay(pdMS_TO_TICKS(500));
    
    nm_init();
    nm_start_monitoring();
    
    // 短暂等待收集初始数据
    vTaskDelay(pdMS_TO_TICKS(300));
    
    ESP_LOGI(TAG, "网络监控异步初始化完成");
    xEventGroupSetBits(async_init_event_group, NETWORK_MONITOR_INIT_DONE_BIT);
    
    // 清理任务句柄
    network_monitor_async_init_handle = NULL;
    vTaskDelete(NULL);
}

// 检查异步服务是否就绪
static bool bsp_is_async_service_ready(EventBits_t service_bit) {
    if (async_init_event_group == NULL) {
        return false;
    }
    
    EventBits_t bits = xEventGroupGetBits(async_init_event_group);
    return (bits & service_bit) != 0;
}
'''
            content += async_task_impl
        
        # 写入修改后的内容
        with open(bsp_board_file, 'w', encoding='utf-8') as f:
            f.write(content)
            
        print("  ✓ 已实施异步Web服务器启动")
        
    def _optimize_parallel_hardware_init(self):
        """优化2: 并行硬件初始化"""
        print("\n优化2: 实施并行硬件初始化")
        
        bsp_board_file = self.bsp_src / "bsp_board.c"
        
        with open(bsp_board_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # 添加并行初始化任务
        if "parallel_hardware_init_task" not in content:
            parallel_tasks = '''
// 并行硬件初始化任务
static TaskHandle_t power_init_task_handle = NULL;
static TaskHandle_t ws2812_init_task_handle = NULL;
static EventGroupHandle_t hardware_init_event_group = NULL;

#define POWER_INIT_DONE_BIT     BIT0
#define WS2812_INIT_DONE_BIT    BIT1
#define ALL_HARDWARE_INIT_BITS  (POWER_INIT_DONE_BIT | WS2812_INIT_DONE_BIT)

// 电源管理并行初始化任务
static void power_init_parallel_task(void *pvParameters) {
    ESP_LOGI(TAG, "并行执行电源管理初始化");
    
    bsp_power_init();
    
    ESP_LOGI(TAG, "电源管理并行初始化完成");
    xEventGroupSetBits(hardware_init_event_group, POWER_INIT_DONE_BIT);
    power_init_task_handle = NULL;
    vTaskDelete(NULL);
}

// WS2812并行初始化任务
static void ws2812_init_parallel_task(void *pvParameters) {
    ESP_LOGI(TAG, "并行执行WS2812初始化");
    
    // 初始化所有WS2812
    esp_err_t ret = bsp_ws2812_init_all();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WS2812初始化失败: %s", esp_err_to_name(ret));
    }
    
    // 快速验证
    ret = bsp_ws2812_set_pixel(BSP_WS2812_TOUCH, 0, 50, 50, 50);
    if (ret == ESP_OK) {
        bsp_ws2812_refresh(BSP_WS2812_TOUCH);
    }
    
    ESP_LOGI(TAG, "WS2812并行初始化完成");
    xEventGroupSetBits(hardware_init_event_group, WS2812_INIT_DONE_BIT);
    ws2812_init_task_handle = NULL;
    vTaskDelete(NULL);
}

// 启动并行硬件初始化
static esp_err_t bsp_start_parallel_hardware_init(void) {
    ESP_LOGI(TAG, "启动并行硬件初始化");
    
    // 创建硬件初始化事件组
    if (hardware_init_event_group == NULL) {
        hardware_init_event_group = xEventGroupCreate();
        if (hardware_init_event_group == NULL) {
            ESP_LOGE(TAG, "创建硬件初始化事件组失败");
            return ESP_ERR_NO_MEM;
        }
    }
    
    // 启动电源管理初始化任务
    BaseType_t ret = xTaskCreate(
        power_init_parallel_task,
        "power_init_parallel",
        4096,
        NULL,
        4,
        &power_init_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建电源管理并行初始化任务失败");
        return ESP_FAIL;
    }
    
    // 启动WS2812初始化任务
    ret = xTaskCreate(
        ws2812_init_parallel_task,
        "ws2812_init_parallel",
        3072,
        NULL,
        3,
        &ws2812_init_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建WS2812并行初始化任务失败");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// 等待并行硬件初始化完成
static esp_err_t bsp_wait_parallel_hardware_init(uint32_t timeout_ms) {
    ESP_LOGI(TAG, "等待并行硬件初始化完成...");
    
    EventBits_t bits = xEventGroupWaitBits(
        hardware_init_event_group,
        ALL_HARDWARE_INIT_BITS,
        pdFALSE,
        pdTRUE,
        pdMS_TO_TICKS(timeout_ms)
    );
    
    if ((bits & ALL_HARDWARE_INIT_BITS) == ALL_HARDWARE_INIT_BITS) {
        ESP_LOGI(TAG, "所有硬件组件并行初始化完成");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "部分硬件组件初始化超时 (完成: 0x%x, 期望: 0x%x)", 
                 (unsigned)bits, (unsigned)ALL_HARDWARE_INIT_BITS);
        return ESP_ERR_TIMEOUT;
    }
}
'''
            content += parallel_tasks
        
        # 修改主初始化流程以使用并行初始化
        # 查找并替换电源和WS2812的串行初始化
        original_power_init = r'bsp_power_init\(\);'
        if re.search(original_power_init, content):
            # 将串行初始化替换为并行启动
            replacement = '''// 启动并行硬件初始化 (电源管理 + WS2812)
    ret = bsp_start_parallel_hardware_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "并行硬件初始化启动失败: %s", esp_err_to_name(ret));
        return ret;
    }'''
            content = re.sub(original_power_init, replacement, content)
        
        # 写入修改后的内容
        with open(bsp_board_file, 'w', encoding='utf-8') as f:
            f.write(content)
            
        print("  ✓ 已实施并行硬件初始化")
        
    def _optimize_smart_waiting(self):
        """优化3: 智能等待机制"""
        print("\n优化3: 实施智能等待机制")
        
        bsp_board_file = self.bsp_src / "bsp_board.c"
        
        with open(bsp_board_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # 添加智能等待函数
        if "smart_wait_for_condition" not in content:
            smart_wait_impl = '''
// 智能等待条件满足（替代固定延迟）
typedef bool (*condition_check_fn_t)(void);

static esp_err_t smart_wait_for_condition(condition_check_fn_t check_fn, 
                                         uint32_t timeout_ms, 
                                         uint32_t check_interval_ms,
                                         const char* condition_name) {
    ESP_LOGI(TAG, "智能等待: %s (超时: %"PRIu32"ms)", condition_name, timeout_ms);
    
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        if (check_fn && check_fn()) {
            ESP_LOGI(TAG, "条件满足: %s (耗时: %"PRIu32"ms)", condition_name, elapsed);
            return ESP_OK;
        }
        
        vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
        elapsed += check_interval_ms;
    }
    
    ESP_LOGW(TAG, "等待超时: %s", condition_name);
    return ESP_ERR_TIMEOUT;
}

// 检查Web服务器是否就绪
static bool is_webserver_ready(void) {
    return bsp_is_async_service_ready(WEBSERVER_INIT_DONE_BIT);
}

// 检查网络监控是否就绪
static bool is_network_monitor_ready(void) {
    return bsp_is_async_service_ready(NETWORK_MONITOR_INIT_DONE_BIT);
}

// 检查硬件初始化是否完成
static bool is_hardware_init_complete(void) {
    if (hardware_init_event_group == NULL) return false;
    EventBits_t bits = xEventGroupGetBits(hardware_init_event_group);
    return (bits & ALL_HARDWARE_INIT_BITS) == ALL_HARDWARE_INIT_BITS;
}
'''
            content += smart_wait_impl
        
        # 替换固定延迟为智能等待
        delay_patterns = [
            (r'vTaskDelay\(pdMS_TO_TICKS\((\d+)\)\);\s*// 等待网络监控系统启动',
             'smart_wait_for_condition(is_network_monitor_ready, \\1, 100, "网络监控系统启动");'),
            (r'vTaskDelay\(pdMS_TO_TICKS\((\d+)\)\);\s*// 等待.*收集初始数据',
             'smart_wait_for_condition(is_network_monitor_ready, \\1, 50, "网络监控收集初始数据");')
        ]
        
        for pattern, replacement in delay_patterns:
            content = re.sub(pattern, replacement, content)
        
        # 写入修改后的内容
        with open(bsp_board_file, 'w', encoding='utf-8') as f:
            f.write(content)
            
        print("  ✓ 已实施智能等待机制")
        
    def _optimize_delayed_services(self):
        """优化4: 延迟服务启动"""
        print("\n优化4: 实施延迟服务启动")
        
        bsp_board_file = self.bsp_src / "bsp_board.c"
        
        with open(bsp_board_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # 添加延迟服务初始化
        if "delayed_services_init_task" not in content:
            delayed_services_impl = '''
// 延迟服务初始化任务
static TaskHandle_t delayed_services_task_handle = NULL;

static void delayed_services_init_task(void *pvParameters) {
    ESP_LOGI(TAG, "启动延迟服务初始化 (延迟3秒)");
    
    // 延迟启动，让关键服务先稳定运行
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "开始初始化非关键监控服务");
    
    // 初始化系统状态控制器
    esp_err_t ret = bsp_system_state_init();
    if (ret == ESP_OK) {
        bsp_system_state_start_monitoring();
        ESP_LOGI(TAG, "系统状态监控已启动");
    } else {
        ESP_LOGW(TAG, "系统状态监控启动失败: %s", esp_err_to_name(ret));
    }
    
    // 启动电源芯片测试（如果需要）
    bsp_power_test_start();
    ESP_LOGI(TAG, "电源芯片测试已启动");
    
    ESP_LOGI(TAG, "延迟服务初始化完成");
    delayed_services_task_handle = NULL;
    vTaskDelete(NULL);
}

// 启动延迟服务
static esp_err_t bsp_start_delayed_services(void) {
    BaseType_t ret = xTaskCreate(
        delayed_services_init_task,
        "delayed_services",
        4096,
        NULL,
        2,  // 低优先级
        &delayed_services_task_handle
    );
    
    if (ret == pdPASS) {
        ESP_LOGI(TAG, "延迟服务初始化任务已启动");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "延迟服务初始化任务创建失败");
        return ESP_FAIL;
    }
}
'''
            content += delayed_services_impl
        
        # 写入修改后的内容
        with open(bsp_board_file, 'w', encoding='utf-8') as f:
            f.write(content)
            
        print("  ✓ 已实施延迟服务启动")
        
    def _add_performance_monitoring(self):
        """优化5: 添加性能监控"""
        print("\n优化5: 添加性能监控")
        
        bsp_board_file = self.bsp_src / "bsp_board.c"
        
        with open(bsp_board_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # 添加性能监控结构
        if "bsp_phase2_performance_t" not in content:
            perf_monitoring = '''
// 第二阶段优化性能监控
typedef struct {
    uint32_t total_init_start_time;
    uint32_t webserver_async_start_time;
    uint32_t hardware_parallel_start_time;
    uint32_t hardware_parallel_complete_time;
    uint32_t delayed_services_start_time;
    uint32_t total_init_complete_time;
    
    bool webserver_ready;
    bool network_monitor_ready;
    bool hardware_init_complete;
    bool delayed_services_started;
    
    uint32_t original_time_estimate_ms;    // 原始预估时间
    uint32_t optimized_time_ms;           // 实际优化后时间
    uint32_t time_saved_ms;               // 节省的时间
    float improvement_percent;             // 改进百分比
} bsp_phase2_performance_t;

static bsp_phase2_performance_t phase2_perf = {
    .original_time_estimate_ms = 36100,  // 基于第一阶段优化结果
    .webserver_ready = false,
    .network_monitor_ready = false,
    .hardware_init_complete = false,
    .delayed_services_started = false
};

// 记录性能时间点
static void bsp_perf_mark_milestone(const char* milestone_name, uint32_t* time_field) {
    if (time_field) {
        *time_field = xTaskGetTickCount() * portTICK_PERIOD_MS;
        ESP_LOGI(TAG, "性能里程碑: %s 在 %"PRIu32"ms", milestone_name, *time_field);
    }
}

// 更新性能状态
static void bsp_perf_update_status(const char* component, bool* status_field, bool new_status) {
    if (status_field && *status_field != new_status) {
        *status_field = new_status;
        ESP_LOGI(TAG, "性能状态更新: %s = %s", component, new_status ? "就绪" : "未就绪");
    }
}

// 计算并打印最终性能报告
static void bsp_print_phase2_performance_report(void) {
    phase2_perf.optimized_time_ms = phase2_perf.total_init_complete_time - phase2_perf.total_init_start_time;
    phase2_perf.time_saved_ms = phase2_perf.original_time_estimate_ms - phase2_perf.optimized_time_ms;
    phase2_perf.improvement_percent = ((float)phase2_perf.time_saved_ms / phase2_perf.original_time_estimate_ms) * 100.0f;
    
    ESP_LOGI(TAG, "=== BSP第二阶段优化性能报告 ===");
    ESP_LOGI(TAG, "原始时间(第一阶段): %"PRIu32"ms (%.1f秒)", 
             phase2_perf.original_time_estimate_ms, phase2_perf.original_time_estimate_ms / 1000.0f);
    ESP_LOGI(TAG, "优化后时间: %"PRIu32"ms (%.1f秒)", 
             phase2_perf.optimized_time_ms, phase2_perf.optimized_time_ms / 1000.0f);
    ESP_LOGI(TAG, "节省时间: %"PRIu32"ms (%.1f秒)", 
             phase2_perf.time_saved_ms, phase2_perf.time_saved_ms / 1000.0f);
    ESP_LOGI(TAG, "性能改进: %.1f%%", phase2_perf.improvement_percent);
    
    ESP_LOGI(TAG, "关键里程碑时间:");
    ESP_LOGI(TAG, "  硬件并行初始化: %"PRIu32"ms", 
             phase2_perf.hardware_parallel_complete_time - phase2_perf.hardware_parallel_start_time);
    ESP_LOGI(TAG, "  异步服务启动: %"PRIu32"ms", 
             phase2_perf.webserver_async_start_time - phase2_perf.total_init_start_time);
    
    ESP_LOGI(TAG, "组件状态:");
    ESP_LOGI(TAG, "  Web服务器: %s", phase2_perf.webserver_ready ? "就绪" : "未就绪");
    ESP_LOGI(TAG, "  网络监控: %s", phase2_perf.network_monitor_ready ? "就绪" : "未就绪");
    ESP_LOGI(TAG, "  硬件初始化: %s", phase2_perf.hardware_init_complete ? "完成" : "未完成");
    
    if (phase2_perf.optimized_time_ms <= 20000) {
        ESP_LOGI(TAG, "✓ 达成第二阶段优化目标: ≤ 20秒");
    } else if (phase2_perf.optimized_time_ms <= 25000) {
        ESP_LOGI(TAG, "⚠ 接近优化目标: 还有进一步优化空间");
    } else {
        ESP_LOGW(TAG, "✗ 未达成优化目标: 需要额外优化措施");
    }
    ESP_LOGI(TAG, "================================");
}
'''
            content += perf_monitoring
        
        # 写入修改后的内容
        with open(bsp_board_file, 'w', encoding='utf-8') as f:
            f.write(content)
            
        print("  ✓ 已添加性能监控")
        
    def generate_optimized_init_function(self):
        """生成优化后的主初始化函数"""
        print("\n生成优化后的主初始化函数")
        
        optimized_init = '''
// BSP第二阶段优化版初始化函数
esp_err_t bsp_board_init_phase2_optimized(void) {
    ESP_LOGI(TAG, "开始BSP第二阶段优化版初始化");
    ESP_LOGI(TAG, "目标: 从36.1秒优化到15-20秒 (45-58%改进)");
    
    // 记录开始时间
    bsp_perf_mark_milestone("初始化开始", &phase2_perf.total_init_start_time);
    
    esp_err_t ret;
    
    // ========== 阶段1: 关键路径优先初始化 (0-3秒) ==========
    ESP_LOGI(TAG, "阶段1: 关键路径优先初始化");
    
    // 最优先: LED系统（提供早期状态指示）
    ESP_LOGI(TAG, "优先初始化LED矩阵系统");
    led_matrix_init();
    ret = bsp_start_animation_task();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LED动画任务启动失败，但继续初始化");
    }
    
    // 次优先: 网络硬件（为后续服务准备）
    ESP_LOGI(TAG, "初始化网络硬件控制器");
    ret = bsp_w5500_init(SPI3_HOST);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "W5500网络控制器初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // ========== 阶段2: 并行硬件初始化 (1-4秒) ==========
    ESP_LOGI(TAG, "阶段2: 并行硬件初始化");
    bsp_perf_mark_milestone("硬件并行初始化开始", &phase2_perf.hardware_parallel_start_time);
    
    ret = bsp_start_parallel_hardware_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "并行硬件初始化启动失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // ========== 阶段3: 异步服务启动 (2-5秒) ==========
    ESP_LOGI(TAG, "阶段3: 异步服务启动");
    bsp_perf_mark_milestone("异步服务启动", &phase2_perf.webserver_async_start_time);
    
    // 异步启动Web服务器
    ret = bsp_init_webserver_service();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Web服务器异步启动失败，但继续初始化");
    }
    
    // 异步启动网络监控
    ret = bsp_init_network_monitoring_service();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "网络监控异步启动失败，但继续初始化");
    }
    
    // ========== 阶段4: 智能等待关键服务 (3-8秒) ==========
    ESP_LOGI(TAG, "阶段4: 智能等待关键服务就绪");
    
    // 等待并行硬件初始化完成
    ret = bsp_wait_parallel_hardware_init(5000);
    if (ret == ESP_OK) {
        bsp_perf_mark_milestone("硬件并行初始化完成", &phase2_perf.hardware_parallel_complete_time);
        bsp_perf_update_status("硬件初始化", &phase2_perf.hardware_init_complete, true);
    } else {
        ESP_LOGW(TAG, "硬件并行初始化部分超时，但继续");
    }
    
    // 智能等待Web服务器就绪
    ret = smart_wait_for_condition(is_webserver_ready, 3000, 100, "Web服务器就绪");
    if (ret == ESP_OK) {
        bsp_perf_update_status("Web服务器", &phase2_perf.webserver_ready, true);
        ESP_LOGI(TAG, "Web服务器已就绪 - http://10.10.99.97/");
    } else {
        ESP_LOGW(TAG, "Web服务器启动超时，但系统可继续运行");
    }
    
    // 智能等待网络监控就绪
    ret = smart_wait_for_condition(is_network_monitor_ready, 2000, 50, "网络监控就绪");
    if (ret == ESP_OK) {
        bsp_perf_update_status("网络监控", &phase2_perf.network_monitor_ready, true);
        
        // 启动网络动画控制器
        bsp_network_animation_init();
        bsp_network_animation_start_monitoring();
    } else {
        ESP_LOGW(TAG, "网络监控启动超时，跳过网络动画功能");
    }
    
    // ========== 阶段5: 延迟启动非关键服务 (异步) ==========
    ESP_LOGI(TAG, "阶段5: 启动延迟服务");
    bsp_perf_mark_milestone("延迟服务启动", &phase2_perf.delayed_services_start_time);
    
    ret = bsp_start_delayed_services();
    if (ret == ESP_OK) {
        bsp_perf_update_status("延迟服务", &phase2_perf.delayed_services_started, true);
    } else {
        ESP_LOGW(TAG, "延迟服务启动失败，但核心功能已就绪");
    }
    
    // ========== 完成初始化 ==========
    bsp_perf_mark_milestone("初始化完成", &phase2_perf.total_init_complete_time);
    
    // 生成性能报告
    bsp_print_phase2_performance_report();
    
    ESP_LOGI(TAG, "=== BSP第二阶段优化版初始化完成 ===");
    ESP_LOGI(TAG, "系统核心功能已就绪，延迟服务将在后台继续初始化");
    
    return ESP_OK;
}
'''
        
        bsp_board_file = self.bsp_src / "bsp_board.c"
        with open(bsp_board_file, 'a', encoding='utf-8') as f:
            f.write(optimized_init)
            
        print("  ✓ 已生成优化后的主初始化函数")

def main():
    project_root = r"c:\Users\sprin\rm01-bsp"
    optimizer = BSPPhase2Optimizer(project_root)
    
    print("BSP第二阶段优化工具")
    print("="*50)
    print("目标: 将初始化时间从36.1秒优化到15-20秒")
    print("策略: 异步启动 + 并行初始化 + 智能等待 + 延迟服务")
    print()
    
    # 执行优化
    optimizer.execute_optimization()
    
    # 生成优化后的初始化函数
    optimizer.generate_optimized_init_function()
    
    print("\n" + "="*50)
    print("第二阶段优化实施完成！")
    print()
    print("下一步操作:")
    print("1. 在main.c中调用 bsp_board_init_phase2_optimized() 替代原函数")
    print("2. 重新编译项目: idf.py build")
    print("3. 烧录测试: idf.py flash monitor")
    print("4. 观察初始化时间和性能报告")
    print()
    print("预期效果:")
    print("- 初始化时间: 36.1秒 → 15-20秒")
    print("- 性能改进: 45-58%")
    print("- Web服务器: 15秒内可访问")
    print("- 关键功能: 10秒内就绪")

if __name__ == "__main__":
    main()
