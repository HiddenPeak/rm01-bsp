#ifndef BSP_WEBSERVER_H
#define BSP_WEBSERVER_H

#include "esp_err.h"

// 初始化并启动Web服务器
esp_err_t bsp_start_webserver(void);

// 停止Web服务器
void bsp_stop_webserver(void);

#endif // BSP_WEBSERVER_H
