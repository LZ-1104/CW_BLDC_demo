/**
  ******************************************************************************
  * @file    bsp_init.c
  * @brief   板级外设初始化（取代原 init.c）。
  *
  * @note    本文件只包含**外设寄存器配置**，不含任何控制逻辑。
  *          所有引脚与参数均来自 app_config.h。
  *
  *          与原 init.c 的差异（均为等价或修正未定义行为，详见重构报告）：
  *            - 删除 ADC1_Configuration 中重复的 __SYSCTRL_ADC_CLK_ENABLE()；
  *            - 删除 ADC1_Configuration 内对 OPA_Config() 的重复调用
  *              （main 已显式调用）；
  *            - USART_config 中的 GPIO_InitTypeDef 补 { 0 } 初始化
  *              （原代码未初始化 .IT 字段，属未定义行为）；
  *            - SENSORLESS_TIM_Config 移入 sensorless.c，重命名为 Sensorless_Init；
  *            - SendDataToUsart 移入 uart_protocol.c，重命名为 Uart_SendStatus。
  ******************************************************************************
  */

#include "bsp_init.h"

#include "cw32l012.h"
#include "cw32l012_sysctrl.h"
#include "cw32l012_gpio.h"
#include "cw32l012_adc.h"
#include "cw32l012_dma.h"
#include "cw32l012_atim.h"
#include "cw32l012_btim.h"
#include "cw32l012_opa.h"
#include "cw32l012_uart.h"

#include "app_config.h"
#include "motor_state.h"

/*==============================================================================
 * 系统时钟
 *============================================================================*/

void Bsp_InitRcc(void)
{
    SYSCTRL_HSI_Enable(SYSCTRL_HSIOSC_DIV1);
    SYSCTRL_HCLKPRS_Config(SYSCTRL_HCLK_DIV1);
    SYSCTRL_PCLKPRS_Config(SYSCTRL_PCLK_DIV1);
    SYSCTRL_SystemCoreClockUpdate(SYSTEM_CORE_CLOCK_HZ);
}

/*==============================================================================
 * 指示灯
 *============================================================================*/

void Bsp_InitLed(void)
{
    GPIO_InitTypeDef gpio = { 0 };

    __SYSCTRL_GPIOA_CLK_ENABLE();
    __SYSCTRL_GPIOB_CLK_ENABLE();
    __SYSCTRL_GPIOC_CLK_ENABLE();

    gpio.IT   = GPIO_IT_NONE;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pins = LED_GPIO_PIN;
    GPIO_Init(LED_GPIO_PORT, &gpio);

    LED_ON();
}

/*==============================================================================
 * 按键
 *============================================================================*/

void Bsp_InitKey(void)
{
    GPIO_InitTypeDef gpio = { 0 };

    __SYSCTRL_GPIOA_CLK_ENABLE();

    gpio.IT   = GPIO_IT_NONE;
    gpio.Mode = GPIO_MODE_INPUT_PULLUP;
    gpio.Pins = KEY_GPIO_PIN;
    GPIO_Init(KEY_GPIO_PORT, &gpio);
}

/*==============================================================================
 * 电流采样运放
 *============================================================================*/

void Bsp_InitOpa(void)
{
    OPA_InitTypeDef opa = { 0 };

    __SYSCTRL_OPA_CLK_ENABLE();
    __SYSCTRL_GPIOA_CLK_ENABLE();
    __SYSCTRL_GPIOB_CLK_ENABLE();

    AFx_OPA1INP2_PA06();        /* OPA1 正端输入  PA06 */
    AFx_OPA1INN2_PA07();        /* OPA1 负端输入  PA07 */
    AFx_OPA1OUT_PB00();         /* OPA1 输出      PB00 */

    opa.InputP   = OPA_INPUT_INP2;              /* 正端来自 INP2            */
    opa.InputN   = OPA_INPUT_INN2;              /* 负端来自 INN2            */
    opa.Bias     = OPA_BIAS_8UA_4US;            /* 偏置 8uA，自动调零 4us   */
    opa.WorkMode = OPA_WORKMODE_STANDALONE;     /* 独立运放，增益由外部电阻 */

    OPA_Init(CW_OPA1, &opa);
    OPA_Start(CW_OPA1);
}

/*==============================================================================
 * ADC1：同步采样瞬时电流 + 三相反电动势
 *============================================================================*/

void Bsp_InitAdc1(void)
{
    ADC_InitTypeDef adc = { 0 };

    __SYSCTRL_ADC_CLK_ENABLE();
    __SYSCTRL_GPIOA_CLK_ENABLE();
    __SYSCTRL_GPIOB_CLK_ENABLE();

    PB00_ANALOG_ENABLE();       /* PB0  ADC1_IN8  瞬时电流      */
    PA00_ANALOG_ENABLE();       /* PA0  ADC1_IN0  反电动势 A    */
    PA01_ANALOG_ENABLE();       /* PA1  ADC1_IN1  反电动势 B    */
    PA02_ANALOG_ENABLE();       /* PA2  ADC1_IN2  反电动势 C    */

    adc.ADC_ClkDiv      = ADC_Clk_Div2;         /* 48MHz / (15+33) ≈ 1MHz 采样 */
    adc.ADC_ConvertMode = ADC_ConvertMode_Once; /* 单次多通道采样             */
    adc.ADC_SQREns      = ADC_SqrEns0to3;

    adc.ADC_IN0.ADC_InputChannel = ADC_InputCH8;    /* 瞬时电流 */
    adc.ADC_IN0.ADC_SampTime     = ADC_SampTime70Clk;
    adc.ADC_IN1.ADC_InputChannel = ADC_InputCH0;    /* 反电动势 A */
    adc.ADC_IN1.ADC_SampTime     = ADC_SampTime70Clk;
    adc.ADC_IN2.ADC_InputChannel = ADC_InputCH1;    /* 反电动势 B */
    adc.ADC_IN2.ADC_SampTime     = ADC_SampTime70Clk;
    adc.ADC_IN3.ADC_InputChannel = ADC_InputCH2;    /* 反电动势 C */
    adc.ADC_IN3.ADC_SampTime     = ADC_SampTime70Clk;

    ADC_Init(CW_ADC1, &adc);

    ADC_ClearITPendingAll(CW_ADC1);
    ADC_ITConfig(CW_ADC1, ADC_IT_EOS, ENABLE);
    NVIC_EnableIRQ(ADC1_IRQn);
    ADC_ExtTrigCfg(CW_ADC1, ADC_TRIG_ATIMCC4, ENABLE);  /* ATIM_CC4 硬件触发 */

    ADC_Enable(CW_ADC1);
}

/*==============================================================================
 * ADC2：DMA 搬运 5 路慢速量
 *============================================================================*/

void Bsp_InitAdc2(void)
{
    ADC_InitTypeDef        adc = { 0 };
    ADC_DMA_TriggerTypeDef dma_trigger;
    DMA_InitTypeDef        dma = { 0 };

    __SYSCTRL_ADC_CLK_ENABLE();
    __SYSCTRL_DMA_CLK_ENABLE();
    __SYSCTRL_GPIOA_CLK_ENABLE();
    __SYSCTRL_GPIOB_CLK_ENABLE();

    PB02_ANALOG_ENABLE();       /* PB2   ADC2_IN11  平均电流   */
    PA08_ANALOG_ENABLE();       /* PA8   ADC2_IN5   母线电压   */
    PA10_ANALOG_ENABLE();       /* PA10  ADC2_IN7   外部调速   */
    PA11_ANALOG_ENABLE();       /* PA11  ADC2_IN8   NTC        */

    dma.DataSize       = DMA_DATA_SIZE_32BITS;
    dma.SrcIncrement   = DMA_ADDRESS_INCREMENT;
    dma.DstIncrement   = DMA_ADDRESS_INCREMENT;
    dma.TransferMode   = DMA_MODE_BLOCK;
    dma.TransferCount  = ADC2_SAMPLE_COUNT;
    dma.BlockCount     = 1;                     /* 必须为 1 */
    dma.SrcAddress     = (uint32_t)&(CW_ADC2->RESULT0);
    dma.DstAddress     = (uint32_t)&g_adc2_raw;
    dma.TriggerType    = DMA_TRIGGER_HARDWARE;
    dma.TriggerSource  = DMA_TRIGGER_SRC_ADC2_SEQ;
    dma.RestartEnable  = TRUE;

    DMA_Init(DMA_CHANNEL_2, &dma);
    DMA_Cmd(DMA_CHANNEL_2, ENABLE);

    adc.ADC_ClkDiv      = ADC_Clk_Div8;
    adc.ADC_ConvertMode = ADC_ConvertMode_Once;
    adc.ADC_SQREns      = ADC_SqrEns0to4;

    adc.ADC_IN0.ADC_InputChannel = ADC_InputCH11;       /* 平均电流 */
    adc.ADC_IN0.ADC_SampTime     = ADC_SampTime518Clk;
    adc.ADC_IN1.ADC_InputChannel = ADC_InputCH5;        /* 母线电压 */
    adc.ADC_IN1.ADC_SampTime     = ADC_SampTime518Clk;
    adc.ADC_IN2.ADC_InputChannel = ADC_InputCH7;        /* 外部调速 */
    adc.ADC_IN2.ADC_SampTime     = ADC_SampTime518Clk;
    adc.ADC_IN3.ADC_InputChannel = ADC_InputCH8;        /* NTC      */
    adc.ADC_IN3.ADC_SampTime     = ADC_SampTime518Clk;
    adc.ADC_IN4.ADC_InputChannel = ADC_InputVref1P2;    /* 内部 1.2V 基准 */
    adc.ADC_IN4.ADC_SampTime     = ADC_SampTime518Clk;

    CW_BGR->CR_f.BGREN = 1;

    ADC_Init(CW_ADC2, &adc);
    ADC_ExtTrigCfg(CW_ADC2, ADC_TRIG_ATIMCC4, ENABLE);
    ADC_SoftwareStartConvCmd(CW_ADC2, ENABLE);

    ADC_ClearITPendingAll(CW_ADC2);

    dma_trigger.dma_eoc_enable = TRUE;
    ADC_DMACmd(CW_ADC2, &dma_trigger);

    ADC_Enable(CW_ADC2);
}

/*==============================================================================
 * ATIM 三相 PWM
 *============================================================================*/

void Bsp_InitPwm(void)
{
    GPIO_InitTypeDef  gpio  = { 0 };
    ATIM_InitTypeDef  atim  = { 0 };
    ATIM_OCInitTypeDef oc   = { 0 };

    __SYSCTRL_ATIM_CLK_ENABLE();
    __SYSCTRL_GPIOA_CLK_ENABLE();
    __SYSCTRL_GPIOB_CLK_ENABLE();

    /* 上桥臂：ATIM 比较输出（复用功能） */
    gpio.IT   = GPIO_IT_NONE;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pins = PWM_AH_PIN | PWM_BH_PIN;
    GPIO_Init(PWM_AH_PORT, &gpio);
    gpio.Pins = PWM_CH_PIN;
    GPIO_Init(PWM_CH_PORT, &gpio);

    PB05_AFx_ATIMCH1();
    PB06_AFx_ATIMCH2();
    PB07_AFx_ATIMCH3();

    /* 下桥臂：普通推挽输出 */
    gpio.IT   = GPIO_IT_NONE;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pins = PWM_AL_PIN;
    GPIO_Init(PWM_AL_PORT, &gpio);
    gpio.Pins = PWM_BL_PIN | PWM_CL_PIN;
    GPIO_Init(PWM_BL_PORT, &gpio);

    PWM_AL_OFF();
    PWM_BL_OFF();
    PWM_CL_OFF();

    atim.BufferState         = ENABLE;                          /* 使能预装载   */
    atim.CounterAlignedMode  = ATIM_COUNT_ALIGN_MODE_EDGE;      /* 边沿对齐     */
    atim.CounterDirection    = ATIM_COUNTING_UP;                /* 向上计数     */
    atim.CounterOPMode       = ATIM_OP_MODE_REPETITIVE;         /* 连续运行     */
    atim.Prescaler           = 1 - 1;                           /* 计数时钟 96MHz */
    atim.ReloadValue         = PWM_PERIOD - 1;
    atim.RepetitionCounter   = 0;
    ATIM_Init(&atim);

    oc.BufferState      = ENABLE;
    oc.OCComplement     = DISABLE;
    oc.OCFastMode       = DISABLE;
    oc.OCInterruptState = DISABLE;
    oc.OCMode           = ATIM_OCMODE_PWM1;
    oc.OCPolarity       = ATIM_OCPOLARITY_NONINVERT;
    ATIM_OC1Init(&oc);
    ATIM_OC2Init(&oc);
    ATIM_OC3Init(&oc);
    ATIM_OC4Init(&oc);

    ATIM_SetCompare1(0);
    ATIM_SetCompare2(0);
    ATIM_SetCompare3(0);
    ATIM_SetCompare4(PWM_PERIOD / 2);

    ATIM_CH1Config(ENABLE);
    ATIM_CH2Config(ENABLE);
    ATIM_CH3Config(ENABLE);
    ATIM_CH4Config(ENABLE);

    ATIM_CtrlPWMOutputs(ENABLE);
    ATIM_Cmd(ENABLE);
}

/*==============================================================================
 * BTIM1：1ms 系统心跳
 *============================================================================*/

void Bsp_InitTick(void)
{
    BTIM_TimeBaseInitTypeDef btim = { 0 };

    __SYSCTRL_BTIM123_CLK_ENABLE();

    __disable_irq();
    NVIC_EnableIRQ(BTIM1_IRQn);
    __enable_irq();

    btim.BTIM_Mode      = BTIM_MODE_TIMER;
    btim.BTIM_CountMode = ATIM_OP_MODE_REPETITIVE;
    btim.BTIM_Period    = 1000 - 1;
    btim.BTIM_Prescaler = 96 - 1;       /* 96MHz / 96 = 1MHz，1000 计数 = 1ms */
    BTIM_TimeBaseInit(CW_BTIM1, &btim);

    BTIM_ITConfig(CW_BTIM1, BTIM_IT_UPDATE, ENABLE);
    BTIM_Cmd(CW_BTIM1, ENABLE);
}

/*==============================================================================
 * UART1：调试/上报串口
 *============================================================================*/

void Bsp_InitUart(void)
{
    GPIO_InitTypeDef gpio = { 0 };
    UART_InitTypeDef uart = { 0 };

    /* 先把 TXD 拉高（空闲电平），再切复用功能 */
    GPIO_WritePin(DEBUG_UART_TX_PORT, DEBUG_UART_TX_PIN, GPIO_Pin_SET);

    gpio.Pins = DEBUG_UART_TX_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_Init(DEBUG_UART_TX_PORT, &gpio);

    gpio.Pins = DEBUG_UART_RX_PIN;
    gpio.Mode = GPIO_MODE_INPUT_PULLUP;
    GPIO_Init(DEBUG_UART_RX_PORT, &gpio);

    DEBUG_UART_AF_TX();
    DEBUG_UART_AF_RX();

    uart.UART_BaudRate            = DEBUG_UART_BAUDRATE;
    uart.UART_Over                = UART_Over_16;
    uart.UART_Source              = UART_Source_PCLK;
    uart.UART_UclkFreq            = DEBUG_UART_UCLK_FREQ;
    uart.UART_StartBit            = UART_StartBit_FE;
    uart.UART_StopBits            = UART_StopBits_1;
    uart.UART_Parity              = UART_Parity_No;
    uart.UART_HardwareFlowControl = UART_HardwareFlowControl_None;
    uart.UART_Mode                = UART_Mode_Rx | UART_Mode_Tx;
    UART_Init(DEBUG_UART, &uart);
}
