idf_component_register(
    SRCS "src/bsp_board.c" "src/network_monitor.c"
    INCLUDE_DIRS "include"
    REQUIRES driver sdmmc esp_adc led_strip esp_event esp_netif esp_eth espressif__ethernet_init esp_timer
)