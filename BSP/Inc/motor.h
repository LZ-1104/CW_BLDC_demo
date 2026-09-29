/**
  ******************************************************************************
  * @file    motor.h
  * @brief   三相桥驱动：六步换相与占空比更新（取代原 motor.h 的公共接口）。
  *
  *          —— 职责划分 ——
  *            motor.c      : 只负责"把某一相导通/关断、占空比写多少"，
  *                           不含任何状态机与保护逻辑；
  *            motor_control: 决定何时换相、何时启停；
  *            sensorless   : 决定何时应该通知换相（过零检测）。
  ******************************************************************************
  */

#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  执行一次换相：按 step 选择导通相，并按 pwm_on 决定是否输出。
 * @param  step    换相步 0..5。
 * @param  pwm_on  非 0 = 输出 PWM；0 = 关断全部输出（滑行）。
 * @note   调用后进入退磁阶段（g_demag_state = DEMAG_STARTED），并由 BTIM3
 *         计时。本函数可在主循环与 BTIM3 中断中调用。
 */
void Motor_Commutation(uint32_t step, uint32_t pwm_on);

/**
 * @brief  仅刷新当前换相步对应通道的占空比（不改变导通相）。
 * @note   更新前关全局中断，避免与 ADC/BTIM3 中断互相打断导致桥臂直通。
 */
void Motor_UpdateDuty(void);

/**
 * @brief  停止 PWM 输出并复位 g_motor_run（原 MOTOR_STOP0）。
 */
void Motor_StopOutput(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_H */
