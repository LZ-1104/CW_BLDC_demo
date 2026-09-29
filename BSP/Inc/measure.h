/**
  ******************************************************************************
  * @file    measure.h
  * @brief   电压 / 电流 / 温度的采样换算与保护判定（取代原 compu.c）。
  *
  *          全部使用定点整数运算：Cortex-M0+ 无 FPU，原工程用 float 计算
  *          会引入软件浮点库调用，且结果受浮点舍入影响。
  *          改为定点后误差不超过 1 个最小单位（0.1V / 1mA），
  *          对阈值判定无影响。
  ******************************************************************************
  */

#ifndef MEASURE_H
#define MEASURE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  由 ADC2 原始值换算母线电流与母线电压，并做过流/过压/欠压判定。
 * @note   每 MEASURE_PERIOD_MS 调用一次（原 SampleVI）。
 */
void Measure_VoltageCurrent(void);

/**
 * @brief  由 NTC 采样值判定过温（原 SampleT）。
 * @note   每 MEASURE_PERIOD_MS 调用一次。
 */
void Measure_Temperature(void);

#ifdef __cplusplus
}
#endif

#endif /* MEASURE_H */
