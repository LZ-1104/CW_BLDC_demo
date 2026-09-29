/**
  ******************************************************************************
  * @file    motor_types.h
  * @brief   电机控制相关的枚举与基本类型定义。
  *
  *          原工程用一堆 #define 与裸数字表示状态和故障码，本文件统一为
  *          enum，使编译器可做类型检查，且调试时可见符号名。
  *
  * @note    枚举的**数值与原 #define 完全一致**，不得随意改动，
  *          否则会改变电机状态机的行为。
  ******************************************************************************
  */

#ifndef MOTOR_TYPES_H
#define MOTOR_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 电机运行状态机
 *   数值对应原 global.h 中的 #define STATExxx。
 *   注意：4 号被原工程跳过（无对应状态），5、6 直接沿用。
 *============================================================================*/
typedef enum {
    MOTOR_STATE_START_CHECK = 0,    /* 上电待机，检测是否给定了转速         */
    MOTOR_STATE_START_PID   = 1,    /* 预留：PID 闭环启动（当前未使用）      */
    MOTOR_STATE_RUN_PID     = 2,    /* 预留：PID 闭环运行（当前未使用）      */
    MOTOR_STATE_STOP        = 3,    /* 停机                                 */
    MOTOR_STATE_ERROR       = 5,    /* 故障，等待故障处理                    */
    MOTOR_STATE_ERROR_OVER  = 6,    /* 故障后闪烁指示，等待复位              */
    MOTOR_STATE_WAIT_START  = 7,    /* 关机后等待按键唤醒                    */
    MOTOR_STATE_START_DELAY = 8,    /* 启动延时，等待母线电压稳定            */
    MOTOR_STATE_START_OPEN  = 9,    /* 开环启动（无感三段式）                */
    MOTOR_STATE_RUN_OPEN    = 10,   /* 开环运行，按档位逐步升降速            */
} motor_state_t;

/*==============================================================================
 * 故障码
 *   数值对应原工程 ErrorCode 的字面量。
 *============================================================================*/
typedef enum {
    FAULT_NONE          = 0,    /* 无故障                               */
    FAULT_START_FAIL    = 3,    /* 无感启动失败                         */
    FAULT_OVER_CURRENT  = 4,    /* 母线过流                             */
    FAULT_OVER_VOLTAGE  = 6,    /* 母线过压                             */
    FAULT_STALL         = 7,    /* 堵转                                 */
    FAULT_UNDER_VOLTAGE = 8,    /* 母线欠压                             */
    FAULT_OVER_TEMP     = 9,    /* NTC 过温                             */
} motor_fault_t;

/*==============================================================================
 * 反电动势过零边沿
 *   数值对应原 sensorless.h 的 #define RISING / FALLING。
 *============================================================================*/
typedef enum {
    BEMF_EDGE_RISING  = 1,      /* 上升沿过零（由低变高）                */
    BEMF_EDGE_FALLING = 2,      /* 下降沿过零（由高变低）                */
} bemf_edge_t;

/*==============================================================================
 * 换相步（六步换相 Table）
 *============================================================================*/
typedef enum {
    COMM_STEP_0 = 0,
    COMM_STEP_1 = 1,
    COMM_STEP_2 = 2,
    COMM_STEP_3 = 3,
    COMM_STEP_4 = 4,
    COMM_STEP_5 = 5,
    COMM_STEP_COUNT = 6,
} comm_step_t;

/*==============================================================================
 * 退磁 / 过零检测状态（原 sensorless.c 中的 Sta 变量）
 *============================================================================*/
typedef enum {
    DEMAG_IDLE        = 0,      /* 空闲                                 */
    DEMAG_STARTED     = 1,      /* 换相后开始退磁                       */
    DEMAG_DONE        = 2,      /* 退磁结束，可以检测反电动势           */
    ZERO_CROSS_FOUND  = 3,      /* 检测到过零点，等待定时换相           */
} demag_state_t;

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_TYPES_H */
