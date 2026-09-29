/**
  ******************************************************************************
  * @file    sensorless.c
  * @brief   无感（反电动势过零）六步换相：启动流程 + 过零检测状态机。
  *
  *          —— 工作原理 ——
  *          六步换相中总有"一相悬空"，该相绕组端电压即为该相反电动势叠加
  *          母线电压中点。把悬空相的 ADC 采样值与母线电压的一半比较，即可
  *          判断是否越过中点（过零点）。
  *
  *          状态由 g_demag_state 表示：
  *            DEMAG_IDLE        空闲，不检测
  *            DEMAG_STARTED     换相刚完成，续流二极管退磁中，不检测
  *            DEMAG_DONE        退磁结束，开始检测过零
  *            ZERO_CROSS_FOUND  已捕获过零，等 BTIM3 延时 30° 后换相
  *
  *          —— 中断向量名 ——
  *          BTIM3_HALLTIM_IRQHandler / ADC1_IRQHandler 两个名字由
  *          startup_cw32l012.s 的向量表固定，不能重命名。
  ******************************************************************************
  */

#include "sensorless.h"

#include "cw32l012.h"
#include "cw32l012_adc.h"
#include "cw32l012_btim.h"
#include "cw32l012_gpio.h"
#include "cw32l012_sysctrl.h"

#include "app_config.h"
#include "motor.h"
#include "motor_state.h"
#include "motor_types.h"

/*==============================================================================
 * 私有常量表
 *============================================================================*/

/** 各换相步对应的"悬空相"在 g_adc1_raw[] 中的下标。 */
static const uint8_t s_bemf_channel[COMM_STEP_COUNT] = {
    ADC1_IDX_BEMF_C, ADC1_IDX_BEMF_B, ADC1_IDX_BEMF_A,
    ADC1_IDX_BEMF_C, ADC1_IDX_BEMF_B, ADC1_IDX_BEMF_A,
};

/** 各换相步悬空相过零的边沿方向。
 *  @note  原工程用 TAB_RFling[2][6]，但只使用了 [1] 一行，[0] 行从未被引用，
 *         故此处只保留实际生效的一行。 */
static const uint8_t s_bemf_edge[COMM_STEP_COUNT] = {
    BEMF_EDGE_FALLING, BEMF_EDGE_RISING, BEMF_EDGE_FALLING,
    BEMF_EDGE_RISING,  BEMF_EDGE_FALLING, BEMF_EDGE_RISING,
};

/*==============================================================================
 * 私有状态（只在本模块内使用）
 *============================================================================*/

/** 启动成功标志：0 = 开环中，1 = 已闭环，2 = 正在执行启动序列。 */
static uint8_t  s_start_ok = 0U;

/** 连续成功过零计数，达到 ZERO_CROSS_VALID_COUNT 判定启动成功。 */
static uint16_t s_zero_cross_cnt = 0U;

/** 本轮（当前换相步）是否已捕获过零。 */
static uint8_t  s_zero_cross_flag = 0U;

/** 过零判定所需的连续 ADC 命中次数（启动初期为 3，稳定后为 2）。 */
static uint8_t  s_adc_confirm = ZERO_CROSS_ADC_CONFIRM_RUN;

/** 本次启动使用的占空比百分数（原 QDPwm）。 */
static uint16_t s_start_duty_pct = 0U;

/** 过零比较阈值（母线电压采样值的一半）。 */
static uint16_t s_bemf_threshold = 0U;

/** 悬空相的反电动势采样值。 */
static uint16_t s_bemf_value = 0U;

/*==============================================================================
 * 私有函数声明
 *============================================================================*/

static uint8_t Sensorless_Start(void);
static void    Sensorless_ProcessBemf(void);

/*==============================================================================
 * 初始化
 *============================================================================*/

void Sensorless_Init(void)
{
    BTIM_TimeBaseInitTypeDef btim = { 0 };

    __SYSCTRL_BTIM123_CLK_ENABLE();

    btim.BTIM_Mode      = BTIM_MODE_TIMER;
    btim.BTIM_CountMode = BTIM_COUNT_MODE_REPETITIVE;
    btim.BTIM_Period    = SENSORLESS_TIM_PERIOD;
    btim.BTIM_Prescaler = SENSORLESS_TIM_PRESCALER - 1U;
    BTIM_TimeBaseInit(CW_BTIM2, &btim);     /* 换相步长测量 */
    BTIM_TimeBaseInit(CW_BTIM3, &btim);     /* 换相延时     */

    NVIC_EnableIRQ(BTIM3_HALLTIM_IRQn);

    BTIM_Cmd(CW_BTIM2, ENABLE);
    BTIM_Cmd(CW_BTIM3, DISABLE);
    BTIM_ITConfig(CW_BTIM3, BTIM_IT_UPDATE, ENABLE);
}

void Sensorless_ResetDemag(void)
{
    g_demag_state = DEMAG_IDLE;
}

/*==============================================================================
 * 中断：BTIM3 定时到，执行换相
 *============================================================================*/

void BTIM3_HALLTIM_IRQHandler(void)
{
    if (BTIM_GetITStatus(CW_BTIM3, BTIM_IT_UPDATE) == 0U)
    {
        return;
    }
    BTIM_ClearITPendingBit(CW_BTIM3, BTIM_IT_UPDATE);

    if (g_demag_state == DEMAG_STARTED)
    {
        /* 退磁时间到，可以开始检测反电动势 */
        g_demag_state = DEMAG_DONE;
        BTIM_Cmd(CW_BTIM3, DISABLE);
    }
    else if (g_demag_state == ZERO_CROSS_FOUND && s_start_ok == 1U)
    {
        /* 过零后再延时 30° 电角度即换相点 */
        BTIM_Cmd(CW_BTIM3, DISABLE);

        g_comm_step++;
        if (g_comm_step >= COMM_STEP_COUNT)
        {
            g_comm_step = 0U;
        }

        Motor_Commutation(g_comm_step, g_motor_run);
    }
}

/*==============================================================================
 * 中断：ADC1 采样完成，做一次过零判定
 *============================================================================*/

void ADC1_IRQHandler(void)
{
    if (CW_ADC1->ISR_f.EOS == 0U)
    {
        return;
    }
    ADC_ClearITPendingAll(CW_ADC1);

    /* 4 路同步采样结果按 SQR 顺序落入 g_adc1_raw[0..3] */
    ADC_GetSqr0Result(CW_ADC1, &g_adc1_raw[ADC1_IDX_BUS_CURRENT]);
    ADC_GetSqr1Result(CW_ADC1, &g_adc1_raw[ADC1_IDX_BEMF_A]);
    ADC_GetSqr2Result(CW_ADC1, &g_adc1_raw[ADC1_IDX_BEMF_B]);
    ADC_GetSqr3Result(CW_ADC1, &g_adc1_raw[ADC1_IDX_BEMF_C]);

    /* 未运转 / 有故障 / 尚未退磁完毕：只更新采样值，不做判定 */
    if (g_motor_run == 0U || g_fault_code != FAULT_NONE)
    {
        return;
    }
    if (g_demag_state != DEMAG_DONE)
    {
        return;
    }

    Sensorless_ProcessBemf();
}

/*==============================================================================
 * 过零判定（原 ADCS_chuli）
 *============================================================================*/

static void Sensorless_ProcessBemf(void)
{
    static uint8_t s_hit_cnt = 0U;      /* 连续命中计数（原 cou） */
    uint8_t is_rising;

    s_bemf_value = g_adc1_raw[s_bemf_channel[g_comm_step]];
    is_rising    = (s_bemf_edge[g_comm_step] == BEMF_EDGE_RISING) ? 1U : 0U;

    /* 悬空相端电压越过母线中点即过零 */
    s_bemf_threshold = (uint16_t)(g_adc2_raw[ADC2_IDX_BUS_VOLTAGE] >> 1);

    if ((is_rising != 0U && s_bemf_value > s_bemf_threshold) ||
        (is_rising == 0U && s_bemf_value < s_bemf_threshold))
    {
        s_hit_cnt++;
        if (s_hit_cnt >= s_adc_confirm)
        {
            s_hit_cnt = 0U;
            g_demag_state = ZERO_CROSS_FOUND;
            s_zero_cross_cnt++;
            s_zero_cross_flag = 1U;
        }
    }
    else
    {
        s_hit_cnt = 0U;      /* 需连续命中，避免毛刺引起误换相 */
    }

    /* 连续 ZERO_CROSS_VALID_COUNT 次稳定过零 → 认为已被反电动势接管 */
    if (s_zero_cross_cnt >= ZERO_CROSS_VALID_COUNT && s_start_ok == 0U)
    {
        s_start_ok = 1U;
    }

    if (s_start_ok == 1U && s_zero_cross_flag == 1U)
    {
        s_zero_cross_flag = 0U;

        if (g_comm_step_time > SENSORLESS_BTIM3_MIN_STEP)
        {
            /* 转速已可测：用测得的步长延时 30° 电角度后换相 */
            BTIM_SetAutoreload(CW_BTIM3, g_comm_step_time >> COMMUTATION_BTIM3_SHIFT);
            BTIM_SetCounter(CW_BTIM3, 0);
            BTIM_Cmd(CW_BTIM3, ENABLE);
        }
        else
        {
            /* 步长太短，定时器方法不可靠：立即换相并把确认次数放宽到 1 */
            s_adc_confirm = 1U;

            g_comm_step++;
            if (g_comm_step >= COMM_STEP_COUNT)
            {
                g_comm_step = 0U;
            }

            Motor_Commutation(g_comm_step, g_motor_run);
        }
    }
}

/*==============================================================================
 * 开环启动序列（原 sensorless.c 的 Sensorless_START）
 *============================================================================*/

static uint8_t Sensorless_Start(void)
{
    uint16_t com_time = 0U;
    uint8_t  ret;

    BTIM_SetCounter(CW_BTIM2, 0);

    s_start_ok      = 2U;               /* 标记"正在启动" */
    g_demag_state   = DEMAG_IDLE;

    /* 初始占空比 = 设定百分数 × 1% 比较值 */
    g_pwm_duty = (uint32_t)(PWM_ONE_PERCENT * s_start_duty_pct);
    g_motor_run = 1U;

    BTIM_SetCounter(CW_BTIM2, 0);
    g_comm_step = 0U;

    Motor_UpdateDuty();
    Motor_Commutation(g_comm_step, g_motor_run);

    /* 强制 B、C 两相下桥臂同时导通，给转子一个确定的初始定位 */
    PWM_BL_ON();
    PWM_CL_ON();

    /* 等待转子在定位力矩下静止 */
    g_tick_temp = 0U;
    while (g_tick_temp < SENSORLESS_FIRST_WAIT_MS)
    {
        /* 等待 BTIM1 心跳推进 g_tick_temp */
    }
    g_tick_temp = 0U;

    com_time          = 0U;
    s_zero_cross_cnt  = 0U;
    s_zero_cross_flag = 0U;
    s_adc_confirm     = ZERO_CROSS_ADC_CONFIRM_START;
    s_start_ok        = 0U;

    /* 人为预置一个较大的步长，使第一次换相有足够的观察窗口 */
    CW_BTIM2->CNT = SENSORLESS_BTIM2_START_CNT;

    g_comm_step++;
    if (g_comm_step >= COMM_STEP_COUNT)
    {
        g_comm_step = 0U;
    }

    do
    {
        if (s_zero_cross_flag == 0U)
        {
            s_zero_cross_cnt = 0U;      /* 本轮没捕获到过零，计数清零 */
        }
        s_zero_cross_flag = 0U;

        if (s_start_ok == 0U)
        {
            /* 尚未闭环：按固定节拍强制换相 */
            Motor_Commutation(g_comm_step, g_motor_run);
        }

        g_tick_temp = 0U;
        while (g_tick_temp < SENSORLESS_STEP_WAIT_MS)
        {
            if (s_zero_cross_flag == 1U || s_start_ok == 1U)
            {
                break;                  /* 已捕获过零，提前结束本步 */
            }
        }

        com_time++;
        g_pwm_duty += SENSORLESS_PWM_STEP;  /* 逐步加大占空比，加速拖入可观测转速 */
    } while (s_start_ok == 0U && com_time < SENSORLESS_MAX_STEPS && g_fault_code == FAULT_NONE);

    if (s_start_ok == 0U)
    {
        /* 尝试到底仍未闭环：关断输出，通知上层重试 */
        g_motor_run = 0U;
        Motor_Commutation(0U, 0U);
        return 0U;
    }

    s_adc_confirm  = ZERO_CROSS_ADC_CONFIRM_RUN;
    g_tick_measure = 0U;

    if (g_fault_code != FAULT_NONE)
    {
        ret = (uint8_t)g_fault_code;
        return ret;
    }
    return 1U;
}

/*==============================================================================
 * 对外的启动入口（原 motor.c 的 Sensorless_MOTOR_START）
 *============================================================================*/

void Sensorless_StartOpenLoop(void)
{
    uint8_t retry = 0U;

    if (g_motor_state == MOTOR_STATE_ERROR)
    {
        return;
    }

    /* 初始占空比随母线电压归一化：母线高时用更小的百分数 */
    s_start_duty_pct = (uint16_t)(SENSORLESS_START_DUTY_PCT * VOLTAGE_NOMINAL / g_bus_voltage_dv);

    do
    {
        if (Sensorless_Start() == 0U)
        {
            retry++;
            s_start_duty_pct += SENSORLESS_RETRY_DUTY_STEP;
        }
        else
        {
            break;
        }
    } while (retry < SENSORLESS_MAX_RETRY && g_fault_code == FAULT_NONE);

    if (retry >= SENSORLESS_MAX_RETRY && g_fault_code == FAULT_NONE)
    {
        g_fault_code  = FAULT_START_FAIL;
        g_motor_state = MOTOR_STATE_ERROR;
    }
}
