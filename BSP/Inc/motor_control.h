/**
  ******************************************************************************
  * @file    motor_control.h
  * @brief   电机主状态机（取代原 control.c / control.h）。
  *
  *          状态迁移：
  *            START_CHECK --(给定转速)--> START_DELAY
  *            START_DELAY --(母线稳定)--> START_OPEN
  *            START_OPEN  --(无感启动)--> RUN_OPEN
  *            RUN_OPEN    --(转速归零)--> STOP --> START_CHECK
  *            任意状态    --(出现故障码)--> ERROR --> ERROR_OVER
  *            ERROR_OVER  --(按键清故障)--> WAIT_START --> START_CHECK
  *
  * @note    本模块只做"状态迁移决策"，不直接操作寄存器；
  *          实际的换相由 motor.c、过零检测由 sensorless.c 完成。
  ******************************************************************************
  */

#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

/** 待机：检测是否给定了转速，有则进入启动延时。 */
void MotorCtrl_StartCheck(void);

/** 启动延时：等待母线电压稳定，随后进入开环启动。 */
void MotorCtrl_StartDelay(void);

/** 开环启动：执行无感三段式启动，成功后进入开环运行。 */
void MotorCtrl_StartOpen(void);

/** 开环运行：按档位逐步升降速（爬坡）。 */
void MotorCtrl_RunOpen(void);

/** 停机：关断输出并等待电机停转。 */
void MotorCtrl_Stop(void);

/** 故障：立即关断输出，转入故障指示。 */
void MotorCtrl_Error(void);

/** 故障指示：用 LED 闪烁次数表示故障码，并处理自动关机。
 *  @note  正常返回意味着故障已被按键清除；否则会阻塞在等待按键处。 */
void MotorCtrl_ErrorOver(void);

/** 关机后等待按键唤醒。 */
void MotorCtrl_WaitStart(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_CONTROL_H */
