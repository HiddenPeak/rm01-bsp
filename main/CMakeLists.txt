idf_component_register(SRCS "main.c"
                    PRIV_REQUIRES spi_flash rm01_esp32s3_bsp esp_event esp_netif esp_eth lwip led_matrix sdmmc json
                    INCLUDE_DIRS ".")
