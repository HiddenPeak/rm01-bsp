idf_component_register(
    SRCS "src/bsp_board.c" "src/bsp_power.c" "src/network_monitor.c" "src/bsp_webserver.c" "src/bsp_storage.c" "src/bsp_network.c" "src/bsp_ws2812.c" "src/bsp_state_manager.c" "src/bsp_display_controller.c" "src/bsp_status_interface.c" "src/bsp_network_adapter.c" "src/bsp_touch_ws2812_display.c"
    INCLUDE_DIRS "include"
    REQUIRES driver sdmmc esp_adc led_strip esp_event esp_netif esp_eth espressif__ethernet_init esp_timer esp_http_server esp_http_client fatfs vfs json led_matrix
)

# 添加web文件到构建目录
set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/web_files")
set(WEB_DEST_DIR "${CMAKE_BINARY_DIR}/tfcard_contents/web")

# 确保目标目录存在
file(MAKE_DIRECTORY ${WEB_DEST_DIR})

# 复制web文件到构建目录
file(GLOB WEB_FILES "${WEB_SRC_DIR}/*.*")
foreach(WEB_FILE ${WEB_FILES})
    get_filename_component(FILE_NAME ${WEB_FILE} NAME)
    configure_file(${WEB_FILE} ${WEB_DEST_DIR}/${FILE_NAME} COPYONLY)
endforeach()