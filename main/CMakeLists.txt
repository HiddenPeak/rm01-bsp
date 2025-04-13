idf_component_register(SRCS "hello_world_main.c"
                    PRIV_REQUIRES spi_flash rm01_esp32s3_bsp esp_event esp_netif esp_eth lwip
                    INCLUDE_DIRS ".")
