/**
  ******************************************************************************
  * @file    motor.c
  * @brief   三相桥驱动：六步换相与占空比更新（取代原 motor.c 的驱动部分）。
  *
  *          换相表（上桥臂 PWM 调制，下桥臂开关）：
  *            step 0 : AH 调制 / BL 导通   —— 电流 A→B
  *            step 1 : AH 调制 / CL 导通   —— 电流 A→C
  *            step 2 : BH 调制 / CL 导通   —— 电流 B→C
  *            step 3 : BH 调制 / AL 导通   —— 电流 B→A
  *            step 4 : CH 调制 / AL 导通   —— 电流 C→A
  *            step 5 : CH 调制 / BL 导通   —— 电流 C→B
  *
  * @note    本文件刻意保留了对 CW_ATIM->CCRx 的直接寄存器写：
  *          ATIM_OC1Init 等库函数带预装载/事件同步，在换相点使用会引入额外
  *          一个 PWM 周期的延迟，改变换相时序。重构不改变这一行为。
  ******************************************************************************
  */

#include "motor.h"

#include "cw32l012.h"
#include "cw32l012_atim.h"
#include "cw32l012_btim.h"
#include "cw32l012_gpio.h"

#include "app_config.h"
#include "motor_state.h"
#include "motor_types.h"

void Motor_Commutation(uint32_t step, uint32_t pwm_on)
{
    /* 关断或处于故障态：三相全部悬空，并把 CCP4 拉到"不触发 ADC"的位置 */
    if (pwm_on == 0U || g_motor_state == MOTOR_STATE_ERROR || g_motor_state == MOTOR_STATE_ERROR_OVER)
    {
        CW_ATIM->CCR1 = 0;
        CW_ATIM->CCR2 = 0;
        CW_ATIM->CCR3 = 0;
        PWM_AL_OFF();
        PWM_BL_OFF();
        PWM_CL_OFF();
        CW_ATIM->CCR4 = PWM_CCR4_OFF_VALUE;
        return;
    }

    /* --- 1) 先把本次不参与续流的下桥臂关断，避免上下桥直通 --- */
    if (step == 0U || step == 5U)      { PWM_AL_OFF(); PWM_CL_OFF(); }
    else if (step == 1U || step == 2U) { PWM_AL_OFF(); PWM_BL_OFF(); }
    else if (step == 3U || step == 4U) { PWM_BL_OFF(); PWM_CL_OFF(); }

    /* --- 2) 只有一相上桥臂做 PWM 调制，其余两相比较值清零 --- */
    if (step == 0U || step == 1U) { CW_ATIM->CCR2 = 0; CW_ATIM->CCR3 = 0; CW_ATIM->CCR1 = g_pwm_duty; }
    if (step == 2U || step == 3U) { CW_ATIM->CCR1 = 0; CW_ATIM->CCR3 = 0; CW_ATIM->CCR2 = g_pwm_duty; }
    if (step == 4U || step == 5U) { CW_ATIM->CCR1 = 0; CW_ATIM->CCR2 = 0; CW_ATIM->CCR3 = g_pwm_duty; }

    /* --- 3) 打开对应的下桥臂作为续流管 --- */
    if (step == 0U || step == 5U)      { PWM_BL_ON(); }
    else if (step == 1U || step == 2U) { PWM_CL_ON(); }
    else if (step == 3U || step == 4U) { PWM_AL_ON(); }

    g_comm_step_last = step;

    /* CCP4 提前比较，产生本次换相的 ADC 触发点（在 PWM 波谷采样） */
    CW_ATIM->CCR4 = PWM_CCR4_ON_VALUE;

    /* --- 4) 用 BTIM2 测量本步耗时，作为下一次换相 30° 延时的基准 --- */
    g_comm_step_time = BTIM_GetCounter(CW_BTIM2);
    BTIM_SetCounter(CW_BTIM2, 0);

    BTIM_SetAutoreload(CW_BTIM3, g_comm_step_time >> COMMUTATION_BTIM3_SHIFT);
    BTIM_SetCounter(CW_BTIM3, 0);
    BTIM_Cmd(CW_BTIM3, ENABLE);

    g_demag_state = DEMAG_STARTED;      /* 进入退磁阶段，退磁结束前不检测过零 */

    g_hall_pulses++;
}

void Motor_UpdateDuty(void)
{
    /* 三个比较值必须原子更新，否则可能出现过零的中间态导致桥臂直通 */
    __disable_irq();

    if (g_comm_step_last == 0U || g_comm_step_last == 1U)
    { CW_ATIM->CCR2 = 0; CW_ATIM->CCR3 = 0; CW_ATIM->CCR1 = g_pwm_duty; }
    if (g_comm_step_last == 2U || g_comm_step_last == 3U)
    { CW_ATIM->CCR1 = 0; CW_ATIM->CCR3 = 0; CW_ATIM->CCR2 = g_pwm_duty; }
    if (g_comm_step_last == 4U || g_comm_step_last == 5U)
    { CW_ATIM->CCR1 = 0; CW_ATIM->CCR2 = 0; CW_ATIM->CCR3 = g_pwm_duty; }

    __enable_irq();

    CW_ATIM->CCR4 = PWM_CCR4_ON_VALUE;
}

void Motor_StopOutput(void)
{
    g_motor_run = 0;
    Motor_Commutation(0U, 0U);
}
