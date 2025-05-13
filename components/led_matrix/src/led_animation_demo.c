#include "led_animation_demo.h"
#include "led_animation.h"
#include "esp_log.h"

static const char *TAG = "LED_ANIM_DEMO";

// 初始化示例动画（设置动画点和颜色）
void initialize_animation_demo(void) {
    ESP_LOGI(TAG, "初始化示例动画");
    
    // 从原始matrix_init函数迁移的所有点和颜色
    
    // 设置掩码点和原始颜色
    led_animation_set_point(11, 8, 151, 189, 246);
    led_animation_set_point(22, 8, 220, 156, 208);
    led_animation_set_point(23, 8, 223, 157, 205);
    led_animation_set_point(24, 8, 232, 160, 199);
    
    led_animation_set_point(9, 9, 146, 202, 247);
    led_animation_set_point(10, 9, 149, 192, 246);
    led_animation_set_point(11, 9, 153, 184, 246);
    led_animation_set_point(12, 9, 159, 172, 246);
    led_animation_set_point(13, 9, 166, 161, 245);
    led_animation_set_point(20, 9, 204, 150, 220);
    led_animation_set_point(21, 9, 211, 153, 215);
    led_animation_set_point(22, 9, 220, 157, 208);
    led_animation_set_point(23, 9, 228, 159, 201);
    led_animation_set_point(24, 9, 236, 162, 200);
    led_animation_set_point(25, 9, 240, 162, 195);
    
    led_animation_set_point(8, 10, 145, 210, 247);
    led_animation_set_point(9, 10, 147, 203, 248);
    led_animation_set_point(10, 10, 148, 198, 247);
    led_animation_set_point(11, 10, 154, 184, 246);
    led_animation_set_point(12, 10, 159, 172, 246);
    led_animation_set_point(13, 10, 166, 162, 245);
    led_animation_set_point(14, 10, 172, 154, 243);
    led_animation_set_point(15, 10, 178, 147, 243);
    led_animation_set_point(18, 10, 191, 144, 235);
    led_animation_set_point(19, 10, 196, 148, 229);
    led_animation_set_point(20, 10, 203, 150, 222);
    led_animation_set_point(21, 10, 212, 153, 214);
    led_animation_set_point(22, 10, 219, 156, 208);
    led_animation_set_point(23, 10, 226, 158, 202);
    
    led_animation_set_point(8, 11, 145, 209, 247);
    led_animation_set_point(9, 11, 146, 203, 247);
    led_animation_set_point(10, 11, 148, 194, 246);
    led_animation_set_point(11, 11, 153, 184, 246);
    led_animation_set_point(12, 11, 159, 172, 245);
    led_animation_set_point(13, 11, 165, 162, 245);
    led_animation_set_point(14, 11, 172, 154, 243);
    led_animation_set_point(15, 11, 179, 147, 243);
    led_animation_set_point(16, 11, 182, 142, 242);
    led_animation_set_point(17, 11, 186, 141, 239);
    led_animation_set_point(18, 11, 191, 144, 235);
    led_animation_set_point(19, 11, 196, 147, 228);
    led_animation_set_point(20, 11, 203, 149, 220);
    led_animation_set_point(21, 11, 212, 153, 215);
    led_animation_set_point(22, 11, 220, 157, 208);
    
    led_animation_set_point(8, 12, 145, 209, 247);
    led_animation_set_point(9, 12, 146, 202, 247);
    led_animation_set_point(10, 12, 149, 194, 246);
    led_animation_set_point(14, 12, 172, 153, 243);
    led_animation_set_point(15, 12, 178, 147, 242);
    led_animation_set_point(16, 12, 183, 142, 243);
    led_animation_set_point(17, 12, 185, 142, 240);
    led_animation_set_point(18, 12, 191, 144, 234);
    led_animation_set_point(19, 12, 197, 147, 229);
    led_animation_set_point(23, 12, 226, 158, 203);
    
    led_animation_set_point(8, 13, 146, 211, 247);
    led_animation_set_point(9, 13, 146, 204, 247);
    led_animation_set_point(10, 13, 149, 195, 247);
    led_animation_set_point(15, 13, 178, 148, 242);
    led_animation_set_point(16, 13, 184, 142, 242);
    led_animation_set_point(17, 13, 185, 142, 240);
    led_animation_set_point(18, 13, 190, 144, 235);
    led_animation_set_point(23, 13, 228, 158, 204);
    led_animation_set_point(24, 13, 235, 161, 198);
    
    led_animation_set_point(8, 14, 145, 209, 247);
    led_animation_set_point(9, 14, 147, 203, 247);
    led_animation_set_point(10, 14, 149, 194, 246);
    led_animation_set_point(15, 14, 178, 147, 243);
    led_animation_set_point(16, 14, 183, 142, 242);
    led_animation_set_point(17, 14, 186, 142, 241);
    led_animation_set_point(18, 14, 190, 144, 235);
    led_animation_set_point(23, 14, 227, 159, 203);
    led_animation_set_point(24, 14, 234, 161, 197);
    led_animation_set_point(25, 14, 241, 163, 195);
    
    led_animation_set_point(8, 15, 145, 209, 247);
    led_animation_set_point(9, 15, 147, 203, 247);
    led_animation_set_point(10, 15, 149, 194, 246);
    led_animation_set_point(15, 15, 178, 147, 243);
    led_animation_set_point(16, 15, 183, 142, 242);
    led_animation_set_point(17, 15, 186, 142, 241);
    led_animation_set_point(18, 15, 190, 144, 235);
    led_animation_set_point(23, 15, 227, 159, 203);
    led_animation_set_point(24, 15, 234, 161, 197);
    led_animation_set_point(25, 15, 241, 163, 195);
    
    led_animation_set_point(8, 16, 145, 209, 247);
    led_animation_set_point(9, 16, 147, 203, 247);
    led_animation_set_point(10, 16, 149, 194, 246);
    led_animation_set_point(15, 16, 178, 147, 243);
    led_animation_set_point(16, 16, 183, 142, 242);
    led_animation_set_point(17, 16, 186, 142, 241);
    led_animation_set_point(18, 16, 190, 144, 235);
    led_animation_set_point(23, 16, 227, 159, 203);
    led_animation_set_point(24, 16, 234, 161, 197);
    led_animation_set_point(25, 16, 241, 163, 195);
    
    led_animation_set_point(8, 17, 145, 209, 247);
    led_animation_set_point(9, 17, 147, 203, 247);
    led_animation_set_point(10, 17, 149, 194, 246);
    led_animation_set_point(15, 17, 178, 147, 243);
    led_animation_set_point(16, 17, 183, 142, 242);
    led_animation_set_point(17, 17, 186, 142, 241);
    led_animation_set_point(18, 17, 190, 144, 235);
    led_animation_set_point(23, 17, 227, 159, 203);
    led_animation_set_point(24, 17, 234, 161, 197);
    led_animation_set_point(25, 17, 241, 163, 195);
    
    led_animation_set_point(9, 18, 146, 202, 247);
    led_animation_set_point(10, 18, 149, 194, 245);
    led_animation_set_point(15, 18, 178, 147, 242);
    led_animation_set_point(16, 18, 183, 142, 242);
    led_animation_set_point(17, 18, 185, 142, 240);
    led_animation_set_point(18, 18, 191, 144, 235);
    led_animation_set_point(23, 18, 227, 159, 203);
    led_animation_set_point(24, 18, 234, 161, 197);
    led_animation_set_point(25, 18, 241, 163, 195);
    
    led_animation_set_point(10, 19, 150, 193, 247);
    led_animation_set_point(13, 19, 165, 162, 245);
    led_animation_set_point(14, 19, 172, 154, 243);
    led_animation_set_point(15, 19, 179, 147, 243);
    led_animation_set_point(16, 19, 182, 142, 242);
    led_animation_set_point(17, 19, 186, 141, 239);
    led_animation_set_point(18, 19, 191, 144, 235);
    led_animation_set_point(19, 19, 196, 147, 228);
    led_animation_set_point(20, 19, 203, 149, 220);
    led_animation_set_point(23, 19, 227, 159, 203);
    led_animation_set_point(24, 19, 234, 161, 197);
    led_animation_set_point(25, 19, 241, 163, 195);
    
    led_animation_set_point(11, 20, 153, 184, 246);
    led_animation_set_point(12, 20, 159, 172, 245);
    led_animation_set_point(13, 20, 165, 162, 245);
    led_animation_set_point(14, 20, 172, 154, 243);
    led_animation_set_point(15, 20, 179, 147, 243);
    led_animation_set_point(16, 20, 182, 142, 242);
    led_animation_set_point(17, 20, 186, 141, 239);
    led_animation_set_point(18, 20, 191, 144, 235);
    led_animation_set_point(19, 20, 196, 147, 228);
    led_animation_set_point(20, 20, 203, 149, 220);
    led_animation_set_point(21, 20, 212, 153, 215);
    led_animation_set_point(22, 20, 220, 157, 208);
    led_animation_set_point(23, 20, 227, 159, 203);
    led_animation_set_point(24, 20, 234, 161, 197);
    led_animation_set_point(25, 20, 241, 163, 195);
    
    led_animation_set_point(10, 21, 149, 194, 246);
    led_animation_set_point(11, 21, 153, 183, 246);
    led_animation_set_point(12, 21, 158, 172, 245);
    led_animation_set_point(13, 21, 165, 163, 245);
    led_animation_set_point(14, 21, 173, 155, 244);
    led_animation_set_point(15, 21, 178, 147, 242);
    led_animation_set_point(18, 21, 191, 144, 235);
    led_animation_set_point(19, 21, 196, 147, 228);
    led_animation_set_point(20, 21, 203, 149, 220);
    led_animation_set_point(21, 21, 212, 153, 215);
    led_animation_set_point(22, 21, 220, 157, 208);
    led_animation_set_point(23, 21, 227, 159, 203);
    led_animation_set_point(24, 21, 234, 161, 197);
    led_animation_set_point(25, 21, 241, 163, 195);
    
    led_animation_set_point(8, 22, 145, 209, 247);
    led_animation_set_point(9, 22, 146, 202, 247);
    led_animation_set_point(10, 22, 149, 194, 246);
    led_animation_set_point(11, 22, 153, 183, 246);
    led_animation_set_point(12, 22, 158, 172, 245);
    led_animation_set_point(13, 22, 165, 163, 245);
    led_animation_set_point(20, 22, 203, 149, 220);
    led_animation_set_point(21, 22, 212, 153, 215);
    led_animation_set_point(22, 22, 220, 157, 208);
    led_animation_set_point(23, 22, 227, 159, 203);
    led_animation_set_point(24, 22, 234, 161, 197);
    
    led_animation_set_point(9, 23, 147, 202, 247);
    led_animation_set_point(10, 23, 149, 193, 245);
    led_animation_set_point(11, 23, 153, 183, 245);
    led_animation_set_point(21, 23, 212, 153, 215);
    led_animation_set_point(22, 23, 218, 156, 209);
    led_animation_set_point(23, 23, 227, 159, 203);
    
    ESP_LOGI(TAG, "示例动画初始化完成");
}
