/**
  ******************************************************************************
  * @file    app_config.h
  * @brief   应用集中配置：所有硬件引脚、外设参数、保护阈值、时序常量。
  *
  *          本文件是工程中唯一允许出现"硬件相关魔数"的地方。
  *          其它模块一律引用此处的具名常量，禁止再写字面量。
  *
  * @note    修改本文件即等同于修改板级行为，请配合硬件原理图核对。
  ******************************************************************************
  */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/*==============================================================================
 * 1. 系统时钟
 *============================================================================*/

/** 系统主频（Hz）。RCC 配置与 UART 波特率计算均以此为准。 */
#define SYSTEM_CORE_CLOCK_HZ        96000000U

/** 上电/复位后的阻塞延时（FirmwareDelay 单位，约 1ms/单位）。 */
#define APP_POWER_ON_DELAY          200U

/*==============================================================================
 * 2. 调试串口 (UART1)
 *============================================================================*/

#define DEBUG_UART                  CW_UART1
#define DEBUG_UART_BAUDRATE         115200U
#define DEBUG_UART_UCLK_FREQ        SYSTEM_CORE_CLOCK_HZ

#define DEBUG_UART_GPIO_CLK         SYSCTRL_AHB_PERIPH_GPIOB
#define DEBUG_UART_TX_PORT          CW_GPIOB
#define DEBUG_UART_TX_PIN           GPIO_PIN_12
#define DEBUG_UART_RX_PORT          CW_GPIOB
#define DEBUG_UART_RX_PIN           GPIO_PIN_11

#define DEBUG_UART_AF_TX()          PB12_AFx_UART1TXD()
#define DEBUG_UART_AF_RX()          PB11_AFx_UART1RXD()

/*==============================================================================
 * 3. 指示灯 (LED)
 *============================================================================*/

#define LED_GPIO_PORT               CW_GPIOC
#define LED_GPIO_PIN                GPIO_PIN_13

/* LED 低电平点亮 */
#define LED_ON()                    GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_Pin_RESET)
#define LED_OFF()                   GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_Pin_SET)

/*==============================================================================
 * 4. 按键 (KEY，低电平有效)
 *============================================================================*/

#define KEY_GPIO_PORT               CW_GPIOA
#define KEY_GPIO_PIN                GPIO_PIN_3

/** 按键按下判定（低电平有效）。 */
#define KEY_IS_PRESSED()            (GPIO_ReadPin(KEY_GPIO_PORT, KEY_GPIO_PIN) == GPIO_Pin_RESET)

/** 按住多少毫秒认为是一次有效按键。 */
#define KEY_PRESS_TIME_MS           60U

/*==============================================================================
 * 5. 三相 PWM 引脚
 *    上桥臂（AH/BH/CH）由 ATIM 比较输出驱动；下桥臂（AL/BL/CL）为普通 GPIO。
 *============================================================================*/

/* 上桥臂 */
#define PWM_AH_PORT                 CW_GPIOB
#define PWM_AH_PIN                  GPIO_PIN_5
#define PWM_BH_PORT                 CW_GPIOB
#define PWM_BH_PIN                  GPIO_PIN_6
#define PWM_CH_PORT                 CW_GPIOB
#define PWM_CH_PIN                  GPIO_PIN_7

/* 下桥臂 */
#define PWM_AL_PORT                 CW_GPIOA
#define PWM_AL_PIN                  GPIO_PIN_15
#define PWM_BL_PORT                 CW_GPIOB
#define PWM_BL_PIN                  GPIO_PIN_3
#define PWM_CL_PORT                 CW_GPIOB
#define PWM_CL_PIN                  GPIO_PIN_4

/* 上桥臂开关（高电平导通） */
#define PWM_AH_ON()                 GPIO_WritePin(PWM_AH_PORT, PWM_AH_PIN, GPIO_Pin_SET)
#define PWM_BH_ON()                 GPIO_WritePin(PWM_BH_PORT, PWM_BH_PIN, GPIO_Pin_SET)
#define PWM_CH_ON()                 GPIO_WritePin(PWM_CH_PORT, PWM_CH_PIN, GPIO_Pin_SET)
#define PWM_AH_OFF()                GPIO_WritePin(PWM_AH_PORT, PWM_AH_PIN, GPIO_Pin_RESET)
#define PWM_BH_OFF()                GPIO_WritePin(PWM_BH_PORT, PWM_BH_PIN, GPIO_Pin_RESET)
#define PWM_CH_OFF()                GPIO_WritePin(PWM_CH_PORT, PWM_CH_PIN, GPIO_Pin_RESET)

/* 下桥臂开关（高电平导通） */
#define PWM_AL_ON()                 GPIO_WritePin(PWM_AL_PORT, PWM_AL_PIN, GPIO_Pin_SET)
#define PWM_BL_ON()                 GPIO_WritePin(PWM_BL_PORT, PWM_BL_PIN, GPIO_Pin_SET)
#define PWM_CL_ON()                 GPIO_WritePin(PWM_CL_PORT, PWM_CL_PIN, GPIO_Pin_SET)
#define PWM_AL_OFF()                GPIO_WritePin(PWM_AL_PORT, PWM_AL_PIN, GPIO_Pin_RESET)
#define PWM_BL_OFF()                GPIO_WritePin(PWM_BL_PORT, PWM_BL_PIN, GPIO_Pin_RESET)
#define PWM_CL_OFF()                GPIO_WritePin(PWM_CL_PORT, PWM_CL_PIN, GPIO_Pin_RESET)

/*==============================================================================
 * 6. PWM 载波与换相时序
 *============================================================================*/

/** PWM 载波周期计数值：96MHz / 20kHz - 1 ≈ 4800。 */
#define PWM_PERIOD                  4800U

/** 输出占空比 1% 对应的比较值。 */
#define PWM_ONE_PERCENT             (PWM_PERIOD / 100U)

/** 换相时 ATIM CCR4 的关断比较值（用于触发 ADC 采样）。 */
#define PWM_CCR4_OFF_VALUE          (PWM_PERIOD - 800U)

/** 换相时 ATIM CCR4 的导通比较值。 */
#define PWM_CCR4_ON_VALUE           300U

/** 换相后 BTIM3 自动重装值的分频系数（StepTime >> 3）。 */
#define COMMUTATION_BTIM3_SHIFT     3U

/*==============================================================================
 * 6.1 电机停机判定
 *============================================================================*/

/** 三相反电动势采样值都低于此值，认为电机已停止转动。 */
#define BEMF_IDLE_ADC_THRESHOLD     180U

/*==============================================================================
 * 7. ADC 采样通道
 *    ADC1：同步采样母线电流 + 三相反电动势（ATIM_CC4 硬件触发）
 *    ADC2：DMA 搬运 5 路慢速量（平均电流 / 母线电压 / 外部调速 / NTC / 内部基准）
 *============================================================================*/

/* ADC1 结果在 g_adc1_raw[] 中的下标 */
#define ADC1_IDX_BUS_CURRENT        0U      /* CH8  PB00  瞬时电流      */
#define ADC1_IDX_BEMF_A             1U      /* CH0  PA00  反电动势 A    */
#define ADC1_IDX_BEMF_B             2U      /* CH1  PA01  反电动势 B    */
#define ADC1_IDX_BEMF_C             3U      /* CH2  PA02  反电动势 C    */
#define ADC1_SAMPLE_COUNT           6U      /* g_adc1_raw[] 数组长度     */

/* ADC2 结果在 g_adc2_raw[] 中的下标（DMA 顺序与 ADC_SQR 配置一致） */
#define ADC2_IDX_AVG_CURRENT        0U      /* CH11 PB02  平均电流      */
#define ADC2_IDX_BUS_VOLTAGE        1U      /* CH5  PA08  母线电压      */
#define ADC2_IDX_EXT_SPEED          2U      /* CH7  PA10  外部调速      */
#define ADC2_IDX_NTC                3U      /* CH8  PA11  NTC 温度      */
#define ADC2_IDX_VREF               4U      /* 内部 1.2V 基准           */
#define ADC2_SAMPLE_COUNT           5U      /* g_adc2_raw[] 数组长度     */

/*==============================================================================
 * 8. 采样换算与保护阈值
 *============================================================================*/

/** 母线电压分压电阻：Vbus = Vadc * (RV1 + RV2) / RV1。 */
#define VOLTAGE_DIV_RV1             1U
#define VOLTAGE_DIV_RV2             10U

/** 母线电压换算单位：内部以 0.1V 为单位存储。 */
#define VOLTAGE_UNIT_PER_VOLT       10U

/** 采样值 105 对应的标称母线电压（0.1V 单位），用于速度标度换算。 */
#define VOLTAGE_NOMINAL             105U

/** 采样/换算比例：speed = rpm * VOLTAGE_NOMINAL / Vbus。 */
#define SPEED_SCALE_NUM             VOLTAGE_NOMINAL

/** 母线电流采样电阻 0.01Ω，放大 10 倍。 */
#define CURRENT_SHUNT_MILLIOHM      10U
#define CURRENT_AMP_GAIN            10U

/** 连续多少次越限才认定故障（硬件瞬时过流 / 软件保护）。 */
#define FAULT_CONFIRM_COUNT         30U

/** 硬件瞬时过流比较阈值（60A * 0.001Ω * 8.2 / 5 * 4096）。 */
#define HARD_FAULT_ADC_THRESHOLD    403U

/** 过流报警值（A）。 */
#define CURRENT_FAULT_THRESHOLD_A   3U

/** 过压报警值（V）。 */
#define VOLTAGE_FAULT_OVER_V        14U

/** 欠压报警值（V）。 */
#define VOLTAGE_FAULT_UNDER_V       6U

/** NTC 采样值低于此值时视为无效（未接/短路）。 */
#define NTC_MIN_VALID_ADC           50U

/** NTC 过温判定：采样值低于此值认为温度过高。 */
#define NTC_OVERTEMP_ADC_THRESHOLD  342U

/** NTC 过温需连续确认次数。 */
#define NTC_OVERTEMP_CONFIRM_COUNT  25U

/** 母线电压采样值的低电压保护区间（CanshuV 单位为 0.1V）。 */
#define VOLTAGE_FAULT_LOW_2S        66U     /* 2S 电池：低于 6.6V 保护   */
#define VOLTAGE_FAULT_LOW_3S_HI     100U    /* 3S 电池：低于 10.0V 保护  */
#define VOLTAGE_FAULT_LOW_3S_LO     95U     /* 3S 电池：高于 9.5V 才判  */

/** 母线电流偏置（0A 时的 ADC 码值）。 */
#define BUS_CURRENT_BIAS_DEFAULT    884U

/*==============================================================================
 * 8.1 转速档位
 *     实际设定值 = 档位标称值 * VOLTAGE_NOMINAL / Vbus，以补偿母线电压波动。
 *============================================================================*/

#define SPEED_LEVEL_COUNT           6U

#define SPEED_LEVEL_RPM_0           0U
#define SPEED_LEVEL_RPM_1           20U
#define SPEED_LEVEL_RPM_2           26U
#define SPEED_LEVEL_RPM_3           32U
#define SPEED_LEVEL_RPM_4           39U
#define SPEED_LEVEL_RPM_5           45U

/*==============================================================================
 * 9. 时序常量（以 BTIM1 的 1ms 心跳为单位）
 *============================================================================*/

/** 启动前等待母线电压稳定的时间（ms）。 */
#define STARTUP_DELAY_MS            400U

/** 开环启动：占空比从 0 爬到 100% 的基础时间（ms）。 */
#define OPEN_LOOP_RAMP_TIME_MS      3000U

/** 开环加速的最短单步时间（ms），防止爬升过快。 */
#define OPEN_LOOP_RAMP_MIN_MS       100U

/** 电压/电流采样与转速计算的周期（ms）。 */
#define MEASURE_PERIOD_MS           100U

/** 串口上报周期（ms）。 */
#define UART_REPORT_PERIOD_MS       500U

/** 无转速且无故障时的自动关机时间（ms）。 */
#define AUTO_POWER_OFF_MS           10000U

/** 低速堵转判定的采样窗口数与阈值（连续低速次数 → 堵转故障）。 */
#define STALL_SPEED_THRESHOLD       30U
#define STALL_CONFIRM_COUNT         50U

/** 上电预充电时间（ms）。 */
#define PRECHARGE_TIME_MS           5U

/*==============================================================================
 * 9.1 串口上报帧格式
 *     帧结构：0x43 'C' | 0x57 'W' | 0x04 类型 | 档位 | 母线电压 | 关机标志 | 校验和
 *     校验和 = 前 6 字节之和 & 0xFF
 *============================================================================*/

#define UART_FRAME_HEAD0            0x43U
#define UART_FRAME_HEAD1            0x57U
#define UART_FRAME_TYPE             0x04U
#define UART_FRAME_PAYLOAD_LEN      6U      /* 参与校验的前导字节数 */
#define UART_FRAME_LEN              7U      /* 含校验和的整帧长度   */

/*==============================================================================
 * 10. 无感（Sensorless）启动与过零检测参数
 *============================================================================*/

/** BTIM2/BTIM3 计数周期与预分频：96MHz / 12 / 65530 ≈ 122Hz 溢出。 */
#define SENSORLESS_TIM_PERIOD       65530U
#define SENSORLESS_TIM_PRESCALER    12U

/** 过零检测连续有效次数，达到即认为启动成功。 */
#define ZERO_CROSS_VALID_COUNT      15U

/** 过零判定所需的连续 ADC 命中次数（启动初期 / 稳定后）。 */
#define ZERO_CROSS_ADC_CONFIRM_START    3U
#define ZERO_CROSS_ADC_CONFIRM_RUN      2U

/** 启动阶段 BTIM2 计数器的初值（人为制造首次换相延时）。 */
#define SENSORLESS_BTIM2_START_CNT  30000U

/** 启动阶段每次换相的最大尝试步数与占空比递增步长。 */
#define SENSORLESS_MAX_STEPS        200U
#define SENSORLESS_PWM_STEP         5U

/** 启动阶段单步等待时间（ms）与首步等待时间（ms）。 */
#define SENSORLESS_STEP_WAIT_MS     10U
#define SENSORLESS_FIRST_WAIT_MS    150U

/** 稳定运行后 BTIM3 换相延时的下限（StepTime > 此值才用定时器延时换相）。 */
#define SENSORLESS_BTIM3_MIN_STEP   2000U

/** 启动失败后重试的最大次数。 */
#define SENSORLESS_MAX_RETRY        1U

/** 开环启动初始占空比（%）与失败重试时的递增量（%）。 */
#define SENSORLESS_START_DUTY_PCT   15U
#define SENSORLESS_RETRY_DUTY_STEP  5U

/*==============================================================================
 * 11. 功能裁剪开关
 *============================================================================*/

/** 是否编译 PID 闭环模块（BSP/Src/pid.c）。默认关闭，与 Keil 目标一致。
 *  @note  由 CMake 选项 ENABLE_PID 同时控制源文件是否参与编译。 */
#define ENABLE_PID_MODULE           1

#endif /* APP_CONFIG_H */
