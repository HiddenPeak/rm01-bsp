/**
 * @file test_board_ws2812_display.h
 * @brief Board WS2812 显示功能测试程序头文件
 */

#ifndef TEST_BOARD_WS2812_DISPLAY_H
#define TEST_BOARD_WS2812_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Board WS2812显示测试主函数
 * 
 * 执行完整的Board WS2812显示功能测试，包括：
 * - 基本颜色显示
 * - 呼吸灯效果测试
 * - 预定义显示模式测试
 * - 监控数据获取测试
 * - 状态信息获取测试
 */
void test_board_ws2812_display_main(void);

/**
 * @brief 演示Board WS2812自动模式
 * 
 * 展示Board WS2812在自动模式下的工作原理和显示规则
 */
void demo_board_ws2812_auto_mode(void);

/**
 * @brief 测试Prometheus数据获取和解析
 * 
 * 专门测试从N305和Jetson获取监控数据的功能
 */
void test_board_ws2812_prometheus_data(void);

#ifdef __cplusplus
}
#endif

#endif // TEST_BOARD_WS2812_DISPLAY_H
