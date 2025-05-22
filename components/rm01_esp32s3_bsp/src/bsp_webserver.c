#include "bsp_webserver.h"
#include "bsp_board.h"
#include "bsp_storage.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>
#include <sys/param.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BSP_WEBSERVER";

// 缓冲区大小
#define FILE_BUFFER_SIZE 4096

// HTTP服务器句柄
static httpd_handle_t s_server = NULL;

// MIME类型映射
typedef struct {
    const char *extension;
    const char *mime_type;
} mime_type_t;

// 常见MIME类型
static const mime_type_t mime_types[] = {
    {".htm", "text/html"},
    {".html", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".ico", "image/x-icon"},
    {".svg", "image/svg+xml"},
    {".txt", "text/plain"},
    {NULL, "application/octet-stream"} // 默认MIME类型
};

// 根据文件扩展名获取MIME类型
static const char* get_mime_type(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (ext) {
        for (size_t i = 0; mime_types[i].extension != NULL; i++) {
            if (strcasecmp(ext, mime_types[i].extension) == 0) {
                return mime_types[i].mime_type;
            }
        }
    }
    return mime_types[sizeof(mime_types)/sizeof(mime_types[0]) - 1].mime_type;
}

// 处理根请求，重定向到index.htm
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/index.htm");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// 处理静态文件请求
static esp_err_t static_file_handler(httpd_req_t *req) {
    char filepath[1024]; // 进一步增加缓冲区大小避免格式截断警告
    
    // 获取URI路径
    const char *uri_path = req->uri;
    if (strcmp(uri_path, "/") == 0) {
        uri_path = "/index.htm"; // 默认页面
    }
    
    // 构建完整文件路径，确保不会发生截断
    const char *adjusted_uri_path = uri_path;
    // 如果URI以/开头，则跳过这个/，避免路径重复
    if (uri_path[0] == '/') {
        adjusted_uri_path = uri_path + 1;
    }
    
    if (snprintf(filepath, sizeof(filepath), "%s/%s", WEB_FOLDER, adjusted_uri_path) >= sizeof(filepath)) {
        ESP_LOGE(TAG, "文件路径太长: %s", uri_path);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    // 检查文件是否存在
    struct stat file_stat;
    if (stat(filepath, &file_stat) != 0) {
        // 文件不存在，尝试在目录中查找相似的文件（FAT文件系统8.3格式兼容）
        ESP_LOGI(TAG, "尝试查找匹配文件: %s", filepath);
        
        char dir_path[1024];
        char file_name[256];
        char file_ext[32] = {0};
        
        // 提取目录路径和文件名
        char *last_slash = strrchr(filepath, '/');
        if (last_slash) {
            strncpy(dir_path, filepath, last_slash - filepath + 1);
            dir_path[last_slash - filepath + 1] = '\0';
            strcpy(file_name, last_slash + 1);
            
            // 提取扩展名（如果有）
            char *dot = strrchr(file_name, '.');
            if (dot) {
                strcpy(file_ext, dot);
                *dot = '\0'; // 分割文件名和扩展名
            }
            
            // 查找目录中可能匹配的文件
            ESP_LOGI(TAG, "搜索目录: %s, 文件名: %s, 扩展名: %s", dir_path, file_name, file_ext);
            DIR *dir = opendir(dir_path);
            if (dir) {
                struct dirent *entry;
                bool found = false;
                
                while ((entry = readdir(dir)) != NULL) {
                    ESP_LOGI(TAG, "检查文件: %s", entry->d_name);
                    
                    // 1. 检查完全匹配（不区分大小写）
                    if (strcasecmp(entry->d_name, file_name) == 0) {
                        found = true;
                    } else if (file_ext[0]) {
                        char temp_file_name[256];
                        strcpy(temp_file_name, file_name);
                        strcat(temp_file_name, file_ext);
                        if (strcasecmp(entry->d_name, temp_file_name) == 0) {
                            found = true;
                        }
                    }
                    
                    if (found) {
                    } else {
                        // 2. 检查8.3格式匹配 (如INDEX~1.HTM = index.htm)
                        char *tilde = strchr(entry->d_name, '~');
                        if (tilde && strncasecmp(entry->d_name, file_name, tilde - entry->d_name) == 0) {
                            // 文件前缀匹配
                            char *entry_ext = strrchr(entry->d_name, '.');
                            // 扩展名也匹配或类似(.HTM = .htm)
                            if (entry_ext && file_ext[0] && strncasecmp(entry_ext, file_ext, 2) == 0) {
                                found = true;
                            }
                        }
                    }
                    
                    if (found) {
                        // 找到匹配文件，更新filepath
                        // 检查缓冲区是否足够存储完整路径，避免截断
                        size_t dir_len = strlen(dir_path);
                        size_t entry_len = strlen(entry->d_name);
                        size_t total_len = dir_len + entry_len + 1; // +1 for null terminator
                        
                        if (total_len <= sizeof(filepath)) {
                            strncpy(filepath, dir_path, sizeof(filepath) - 1);
                            strncat(filepath, entry->d_name, sizeof(filepath) - dir_len - 1);
                            filepath[sizeof(filepath) - 1] = '\0'; // 确保字符串以NULL结尾
                            ESP_LOGI(TAG, "找到匹配文件: %s", filepath);
                            break;
                        } else {
                            ESP_LOGE(TAG, "文件路径太长，无法处理: %s...", dir_path);
                            httpd_resp_send_404(req);
                            return ESP_FAIL;
                        }
                    }
                }
                
                closedir(dir);
                
                if (!found) {
                    ESP_LOGE(TAG, "在目录中未找到匹配文件");
                    httpd_resp_send_404(req);
                    return ESP_FAIL;
                }
            } else {
                ESP_LOGE(TAG, "无法打开目录: %s", dir_path);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        } else {
            ESP_LOGE(TAG, "文件路径格式错误: %s", filepath);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        
        // 再次检查文件是否存在
        if (stat(filepath, &file_stat) != 0) {
            ESP_LOGE(TAG, "文件仍然不存在: %s", filepath);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
    }
    
    // 获取文件大小
    size_t file_size = file_stat.st_size;
    
    // 打开文件
    FILE *file = fopen(filepath, "r");
    if (!file) {
        ESP_LOGE(TAG, "无法打开文件: %s", filepath);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // 设置Content-Type
    const char *mime_type = get_mime_type(filepath);
    httpd_resp_set_type(req, mime_type);
    
    // 读取并发送文件内容
    char *buffer = malloc(FILE_BUFFER_SIZE);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        fclose(file);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, FILE_BUFFER_SIZE, file)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, bytes_read) != ESP_OK) {
            ESP_LOGE(TAG, "发送文件数据失败");
            free(buffer);
            fclose(file);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    }
    
    // 发送空块表示结束
    httpd_resp_send_chunk(req, NULL, 0);
    
    // 清理
    free(buffer);
    fclose(file);
    
    ESP_LOGI(TAG, "已发送文件: %s (%u字节)", filepath, (unsigned int)file_size);
    return ESP_OK;
}

// 网络状态API处理函数
static esp_err_t network_status_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON *targets = cJSON_CreateArray();
    
    // 添加时间戳
    cJSON_AddNumberToObject(root, "timestamp", esp_timer_get_time() / 1000000); // 秒
    
    // 获取网络监控数据
    const network_target_t *network_targets = bsp_get_network_targets();
    
    // 构建每个目标的状态信息
    for (int i = 0; i < NETWORK_TARGET_COUNT; i++) {
        cJSON *target = cJSON_CreateObject();
        
        cJSON_AddStringToObject(target, "ip", network_targets[i].ip);
        cJSON_AddStringToObject(target, "name", network_targets[i].name);
        
        // 根据状态码设置状态字符串
        const char *status_str = "UNKNOWN";
        if (network_targets[i].status == NETWORK_STATUS_UP) {
            status_str = "UP";
        } else if (network_targets[i].status == NETWORK_STATUS_DOWN) {
            status_str = "DOWN";
        }
        
        cJSON_AddStringToObject(target, "status", status_str);
        cJSON_AddNumberToObject(target, "response_time", network_targets[i].average_response_time);
        cJSON_AddNumberToObject(target, "loss_rate", network_targets[i].loss_rate);
        
        cJSON_AddItemToArray(targets, target);
    }
    
    cJSON_AddItemToObject(root, "targets", targets);
    
    // 转换为字符串
    char *json_str = cJSON_Print(root);
    
    // 设置返回类型为JSON
    httpd_resp_set_type(req, "application/json");
    
    // 发送响应
    httpd_resp_send(req, json_str, strlen(json_str));
    
    // 释放资源
    free(json_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}

// Web服务器任务函数
static void webserver_task(void *pvParameters) {
    ESP_LOGI(TAG, "Web服务器任务开始运行");
    
    // 使用存储模块挂载SD卡
    esp_err_t ret = bsp_storage_sdcard_mount(MOUNT_POINT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "文件系统挂载失败，Web服务器任务退出");
        vTaskDelete(NULL);
        return;
    }
    
    // 确保web文件夹存在
    ret = bsp_storage_create_dir_if_not_exists(WEB_FOLDER);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "创建web文件夹失败，但将继续尝试");
    }
    
    // 列出web文件夹内容
    bsp_storage_list_dir(WEB_FOLDER);
    
    // 检查index.htm文件是否存在，如果不存在则创建
    char filepath[1024];
    
    // 构建完整文件路径
    snprintf(filepath, sizeof(filepath), "%s/index.htm", WEB_FOLDER);
    
    struct stat file_stat;
    if (stat(filepath, &file_stat) != 0) {
        // 尝试查找是否存在8.3格式的文件（如INDEX~1.HTM）
        char dir_path[1024];
        snprintf(dir_path, sizeof(dir_path), "%s/", WEB_FOLDER);
        
        DIR *dir = opendir(dir_path);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strncasecmp(entry->d_name, "index", 5) == 0 && 
                    (strstr(entry->d_name, ".htm") || strstr(entry->d_name, ".HTM"))) {
                    break;
                }
            }
            closedir(dir);
        }
    }
    
    // 配置HTTP服务器
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = 8192; // 增加堆栈大小，避免堆栈溢出
    
    ESP_LOGI(TAG, "启动HTTP服务器");
    if (httpd_start(&s_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "启动HTTP服务器失败");
        bsp_storage_sdcard_unmount(MOUNT_POINT);
        vTaskDelete(NULL);
        return;
    }
    
    // 注册URI处理程序
    
    // 1. 根路径处理 - 重定向到index.htm
    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &root);
    
    // 2. 网络状态API
    httpd_uri_t network_status = {
        .uri = "/api/network",
        .method = HTTP_GET,
        .handler = network_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &network_status);
    
    // 3. 通配符路由处理静态文件
    httpd_uri_t static_files = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = static_file_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &static_files);
    
    ESP_LOGI(TAG, "HTTP服务器已启动，访问 http://10.10.99.97/ 查看网络监控页面");
    
    // 任务保持运行状态，直到系统重启或者明确调用stop_webserver函数
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // 每秒检查一次
    }
}

// 启动Web服务器
esp_err_t bsp_start_webserver(void) {
    // 在单独的任务中启动Web服务器，避免主任务堆栈溢出
    static TaskHandle_t webserver_task_handle = NULL;
    BaseType_t xReturned;
    
    if (webserver_task_handle != NULL) {
        ESP_LOGI(TAG, "Web服务器任务已经在运行");
        return ESP_OK;
    }
    
    // 创建新任务，分配足够的堆栈空间
    xReturned = xTaskCreate(
        webserver_task,           // 任务函数
        "webserver_task",         // 任务名称
        8192,                     // 堆栈大小(字节)
        NULL,                     // 任务参数
        5,                        // 优先级(越大优先级越高)
        &webserver_task_handle    // 任务句柄
    );
    
    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "无法创建Web服务器任务");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Web服务器任务已创建");
    return ESP_OK;
}

// 停止Web服务器
void bsp_stop_webserver(void) {
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "HTTP服务器已停止");
    }
    
    // 卸载文件系统
    bsp_storage_sdcard_unmount(MOUNT_POINT);
}
