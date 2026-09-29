/**
  ******************************************************************************
  * @file    bsp_init.h
  * @brief   板级外设初始化（取代原 init.h）。
  ******************************************************************************
  */

#ifndef BSP_INIT_H
#define BSP_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

/** 系统时钟与 Flash 等待周期配置。 */
void Bsp_InitRcc(void);

/** 指示灯 GPIO。 */
void Bsp_InitLed(void);

/** 按键 GPIO（上拉输入）。 */
void Bsp_InitKey(void);

/** 电流采样运放 OPA1。 */
void Bsp_InitOpa(void);

/** ADC1：三相反电动势 + 瞬时电流，由 ATIM_CC4 触发，EOS 中断。 */
void Bsp_InitAdc1(void);

/** ADC2：平均电流 / 母线电压 / 外部调速 / NTC / 内部基准，DMA 搬运。 */
void Bsp_InitAdc2(void);

/** ATIM 三相 PWM 输出（载波 20kHz）。 */
void Bsp_InitPwm(void);

/** BTIM1：1ms 系统心跳中断。 */
void Bsp_InitTick(void);

/** UART1：调试/上报串口。 */
void Bsp_InitUart(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_INIT_H */
