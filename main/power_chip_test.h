/**
 * @file power_chip_test.h
 * @brief 电源芯片UART通信测试头文件
 */

#ifndef POWER_CHIP_TEST_H
#define POWER_CHIP_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动电源芯片测试
 * 调用此函数来启动电源芯片数据读取测试
 */
void start_power_chip_test(void);

/**
 * @brief 显示电源系统状态
 * 综合显示主电源、辅助电源和电源芯片的状态信息
 */
void show_power_system_status(void);

#ifdef __cplusplus
}
#endif

#endif // POWER_CHIP_TEST_H
