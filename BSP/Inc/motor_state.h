/**
  ******************************************************************************
  * @file    motor_state.h
  * @brief   电机控制共享运行时状态（原 global.h / global.c 中的全局变量）。
  *
  *          设计说明：
  *          这些变量被主循环、1ms 心跳中断、ADC 中断、换相中断等多个上下文
  *          共享。为了保证**中断响应时序与原来完全一致**，此处刻意保持为
  *          普通全局变量，而不用 getter/setter 封装——后者会在 ISR 热路径上
  *          引入函数调用开销，可能改变 20kHz PWM 下的采样/换相时序。
  *
  *          变量按用途分组命名（g_ 前缀），并统一使用显式位宽类型。
  *          原先的 `unsigned int` 一律映射为 `uint32_t`：在 ARM32 上二者
  *          完全等价，但显式位宽可避免移植到其它平台时静默改变行为。
  ******************************************************************************
  */

#ifndef MOTOR_STATE_H
#define MOTOR_STATE_H

#include <stdint.h>

#include "app_config.h"
#include "motor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 1. 电机状态与故障
 *============================================================================*/

/** 主状态机当前状态。 */
extern motor_state_t g_motor_state;

/** 当前故障码，FAULT_NONE 表示无故障。 */
extern motor_fault_t g_fault_code;

/** 电机启停标志：非 0 表示 PWM 输出允许。 */
extern uint32_t g_motor_run;

/*==============================================================================
 * 2. 换相
 *============================================================================*/

/** 下一个/当前换相步（0..5）。 */
extern uint32_t g_comm_step;

/** 上一次生效的换相步，用于 UPPWM 只更新对应通道。 */
extern uint32_t g_comm_step_last;

/** 换相步时间：BTIM2 记录的两次换相间隔计数。 */
extern uint32_t g_comm_step_time;

/** 退磁 / 过零检测状态（原 sensorless.c 的 Sta）。
 *  @note  由 sensorless 模块维护，motor_control 模块在停机时复位。 */
extern demag_state_t g_demag_state;

/*==============================================================================
 * 3. 采样原始值
 *============================================================================*/

/** ADC1 同步采样结果：电流 + 三相反电动势（索引见 app_config.h）。 */
extern uint16_t g_adc1_raw[ADC1_SAMPLE_COUNT];

/** ADC2 慢速采样结果（DMA 32bit 搬运，索引见 app_config.h）。
 *  @note 这里必须保持 uint32_t：DMA 配置为 DMA_DATA_SIZE_32BITS，
 *        5 个字 = 20 字节，改成 uint16_t 会被 DMA 越界写坏。 */
extern uint32_t g_adc2_raw[ADC2_SAMPLE_COUNT];

/*==============================================================================
 * 4. 转速与占空比
 *============================================================================*/

/** 用户设定的目标转速（已按母线电压归一化）。 */
extern uint32_t g_speed_set;

/** 开环爬坡的当前目标转速档。 */
extern uint32_t g_speed_target;

/** 瞬时转速（保留原 RealS1 语义，当前恒为 0）。 */
extern uint32_t g_speed_instant;

/** 平均转速（每 100ms 由霍尔脉冲数换算）。 */
extern uint32_t g_speed_average;

/** 100ms 内的霍尔/换相脉冲数。 */
extern uint32_t g_hall_pulses;

/** 上一个 100ms 窗口的脉冲数，用于计算平均转速。 */
extern uint32_t g_hall_pulses_100ms;

/** 当前 PWM 占空比比较值（0..PWM_PERIOD）。 */
extern uint32_t g_pwm_duty;

/** 开环爬坡时"每提升 1% 占空比"所需的毫秒数。 */
extern uint32_t g_ramp_step_ms;

/** 开环爬坡总时间（ms）。 */
extern uint32_t g_ramp_time_ms;

/** 无感启动初始占空比（%）。 */
extern uint32_t g_start_duty_pct;

/*==============================================================================
 * 5. 电压 / 电流
 *============================================================================*/

/** 母线电压，单位 0.1V。 */
extern uint32_t g_bus_voltage_dv;

/** 母线电流，单位 mA。 */
extern uint32_t g_bus_current_ma;

/** 母线电流零偏（0A 对应的 ADC 码值）。 */
extern uint32_t g_current_bias;

/*==============================================================================
 * 6. 可配置参数
 *============================================================================*/

/** 启动前等待母线稳定的时间（ms）。 */
extern uint32_t g_startup_delay_ms;

/** 过流报警阈值（A）。 */
extern uint32_t g_overcurrent_a;

/** 过压报警阈值（V）。 */
extern uint32_t g_overvoltage_v;

/*==============================================================================
 * 7. 1ms 心跳计数器（由 BTIM1 中断自增，主循环读取）
 *============================================================================*/

extern uint32_t g_tick_temp;        /* 通用短延时计时           */
extern uint32_t g_tick_startup;     /* 启动延时计时             */
extern uint32_t g_tick_uart;        /* 串口上报计时             */
extern uint32_t g_tick_ramp;        /* 开环爬坡计时             */
extern uint32_t g_tick_speed;       /* 转速窗口计时             */
extern uint32_t g_tick_measure;     /* 电压电流采样计时         */
extern uint32_t g_tick_standby;     /* 无操作自动关机计时       */

/*==============================================================================
 * 8. 人机交互
 *============================================================================*/

/** 当前转速档位（0 = 停机，1..SPEED_LEVEL_COUNT-1 对应各档）。 */
extern uint32_t g_speed_level;

/** 关机标志：非 0 表示处于待机（PWM 关闭、等待按键）。 */
extern uint32_t g_power_off;

/** 请求立即上报一次串口数据。 */
extern uint32_t g_uart_report_req;

/*==============================================================================
 * 9. 操作接口
 *============================================================================*/

/**
 * @brief  切换转速档位，并按当前母线电压换算 g_speed_set。
 * @param  level  档位号，0 表示停机，1..SPEED_LEVEL_COUNT-1 对应各档。
 * @note   仅在主循环 / 按键处理（1ms 心跳）中调用，不涉及 ISR 热路径。
 */
void MotorState_SetSpeedLevel(uint32_t level);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_STATE_H */
