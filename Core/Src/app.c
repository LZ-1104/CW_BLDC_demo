/**
  ******************************************************************************
  * @file    app.c
  * @brief   应用层：初始化编排、主循环调度与 1ms 心跳中断。
  *
  *          原工程把以上内容全部塞在 main.c 里（约 130 行，含状态机 switch、
  *          按键处理、测速、保护、自动关机）。此处按职责拆分：
  *            App_Init()          上电初始化与预充电
  *            App_Run()           主循环（原 main 的 while(1)）
  *            App_HandleKey()     按键消抖与档位切换（原 BTIM1 ISR 内的逻辑）
  *            BTIM1_IRQHandler()  仅保留 1ms 计数与调度，不再内嵌业务逻辑
  *
  * @note    BTIM1_IRQHandler 的名字由 startup_cw32l012.s 的向量表固定。
  ******************************************************************************
  */

#include "app.h"

#include "cw32l012.h"
#include "cw32l012_adc.h"
#include "cw32l012_btim.h"
#include "cw32l012_gpio.h"
#include "system_cw32l012.h"

#include "app_config.h"
#include "bsp_init.h"
#include "measure.h"
#include "motor_control.h"
#include "motor_state.h"
#include "motor_types.h"
#include "sensorless.h"
#include "uart_protocol.h"

/*==============================================================================
 * 私有函数声明
 *============================================================================*/

static void App_HandleKey(void);
static void App_OnKeyPressed(void);

/*==============================================================================
 * 初始化
 *============================================================================*/

void App_Init(void)
{
    FirmwareDelay(APP_POWER_ON_DELAY);

    /* --- 外设初始化（顺序与原 main 完全一致） --- */
    Bsp_InitRcc();
    Bsp_InitLed();
    Bsp_InitKey();
    Bsp_InitOpa();
    Bsp_InitAdc1();
    Bsp_InitAdc2();
    Bsp_InitPwm();
    Bsp_InitTick();
    Sensorless_Init();

    FirmwareDelay(APP_POWER_ON_DELAY);

    /* --- 预充电：三相下桥臂同时导通数毫秒，给自举电容充电 --- */
    if (g_fault_code == FAULT_NONE)
    {
        PWM_AL_ON();
        PWM_BL_ON();
        PWM_CL_ON();

        g_tick_temp = 0U;
        while (g_tick_temp <= PRECHARGE_TIME_MS)
        {
            /* 由 BTIM1 1ms 心跳推进 */
        }

        PWM_AL_OFF();
        PWM_BL_OFF();
        PWM_CL_OFF();
    }

    /* --- 串口 --- */
    Bsp_InitUart();

    /* --- 上电默认进入"关机待唤醒"，需按键开机 --- */
    LED_OFF();
    g_power_off    = 1U;
    g_tick_standby = 1000U;

    FirmwareDelay(APP_POWER_ON_DELAY);

    /* --- 标定电流零偏并上报一次初始状态 --- */
    g_current_bias = g_adc2_raw[ADC2_IDX_AVG_CURRENT];
    Measure_VoltageCurrent();
    Uart_SendStatus();
}

/*==============================================================================
 * 主循环
 *============================================================================*/

void App_Run(void)
{
    static uint8_t s_stall_cnt = 0U;    /* 原 DZCount：连续低速次数 */

    while (1)
    {
        if (g_power_off == 0U)
        {
            /* --- 1) 串口上报：周期上报，或按键后立即上报 --- */
            if (g_tick_uart >= UART_REPORT_PERIOD_MS || g_uart_report_req == 1U)
            {
                Uart_SendStatus();
                g_tick_uart        = 0U;
                g_uart_report_req  = 0U;
            }

            /* --- 2) 每 100ms：量测 + 测速 + 堵转保护 --- */
            if (g_tick_measure >= MEASURE_PERIOD_MS)
            {
                g_tick_measure = 0U;

                Measure_VoltageCurrent();
                Measure_Temperature();

                /* 电速度：100ms 窗口内的换相脉冲数 × 100 */
                g_speed_average = g_hall_pulses_100ms * 100U;

                if (g_speed_average < STALL_SPEED_THRESHOLD &&
                    g_motor_state == MOTOR_STATE_RUN_OPEN)
                {
                    s_stall_cnt++;
                    if (s_stall_cnt >= STALL_CONFIRM_COUNT)
                    {
                        s_stall_cnt  = 0U;
                        g_fault_code = FAULT_STALL;
                    }
                }
                else
                {
                    s_stall_cnt = 0U;
                }
            }

            /* --- 3) 任一保护动作置位故障码后，统一由这里进入故障态 --- */
            if (g_fault_code != FAULT_NONE &&
                g_motor_state != MOTOR_STATE_ERROR &&
                g_motor_state != MOTOR_STATE_ERROR_OVER)
            {
                g_motor_state = MOTOR_STATE_ERROR;
            }

            /* --- 4) 状态机调度 --- */
            switch (g_motor_state)
            {
                case MOTOR_STATE_START_CHECK: MotorCtrl_StartCheck(); break;
                case MOTOR_STATE_START_DELAY: MotorCtrl_StartDelay(); break;
                case MOTOR_STATE_START_OPEN:  MotorCtrl_StartOpen();  break;
                case MOTOR_STATE_RUN_OPEN:    MotorCtrl_RunOpen();    break;
                case MOTOR_STATE_STOP:        MotorCtrl_Stop();       break;
                case MOTOR_STATE_ERROR:       MotorCtrl_Error();      break;
                case MOTOR_STATE_ERROR_OVER:  MotorCtrl_ErrorOver();  break;
                case MOTOR_STATE_WAIT_START:  MotorCtrl_WaitStart();  break;
                default:                                              break;
            }
        }

        /* --- 5) 无转速/故障持续 10s 自动关机 --- */
        if (g_tick_standby >= AUTO_POWER_OFF_MS)
        {
            LED_OFF();
            g_power_off = 1U;
            Uart_SendStatus();
            while (g_power_off == 1U)
            {
                /* 等待按键重新开机 */
            }
        }
    }
}

/*==============================================================================
 * 按键交互
 *============================================================================*/

static void App_HandleKey(void)
{
    static uint16_t s_hold      = 0U;
    static uint8_t  s_triggered = 0U;

    if (KEY_IS_PRESSED())
    {
        if (s_triggered == 0U)
        {
            s_hold++;
            if (s_hold >= KEY_PRESS_TIME_MS)
            {
                s_hold      = 0U;
                s_triggered = 1U;       /* 长按只触发一次，松开后复位 */
                App_OnKeyPressed();
            }
        }
    }
    else
    {
        s_hold      = 0U;
        s_triggered = 0U;
    }
}

static void App_OnKeyPressed(void)
{
    g_uart_report_req = 1U;

    if (g_power_off == 0U)
    {
        /* ---------------- 开机状态：清故障 / 换档 ---------------- */
        g_tick_standby = 0U;

        if (g_fault_code != FAULT_NONE)
        {
            /* 有故障：清除故障并回到 0 档，状态机自动经 ERROR_OVER→WAIT_START
               回到待机（不在此处强改 g_motor_state，与原逻辑一致） */
            g_fault_code = FAULT_NONE;
            MotorState_SetSpeedLevel(0U);
        }
        else
        {
            g_speed_level++;
            if (g_speed_level >= SPEED_LEVEL_COUNT)
            {
                g_speed_level = 0U;
            }
            MotorState_SetSpeedLevel(g_speed_level);
        }
    }
    else
    {
        /* ---------------- 关机状态：按键开机 ---------------- */
        g_power_off   = 0U;
        LED_ON();
        g_motor_state = MOTOR_STATE_START_CHECK;
        g_tick_standby = 0U;
        g_fault_code  = FAULT_NONE;
        MotorState_SetSpeedLevel(0U);
    }
}

/*==============================================================================
 * 1ms 心跳中断
 *============================================================================*/

void BTIM1_IRQHandler(void)
{
    static uint8_t s_adc_div = 0U;

    if (BTIM_GetITStatus(CW_BTIM1, BTIM_IT_UPDATE) == 0U)
    {
        return;
    }
    BTIM_ClearITPendingBit(CW_BTIM1, BTIM_IT_UPDATE);

    /* --- 按键扫描（每 1ms） --- */
    App_HandleKey();

    /* --- 每 5ms 软件触发一次 ADC2（慢速量，DMA 搬运） --- */
    s_adc_div++;
    if (s_adc_div >= 5U)
    {
        s_adc_div = 0U;
        ADC_SoftwareStartConvCmd(CW_ADC2, ENABLE);
    }

    /* 关机状态下不推进任何计时 */
    if (g_power_off != 0U)
    {
        return;
    }

    /* 0 速或故障时累计自动关机时间 */
    if (g_speed_set == 0U || g_fault_code != FAULT_NONE)
    {
        g_tick_standby++;
    }

    g_tick_uart++;      /* 串口上报周期     */
    g_tick_temp++;      /* 通用阻塞延时     */
    g_tick_startup++;   /* 启动前母线稳定   */
    g_tick_ramp++;      /* 开环爬坡节拍     */
    g_tick_measure++;   /* 量测周期         */

    /* 每 100ms 采样一次换相脉冲数，用于测速与堵转保护 */
    g_tick_speed++;
    if (g_tick_speed >= MEASURE_PERIOD_MS)
    {
        g_tick_speed        = 0U;
        g_hall_pulses_100ms = g_hall_pulses;
        g_hall_pulses       = 0U;
    }
}
