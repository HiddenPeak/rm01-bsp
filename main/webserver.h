#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_err.h"

// 初始化并启动Web服务器
esp_err_t start_webserver(void);

// 停止Web服务器
void stop_webserver(void);

#endif // WEBSERVER_H
