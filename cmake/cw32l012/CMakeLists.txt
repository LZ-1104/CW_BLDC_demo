################################################################################
#  CW32L012 - 源文件清单
#
#    本目录下所有源文件必须使用全小写文件名与 .c/.h 后缀。
#    CMake 依据扩展名判定语言，而 Windows 文件名不区分大小写：
#    若写成 "motor.C"（大写 C），CMake 会按 C++ 扩展名处理并调用 cc1plus，
#    出现 "command-line option '-std=gnu99' is valid for C/ObjC but not for C++"
#    之类的告警。
#
#  与 Keil MDK 工程 "BLDC CONTROL/Project.uvprojx" 中的 Target_1 保持一致。
#  修改工程结构时，只需维护本文件。
################################################################################

# ------------------------------------------------------------------------------
# 可选项
# ------------------------------------------------------------------------------
option(ENABLE_PID "编译 BSP/Src/pid.c (默认不参与编译，与 Keil 目标一致)" ON)

# ------------------------------------------------------------------------------
# Core : 应用核心 (入口 / 应用编排 / 中断 / 系统)
# ------------------------------------------------------------------------------
set(CW32_CORE_SOURCES
    "${CW32_CORE_DIR}/Src/system_cw32l012.c"
    "${CW32_CORE_DIR}/Src/main.c"
    "${CW32_CORE_DIR}/Src/app.c"
    "${CW32_CORE_DIR}/Src/interrupts_cw32l012.c"
)

# ------------------------------------------------------------------------------
# Drivers : CW32L012 外设驱动库
# ------------------------------------------------------------------------------
set(CW32_HAL_SOURCES
    "${CW32_DRIVER_DIR}/Src/cw32l012_adc.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_atim.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_btim.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_cordic.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_crc.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_dac.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_debug.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_digitalsign.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_dma.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_eau.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_flash.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_gpio.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_gtim.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_halltim.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_irmod.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_iwdt.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_lpi2c.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_lptim.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_lvd.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_opa.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_pwr.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_ram.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_rtc.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_spi.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_sysctrl.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_systick.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_uart.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_vc.c"
    "${CW32_DRIVER_DIR}/Src/cw32l012_wwdt.c"
)

# ------------------------------------------------------------------------------
# BSP : 板级外设 / 电机控制
#   每层只依赖下一层，依赖方向单向自下而上，不得反向引用。
# ------------------------------------------------------------------------------
set(CW32_BSP_SOURCES
    # ---- 板级外设初始化 ----
    "${CW32_BSP_DIR}/Src/bsp_init.c"
    # ---- 共享运行时状态 ----
    "${CW32_BSP_DIR}/Src/motor_state.c"
    # ---- 量测与保护 ----
    "${CW32_BSP_DIR}/Src/measure.c"
    # ---- 电机驱动与换相 ----
    "${CW32_BSP_DIR}/Src/motor.c"
    # ---- 无感启动与过零检测 ----
    "${CW32_BSP_DIR}/Src/sensorless.c"
    # ---- 电机状态机 ----
    "${CW32_BSP_DIR}/Src/motor_control.c"
    # ---- 串口上报 ----
    "${CW32_BSP_DIR}/Src/uart_protocol.c"
)

if(ENABLE_PID)
    list(APPEND CW32_BSP_SOURCES "${CW32_BSP_DIR}/Src/pid.c")
endif()

# ------------------------------------------------------------------------------
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CW32_CORE_SOURCES}
    ${CW32_HAL_SOURCES}
    ${CW32_BSP_SOURCES}
)
