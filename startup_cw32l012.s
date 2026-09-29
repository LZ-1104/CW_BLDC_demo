/**
  ******************************************************************************
  * @file      startup_cw32l012.s
  * @brief     CW32L012 (ARM Cortex-M0+) startup file for GNU as
  *            (arm-none-eabi-gcc)
  *
  *            由 IDEsupport/MDK/startup_cw32l012.s (Keil ARMCLANG) 移植而来，
  *            保留相同的向量表和中断处理函数名称。
  ******************************************************************************
  */

  .syntax   unified
  .cpu      cortex-m0plus
  .fpu      softvfp
  .thumb

  .global   g_pfnVectors
  .global   Default_Handler

/* ---------------------------------------------------------------------------
 * 复位处理：初始化 C 运行环境后跳转到 main()
 *
 * _sidata / _sdata / _edata / _sbss / _ebss 由链接脚本提供。
 * ------------------------------------------------------------------------- */
  .section  .text.Reset_Handler
  .weak     Reset_Handler
  .type     Reset_Handler, %function
Reset_Handler:
  ldr       r0, =_estack
  mov       sp, r0

  /* 把 .data 段的初值从 FLASH 拷贝到 SRAM */
  ldr       r0, =_sdata
  ldr       r1, =_edata
  ldr       r2, =_sidata
  movs      r3, #0
  b         LoopCopyDataInit

CopyDataInit:
  ldr       r4, [r2, r3]
  str       r4, [r0, r3]
  adds      r3, r3, #4

LoopCopyDataInit:
  adds      r4, r0, r3
  cmp       r4, r1
  bcc       CopyDataInit

  /* 清零 .bss 段 */
  ldr       r2, =_sbss
  ldr       r4, =_ebss
  movs      r3, #0
  b         LoopFillZerobss

FillZerobss:
  str       r3, [r2]
  adds      r2, r2, #4

LoopFillZerobss:
  cmp       r2, r4
  bcc       FillZerobss

  /* 系统初始化（时钟 / 内部 RC 修调值等） */
  bl        SystemInit
  /* C 库与静态构造函数 */
  bl        __libc_init_array
  /* 应用程序入口 */
  bl        main
  bx        lr

  .size     Reset_Handler, .-Reset_Handler

/* ---------------------------------------------------------------------------
 * 中断向量表
 * ------------------------------------------------------------------------- */
  .section  .isr_vector,"a",%progbits
  .type     g_pfnVectors, %object

g_pfnVectors:
  .word  _estack                    /* 0x00  Cortex-M0+ 初始栈顶           */
  .word  Reset_Handler              /* 0x04  Reset                          */
  .word  NMI_Handler                /* 0x08  NMI                            */
  .word  HardFault_Handler          /* 0x0C  Hard Fault                     */
  .word  0                          /* 0x10  Reserved                       */
  .word  0                          /* 0x14  Reserved                       */
  .word  0                          /* 0x18  Reserved                       */
  .word  0                          /* 0x1C  Reserved                       */
  .word  0                          /* 0x20  Reserved                       */
  .word  0                          /* 0x24  Reserved                       */
  .word  0                          /* 0x28  Reserved                       */
  .word  SVC_Handler                /* 0x2C  SVCall                         */
  .word  0                          /* 0x30  Reserved                       */
  .word  0                          /* 0x34  Reserved                       */
  .word  PendSV_Handler             /* 0x38  PendSV                         */
  .word  SysTick_Handler            /* 0x3C  SysTick                        */

  /* ---- 外部中断 ---- */
  .word  WDT_IRQHandler             /*  0  Watch Dog Timer                  */
  .word  LVD_IRQHandler             /*  1  Low Voltage Detect               */
  .word  RTC_IRQHandler             /*  2  RTC                              */
  .word  FLASHRAM_IRQHandler        /*  3  Flash / RAM                      */
  .word  SYSCTRL_IRQHandler         /*  4  SysCtrl                          */
  .word  GPIOA_IRQHandler           /*  5  GPIOA                            */
  .word  GPIOB_IRQHandler           /*  6  GPIOB                            */
  .word  GPIOC_IRQHandler           /*  7  GPIOC                            */
  .word  GPIOF_IRQHandler           /*  8  GPIOF                            */
  .word  DMACH12_IRQHandler         /*  9  DMA CH1 / CH2                    */
  .word  DMACH34_IRQHandler         /* 10  DMA CH3 / CH4                    */
  .word  CORDIC_IRQHandler          /* 11  CORDIC                           */
  .word  ADC1_IRQHandler            /* 12  ADC1                             */
  .word  ATIM_IRQHandler            /* 13  Advanced Timer                   */
  .word  VC13_IRQHandler            /* 14  Voltage Comparator 1 / 3         */
  .word  VC24_IRQHandler            /* 15  Voltage Comparator 2 / 4         */
  .word  GTIM1_IRQHandler           /* 16  General Timer1                   */
  .word  GTIM2_IRQHandler           /* 17  General Timer2                   */
  .word  GTIM34_IRQHandler          /* 18  General Timer3 / General Timer4  */
  .word  LPTIM_IRQHandler           /* 19  LPTIM                            */
  .word  BTIM1_IRQHandler           /* 20  BTIM1                            */
  .word  BTIM2_IRQHandler           /* 21  BTIM2                            */
  .word  BTIM3_HALLTIM_IRQHandler   /* 22  BTIM3 / Hall timer               */
  .word  I2C1_IRQHandler            /* 23  I2C1                             */
  .word  I2C2_IRQHandler            /* 24  I2C2                             */
  .word  SPI1_IRQHandler            /* 25  SPI1                             */
  .word  SPI23_IRQHandler           /* 26  SPI2 / SPI3                      */
  .word  UART1_IRQHandler           /* 27  UART1                            */
  .word  UART2_IRQHandler           /* 28  UART2                            */
  .word  UART3_IRQHandler           /* 29  UART3                            */
  .word  ADC2_DAC_IRQHandler        /* 30  ADC2 and DAC                     */
  .word  CLKFAULT_IRQHandler        /* 31  Clock Fault                      */

  .size  g_pfnVectors, .-g_pfnVectors

/* ---------------------------------------------------------------------------
 * 默认处理函数（弱符号，可在用户代码中重定义）
 * ------------------------------------------------------------------------- */
  .section  .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b  Infinite_Loop
  .size  Default_Handler, .-Default_Handler

/* ---- 内核异常 ---- */
  .weak      NMI_Handler
  .thumb_set NMI_Handler,Default_Handler

  .weak      HardFault_Handler
  .thumb_set HardFault_Handler,Default_Handler

  .weak      SVC_Handler
  .thumb_set SVC_Handler,Default_Handler

  .weak      PendSV_Handler
  .thumb_set PendSV_Handler,Default_Handler

  .weak      SysTick_Handler
  .thumb_set SysTick_Handler,Default_Handler

/* ---- 外部中断 ---- */
  .weak      WDT_IRQHandler
  .thumb_set WDT_IRQHandler,Default_Handler

  .weak      LVD_IRQHandler
  .thumb_set LVD_IRQHandler,Default_Handler

  .weak      RTC_IRQHandler
  .thumb_set RTC_IRQHandler,Default_Handler

  .weak      FLASHRAM_IRQHandler
  .thumb_set FLASHRAM_IRQHandler,Default_Handler

  .weak      SYSCTRL_IRQHandler
  .thumb_set SYSCTRL_IRQHandler,Default_Handler

  .weak      GPIOA_IRQHandler
  .thumb_set GPIOA_IRQHandler,Default_Handler

  .weak      GPIOB_IRQHandler
  .thumb_set GPIOB_IRQHandler,Default_Handler

  .weak      GPIOC_IRQHandler
  .thumb_set GPIOC_IRQHandler,Default_Handler

  .weak      GPIOF_IRQHandler
  .thumb_set GPIOF_IRQHandler,Default_Handler

  .weak      DMACH12_IRQHandler
  .thumb_set DMACH12_IRQHandler,Default_Handler

  .weak      DMACH34_IRQHandler
  .thumb_set DMACH34_IRQHandler,Default_Handler

  .weak      CORDIC_IRQHandler
  .thumb_set CORDIC_IRQHandler,Default_Handler

  .weak      ADC1_IRQHandler
  .thumb_set ADC1_IRQHandler,Default_Handler

  .weak      ATIM_IRQHandler
  .thumb_set ATIM_IRQHandler,Default_Handler

  .weak      VC13_IRQHandler
  .thumb_set VC13_IRQHandler,Default_Handler

  .weak      VC24_IRQHandler
  .thumb_set VC24_IRQHandler,Default_Handler

  .weak      GTIM1_IRQHandler
  .thumb_set GTIM1_IRQHandler,Default_Handler

  .weak      GTIM2_IRQHandler
  .thumb_set GTIM2_IRQHandler,Default_Handler

  .weak      GTIM34_IRQHandler
  .thumb_set GTIM34_IRQHandler,Default_Handler

  .weak      LPTIM_IRQHandler
  .thumb_set LPTIM_IRQHandler,Default_Handler

  .weak      BTIM1_IRQHandler
  .thumb_set BTIM1_IRQHandler,Default_Handler

  .weak      BTIM2_IRQHandler
  .thumb_set BTIM2_IRQHandler,Default_Handler

  .weak      BTIM3_HALLTIM_IRQHandler
  .thumb_set BTIM3_HALLTIM_IRQHandler,Default_Handler

  .weak      I2C1_IRQHandler
  .thumb_set I2C1_IRQHandler,Default_Handler

  .weak      I2C2_IRQHandler
  .thumb_set I2C2_IRQHandler,Default_Handler

  .weak      SPI1_IRQHandler
  .thumb_set SPI1_IRQHandler,Default_Handler

  .weak      SPI23_IRQHandler
  .thumb_set SPI23_IRQHandler,Default_Handler

  .weak      UART1_IRQHandler
  .thumb_set UART1_IRQHandler,Default_Handler

  .weak      UART2_IRQHandler
  .thumb_set UART2_IRQHandler,Default_Handler

  .weak      UART3_IRQHandler
  .thumb_set UART3_IRQHandler,Default_Handler

  .weak      ADC2_DAC_IRQHandler
  .thumb_set ADC2_DAC_IRQHandler,Default_Handler

  .weak      CLKFAULT_IRQHandler
  .thumb_set CLKFAULT_IRQHandler,Default_Handler

  .end
