/**
  ******************************************************************************
  * @file    app.h
  * @brief   应用层：初始化编排、主循环调度与 1ms 心跳中断。
  *
  *          分层关系（自下而上）：
  *            Drivers/   芯片外设驱动（CW32 官方库）
  *            BSP/       板级外设初始化 (bsp_init)、电机驱动 (motor)、
  *                       无感换相 (sensorless)、量测 (measure)、
  *                       上报 (uart_protocol)、共享状态 (motor_state)
  *            Core/app.c 应用编排：状态机调度 + 按键交互 + 自动关机
  *            Core/main.c 仅调用 App_Init() / App_Run()
  ******************************************************************************
  */

#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  上电初始化：外设、预充电、串口，并进入"关机待唤醒"状态。
 */
void App_Init(void);

/**
 * @brief  主循环：周期上报、量测与保护、状态机调度、自动关机。
 * @note   正常路径不会返回。
 */
void App_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
