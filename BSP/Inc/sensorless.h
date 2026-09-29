/**
  ******************************************************************************
  * @file    sensorless.h
  * @brief   无感（反电动势过零）六步换相：启动与过零检测。
  ******************************************************************************
  */

#ifndef SENSORLESS_H
#define SENSORLESS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化无感检测所需的 BTIM2（步长测量）与 BTIM3（换相延时）。
 * @note   原 SENSORLESS_TIM_Config，从 init.c 移入本模块以保持内聚。
 */
void Sensorless_Init(void);

/**
 * @brief  执行一次完整的无感开环启动，失败则上报启动失败故障。
 * @note   原 motor.c 中的 Sensorless_MOTOR_START。内部会阻塞等待电机
 *         被拖入可观测反电动势的状态，因此只允许在主循环中调用。
 */
void Sensorless_StartOpenLoop(void);

/**
 * @brief  复位退磁/过零检测状态（停机时调用）。
 */
void Sensorless_ResetDemag(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSORLESS_H */
