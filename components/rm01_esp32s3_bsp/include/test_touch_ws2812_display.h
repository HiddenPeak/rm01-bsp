/**
 * @file test_touch_ws2812_display.h
 * @brief Touch WS2812 显示功能测试程序头文件
 */

#ifndef TEST_TOUCH_WS2812_DISPLAY_H
#define TEST_TOUCH_WS2812_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Touch WS2812显示测试主函数
 * 
 * 执行完整的Touch WS2812显示功能测试，包括：
 * - 基本颜色显示
 * - 闪烁效果测试
 * - 呼吸灯效果测试
 * - 预定义显示模式测试
 * - 亮度调节测试
 * - 状态信息获取测试
 */
void test_touch_ws2812_display_main(void);

/**
 * @brief 演示Touch WS2812自动模式
 * 
 * 展示Touch WS2812在自动模式下的工作原理和显示规则
 */
void demo_touch_ws2812_auto_mode(void);

#ifdef __cplusplus
}
#endif

#endif // TEST_TOUCH_WS2812_DISPLAY_H
