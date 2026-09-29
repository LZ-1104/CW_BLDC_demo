/**
  ******************************************************************************
  * @file    pid.c
  * @brief   速度环 PID（可选模块）。
  *
  * @note    本文件仅在 CMake 选项 ENABLE_PID=ON 时参与编译。
  *          计算过程与原 pid.c 完全一致，仅做了规范化：
  *            - 模块内的中间变量改为 static，避免污染全局命名空间；
  *            - 类型显式化（int32_t / double / float）；
  *            - 输出直接写 g_pwm_duty 并调用 Motor_UpdateDuty()，
  *              取代原来对 OutPwmValue / UPPWM() 的引用。
  *
  * @warning  原实现中 `if (SumError < 6) SumError = 6;` 把积分项下限钳位为
  *           正值 6，导致即使误差为 0 也存在恒定积分输出。该行为被原样保留
  *           以保证等价；若将来启用闭环，应确认这是否为设计意图。
  ******************************************************************************
  */

#include "pid.h"

#include "app_config.h"
#include "motor.h"
#include "motor_state.h"

/** 比例系数。 */
static float s_kp = 0.1f;

/** 积分系数。 */
static float s_ki = 0.05f;

/** 误差累积（积分项）。 */
static double s_sum_error = 0.0;

/** 本次误差。 */
static int32_t s_error = 0;

void Pid_Init(void)
{
    s_error     = 0;
    s_sum_error = 0.0;
}

void Pid_Compute(uint32_t target, uint32_t actual)
{
    float output;

    /* 保持原语义：先做无符号减法再解释为有符号误差 */
    s_error = (int32_t)(target - actual);

    s_sum_error += (double)s_error;
    if (s_sum_error < 6.0)
    {
        s_sum_error = 6.0;
    }
    else if (s_sum_error > 128000.0)
    {
        s_sum_error = 128000.0 + 50.0;
    }

    output = (float)((double)s_error * (double)s_kp + s_sum_error * (double)s_ki);

    if (output > (float)PWM_PERIOD)
    {
        g_pwm_duty = PWM_PERIOD;
    }
    else if (output < 1.0f)
    {
        g_pwm_duty = 1U;
    }
    else
    {
        g_pwm_duty = (uint32_t)output;
    }

    Motor_UpdateDuty();
}
