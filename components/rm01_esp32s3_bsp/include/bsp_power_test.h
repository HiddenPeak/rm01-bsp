/**
 * @file bsp_power_test.h
 * @brief BSP电源芯片UART协商通信测试头文件
 */

#ifndef BSP_POWER_TEST_H
#define BSP_POWER_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动BSP电源芯片测试
 * 调用此函数来启动基于电压变化触发的电源芯片协商测试
 */
void bsp_power_test_start(void);

/**
 * @brief 显示BSP电源系统状态
 * 综合显示主电源、辅助电源和电源芯片的状态信息
 */
void bsp_power_system_status_show(void);

#ifdef __cplusplus
}
#endif

#endif // BSP_POWER_TEST_H
