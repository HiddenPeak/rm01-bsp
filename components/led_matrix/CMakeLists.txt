idf_component_register(
    SRCS 
        "src/led_matrix.c"
        "src/led_color.c"
        "src/led_animation.c"
        "src/led_animation_demo.c"
        "src/led_animation_export.c"
        "src/led_animation_loader.c"
        "src/led_matrix_logo_display.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        "driver"
        "led_strip"
        "rm01_esp32s3_bsp"
        "json"
)
