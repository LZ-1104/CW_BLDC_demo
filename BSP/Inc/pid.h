/**
  ******************************************************************************
  * @file    pid.h
  * @brief   速度环 PID（可选模块，由 CMake 选项 ENABLE_PID 决定是否编译）。
  *
  * @note    当前默认不参与编译，与 Keil 工程的目标保持一致。
  *          工程主线为"开环爬坡 + 无感过零换相"，PID 闭环尚未接入。
  ******************************************************************************
  */

#ifndef PID_H
#define PID_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 复位积分项与历史误差。 */
void Pid_Init(void);

/**
 * @brief  执行一次速度环计算，并把结果写入 g_pwm_duty 后刷新输出。
 * @param  target  目标转速（与原工程 SetSpeed 同量纲）。
 * @param  actual  实际转速。
 * @note   内部带有 6..128050 的积分限幅，以及 [1, PWM_PERIOD] 的输出限幅。
 */
void Pid_Compute(uint32_t target, uint32_t actual);

#ifdef __cplusplus
}
#endif

#endif /* PID_H */
