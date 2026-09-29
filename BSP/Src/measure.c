/**
  ******************************************************************************
  * @file    measure.c
  * @brief   电压 / 电流 / 温度的采样换算与保护判定（取代原 compu.c）。
  *
  *          换算链路（与原 compu.c 一致，仅由 float 改为定点整数）：
  *
  *            毫伏 mv = BGR * adc_code / Vref_code
  *              · BGR       : 芯片出厂写入 Flash 的带隙基准校准值
  *              · Vref_code : ADC2 内部 1.2V 基准通道的采样码
  *
  *            电流 mA = mv * 10              （0.01Ω 取样 + 10 倍放大）
  *            电压 0.1V = mv * (RV1+RV2) / RV1 / 100
  ******************************************************************************
  */

#include "measure.h"

#include "cw32l012.h"
#include "cw32l012_adc.h"

#include "app_config.h"
#include "motor_state.h"
#include "motor_types.h"

/** Flash 中保存的带隙基准校准值（芯片出厂标定，只读）。 */
#define BGR_CAL_VALUE   (*(const uint16_t *)(ADC_BGR_VOL_ADDRESS))

/**
 * @brief  把 ADC 采样码换算为毫伏。
 * @param  adc_code  ADC 原始码值（12bit）。
 * @return 对应的毫伏值。
 */
static uint32_t AdcCodeToMillivolt(uint32_t adc_code)
{
    uint32_t vref_code = g_adc2_raw[ADC2_IDX_VREF];

    if (vref_code == 0U)
    {
        return 0U;              /* 基准通道尚未采到有效值，避免除零 */
    }

    return (uint32_t)BGR_CAL_VALUE * adc_code / vref_code;
}

void Measure_VoltageCurrent(void)
{
    /* 连续越限计数：只有连续 FAULT_CONFIRM_COUNT 次都越限才报故障，滤除尖峰 */
    static uint8_t s_over_current_cnt = 0U;
    static uint8_t s_over_voltage_cnt = 0U;
    static uint8_t s_under_voltage_cnt = 0U;

    /*--------------------------------------------------------------------------
     * 1. 母线电流（单位 mA）
     *------------------------------------------------------------------------*/
    uint32_t adc = g_adc2_raw[ADC2_IDX_AVG_CURRENT];

    if (adc <= g_current_bias)
    {
        g_bus_current_ma = 0U;
    }
    else
    {
        /* 去掉零偏后换算：mV → mA（0.01Ω 取样电阻、放大 10 倍） */
        g_bus_current_ma = AdcCodeToMillivolt(adc - g_current_bias) * 10U;
    }

    if (g_bus_current_ma >= g_overcurrent_a * 1000U && g_fault_code == FAULT_NONE)
    {
        s_over_current_cnt++;
        if (s_over_current_cnt >= FAULT_CONFIRM_COUNT)
        {
            g_fault_code = FAULT_OVER_CURRENT;
            s_over_current_cnt = 0U;
        }
    }
    else
    {
        s_over_current_cnt = 0U;
    }

    /*--------------------------------------------------------------------------
     * 2. 母线电压（单位 0.1V）
     *------------------------------------------------------------------------*/
    {
        uint32_t mv = AdcCodeToMillivolt(g_adc2_raw[ADC2_IDX_BUS_VOLTAGE]);

        /* 分压还原后把 mV 换算为 0.1V 单位：/1000mV * 10 (0.1V/V) == /100 */
        g_bus_voltage_dv = mv * (VOLTAGE_DIV_RV1 + VOLTAGE_DIV_RV2)
                              / VOLTAGE_DIV_RV1
                              / (1000U / VOLTAGE_UNIT_PER_VOLT);
    }

    if (g_bus_voltage_dv >= g_overvoltage_v * VOLTAGE_UNIT_PER_VOLT && g_fault_code == FAULT_NONE)
    {
        s_over_voltage_cnt++;
        if (s_over_voltage_cnt >= FAULT_CONFIRM_COUNT)
        {
            g_fault_code = FAULT_OVER_VOLTAGE;
        }
    }
    else
    {
        s_over_voltage_cnt = 0U;
    }

    /*--------------------------------------------------------------------------
     * 3. 欠压保护
     *    原工程同时兼容 2S 与 3S 电池：低于 2S 下限，或落在 3S 下限区间。
     *------------------------------------------------------------------------*/
    if (g_bus_voltage_dv < VOLTAGE_FAULT_LOW_2S && g_fault_code == FAULT_NONE)
    {
        s_under_voltage_cnt++;
        if (s_under_voltage_cnt >= FAULT_CONFIRM_COUNT)
        {
            g_fault_code = FAULT_UNDER_VOLTAGE;
        }
    }
    else if (g_bus_voltage_dv < VOLTAGE_FAULT_LOW_3S_HI &&
             g_bus_voltage_dv > VOLTAGE_FAULT_LOW_3S_LO &&
             g_fault_code == FAULT_NONE)
    {
        s_under_voltage_cnt++;
        if (s_under_voltage_cnt >= FAULT_CONFIRM_COUNT)
        {
            g_fault_code = FAULT_UNDER_VOLTAGE;
        }
    }
    else
    {
        s_under_voltage_cnt = 0U;
    }
}

void Measure_Temperature(void)
{
    static uint8_t s_over_temp_cnt = 0U;
    uint32_t ntc;

    if (g_fault_code != FAULT_NONE)
    {
        return;
    }

    ntc = g_adc2_raw[ADC2_IDX_NTC];

    /* 采样值过低说明 NTC 未接或短路，忽略 */
    if (ntc < NTC_MIN_VALID_ADC)
    {
        return;
    }

    /* 90℃ 对应码值 342，80℃ 对应 455（10K / B=3950） */
    if (ntc <= NTC_OVERTEMP_ADC_THRESHOLD)
    {
        s_over_temp_cnt++;
        if (s_over_temp_cnt >= NTC_OVERTEMP_CONFIRM_COUNT)
        {
            g_fault_code = FAULT_OVER_TEMP;
        }
    }
    else
    {
        s_over_temp_cnt = 0U;
    }
}
