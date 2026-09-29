/**
  ******************************************************************************
  * @file    motor_control.c
  * @brief   电机主状态机（取代原 control.c）。
  *
  * @note    与原 control.c 的差异：
  *            - MotorStartDealy → MotorCtrl_StartDelay（修正拼写）；
  *            - 删除两处散落的 `extern unsigned char Sta;`，
  *              退磁状态统一通过 g_demag_state 访问；
  *            - MotorStop 中 `RealS1 == 0 || RealS == 0` 恒为真
  *              （RealS1 从未被赋值），故直接展开为无条件等待 500ms，
  *              行为不变（详见重构报告）；
  *            - MotorErrorOver 中 `if (times >= ErrorCode);` 是一条空语句，
  *              无任何副作用，已删除。
  ******************************************************************************
  */

#include "motor_control.h"

#include "cw32l012.h"
#include "cw32l012_btim.h"
#include "cw32l012_gpio.h"

#include "app_config.h"
#include "measure.h"
#include "motor.h"
#include "motor_state.h"
#include "motor_types.h"
#include "sensorless.h"
#include "uart_protocol.h"

void MotorCtrl_StartCheck(void)
{
    if (g_motor_state == MOTOR_STATE_ERROR)
    {
        return;
    }

    if (g_speed_set > 0U)
    {
        g_motor_state  = MOTOR_STATE_START_DELAY;
        g_speed_target = 0U;
        g_tick_startup = 0U;
    }
}

void MotorCtrl_StartDelay(void)
{
    if (g_motor_state == MOTOR_STATE_ERROR)
    {
        return;
    }

    if (g_speed_set == 0U)
    {
        /* 启动过程中转速被清零：直接停机 */
        g_motor_state = MOTOR_STATE_STOP;
    }

    if (g_tick_startup >= g_startup_delay_ms)
    {
        g_tick_startup = 0U;
        g_motor_state  = MOTOR_STATE_START_OPEN;

        /* 母线稳定后重新标定电流零偏 */
        g_current_bias = g_adc2_raw[ADC2_IDX_AVG_CURRENT];
    }
}

void MotorCtrl_StartOpen(void)
{
    if (g_motor_state == MOTOR_STATE_ERROR)
    {
        return;
    }

    /* 占空比从 0 爬到 100% 的总时间，折算为"每 1% 所需毫秒数"，下限 100ms */
    if (g_ramp_time_ms < OPEN_LOOP_RAMP_MIN_MS)
    {
        g_ramp_time_ms = OPEN_LOOP_RAMP_MIN_MS;
    }
    g_ramp_step_ms = g_ramp_time_ms / 100U;

    Sensorless_StartOpenLoop();

    g_motor_state  = MOTOR_STATE_RUN_OPEN;
    g_speed_target = g_pwm_duty / PWM_ONE_PERCENT;
}

void MotorCtrl_RunOpen(void)
{
    if (g_motor_state == MOTOR_STATE_ERROR)
    {
        return;
    }

    if (g_speed_set == 0U)
    {
        g_motor_state = MOTOR_STATE_STOP;
    }

    if (g_tick_ramp >= g_ramp_step_ms)
    {
        g_tick_ramp = 0U;

        if (g_speed_target < g_speed_set)
        {
            g_speed_target++;
            if (g_speed_target >= g_speed_set)
            {
                g_speed_target = g_speed_set;
            }
            g_pwm_duty = g_speed_target * PWM_ONE_PERCENT;
            Motor_UpdateDuty();
        }
        else if (g_speed_target > g_speed_set)
        {
            g_speed_target = g_speed_set;
            g_pwm_duty     = g_speed_target * PWM_ONE_PERCENT;
            Motor_UpdateDuty();
        }
    }
}

void MotorCtrl_Stop(void)
{
    if (g_motor_state == MOTOR_STATE_ERROR)
    {
        return;
    }

    g_motor_run = 0U;
    Motor_StopOutput();

    /* 三相反电动势都回落到悬空值 → 电机确实已停转 */
    if (g_adc1_raw[ADC1_IDX_BEMF_C] < BEMF_IDLE_ADC_THRESHOLD &&
        g_adc1_raw[ADC1_IDX_BEMF_B] < BEMF_IDLE_ADC_THRESHOLD &&
        g_adc1_raw[ADC1_IDX_BEMF_A] < BEMF_IDLE_ADC_THRESHOLD)
    {
        g_speed_instant = 0U;
        g_tick_temp     = 0U;
        Sensorless_ResetDemag();
    }

    /* 原条件 `RealS1 == 0 || RealS == 0` 恒为真（RealS1 从未被赋值），
       因此这里无条件等待 500ms 再回到待机状态，与原行为一致。 */
    g_tick_temp = 0U;
    while (g_tick_temp < 500U)
    {
        /* 等待转子完全停转 */
    }
    Sensorless_ResetDemag();
    g_motor_state = MOTOR_STATE_START_CHECK;
}

void MotorCtrl_Error(void)
{
    Motor_StopOutput();
    g_motor_run   = 0U;
    g_motor_state = MOTOR_STATE_ERROR_OVER;
}

void MotorCtrl_ErrorOver(void)
{
    uint32_t times = 0U;

    /* 用 LED 闪烁次数指示故障码：闪 N 次 → 故障码 N */
    LED_OFF();
    g_tick_temp = 0U;
    while (g_tick_temp < 200U)
    {
        /* 熄灭 200ms 作为起始间隔 */
    }

    while (1)
    {
        if (times < (uint32_t)g_fault_code)
        {
            if (g_tick_temp < 200U)
            {
                LED_ON();
            }
            else if (g_tick_temp <= 400U)
            {
                LED_OFF();
            }
            else
            {
                /* 一个闪烁周期结束 */
                times++;
                g_tick_temp = 0U;

                if (times < (uint32_t)g_fault_code)
                {
                    LED_OFF();
                }
            }
        }
        else if (g_tick_temp < 500U)
        {
            /* 组间间隔：保持熄灭 */
        }
        else
        {
            times       = 0U;
            g_tick_temp = 0U;
        }

        /* 按键清除故障后跳出，进入等待启动状态 */
        if (g_fault_code == FAULT_NONE)
        {
            LED_ON();
            g_motor_state = MOTOR_STATE_WAIT_START;
            break;
        }

        /* 长时间无操作自动关机并上报（原 closeflag 握手） */
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

void MotorCtrl_WaitStart(void)
{
    if (g_speed_set == 0U)
    {
        g_motor_state = MOTOR_STATE_START_CHECK;
    }
}
