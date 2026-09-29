/**
  ******************************************************************************
  * @file    motor_state.c
  * @brief   电机控制共享运行时状态的定义与操作。
  *
  *          取代原 global.c。所有变量初值与原工程保持一致。
  ******************************************************************************
  */

#include "motor_state.h"

#include "app_config.h"
#include "motor_types.h"

/*==============================================================================
 * 1. 电机状态与故障
 *============================================================================*/

motor_state_t g_motor_state = MOTOR_STATE_START_CHECK;
motor_fault_t g_fault_code  = FAULT_NONE;
uint32_t      g_motor_run   = 0;

/*==============================================================================
 * 2. 换相
 *============================================================================*/

uint32_t g_comm_step       = 0;    /* 原 bHallStartStep1，BSS 零初始化 */
uint32_t g_comm_step_last  = 0;
uint32_t g_comm_step_time  = 0;

demag_state_t g_demag_state = DEMAG_IDLE;   /* 原 sensorless.c 的 Sta */

/*==============================================================================
 * 3. 采样原始值
 *============================================================================*/

uint16_t g_adc1_raw[ADC1_SAMPLE_COUNT];
uint32_t g_adc2_raw[ADC2_SAMPLE_COUNT];

/*==============================================================================
 * 4. 转速与占空比
 *============================================================================*/

uint32_t g_speed_set         = 0;
uint32_t g_speed_target      = 150;   /* 原 TargS1 初值，仅作爬坡起点 */
uint32_t g_speed_instant     = 0;
uint32_t g_speed_average     = 0;
uint32_t g_hall_pulses       = 0;
uint32_t g_hall_pulses_100ms = 0;     /* 原 main.c 中的 hte */
uint32_t g_pwm_duty          = 0;
uint32_t g_ramp_step_ms      = 0;
uint32_t g_ramp_time_ms      = OPEN_LOOP_RAMP_TIME_MS;
uint32_t g_start_duty_pct    = 0;

/*==============================================================================
 * 5. 电压 / 电流
 *============================================================================*/

uint32_t g_bus_voltage_dv = 0;
uint32_t g_bus_current_ma = 0;
uint32_t g_current_bias   = BUS_CURRENT_BIAS_DEFAULT;

/*==============================================================================
 * 6. 可配置参数
 *============================================================================*/

uint32_t g_startup_delay_ms = STARTUP_DELAY_MS;
uint32_t g_overcurrent_a    = CURRENT_FAULT_THRESHOLD_A;
uint32_t g_overvoltage_v    = VOLTAGE_FAULT_OVER_V;

/*==============================================================================
 * 7. 1ms 心跳计数器
 *============================================================================*/

uint32_t g_tick_temp     = 0;
uint32_t g_tick_startup  = 0;
uint32_t g_tick_uart     = 0;
uint32_t g_tick_ramp     = 0;
uint32_t g_tick_speed    = 0;
uint32_t g_tick_measure  = 0;
uint32_t g_tick_standby  = 0;

/*==============================================================================
 * 8. 人机交互
 *============================================================================*/

uint32_t g_speed_level     = 0;
uint32_t g_power_off       = 0;
uint32_t g_uart_report_req = 1;   /* 原 SendFlag 初值：上电立即上报一次 */

/*==============================================================================
 * 9. 转速档位
 *     实际设定转速 = 档位标称值 * VOLTAGE_NOMINAL / 母线电压，
 *     以补偿母线电压波动对转速的影响。
 *============================================================================*/

static const uint32_t s_speed_level_rpm[SPEED_LEVEL_COUNT] = {
    SPEED_LEVEL_RPM_0,
    SPEED_LEVEL_RPM_1,
    SPEED_LEVEL_RPM_2,
    SPEED_LEVEL_RPM_3,
    SPEED_LEVEL_RPM_4,
    SPEED_LEVEL_RPM_5,
};

void MotorState_SetSpeedLevel(uint32_t level)
{
    g_speed_level = level;

    /* 停机档不做除法：原工程同样在 level==0 时直接置 0，避免母线电压为 0 时除零 */
    if (level == 0 || level >= SPEED_LEVEL_COUNT) {
        g_speed_set = 0;
    } else {
        g_speed_set = s_speed_level_rpm[level] * SPEED_SCALE_NUM / g_bus_voltage_dv;
    }
}
