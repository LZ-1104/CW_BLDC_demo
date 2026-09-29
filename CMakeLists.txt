################################################################################
#  CW32L012 BLDC Demo  --  CMake build framework
#
#  参照 STM32CubeMX 生成 CMake 工程的方式组织：
#
#    CW_demo/
#    ├── CMakeLists.txt                 <- 本文件 (手动维护)
#    ├── CMakePresets.json              <- configure / build 预设
#    ├── CW32L012C8_FLASH.ld            <- GNU ld 链接脚本
#    ├── startup_cw32l012.s             <- GNU as 启动文件
#    ├── cmake/
#    │   ├── gcc-arm-none-eabi.cmake    <- 工具链文件
#    │   └── cw32l012/
#    │       └── CMakeLists.txt         <- 源文件清单 (对应 cmake/stm32cubemx/CMakeLists.txt)
#    ├── Core/                          <- 应用核心 (main / app / 中断 / 系统)
#    │   ├── Inc/
#    │   └── Src/
#    ├── BSP/                           <- 板级 / 电机控制
#    │   ├── Inc/                        (app_config / motor_types / motor_state ...)
#    │   └── Src/
#    └── Drivers/                       <- MCU 支撑代码
#        ├── CMSIS/Include/             <- ARM CMSIS 内核头文件 (需自行放置)
#        └── CW32L012_Driver/           <- CW32L012 外设驱动库
#            ├── Inc/
#            └── Src/
#
#  构建方式：
#    cmake --preset Debug
#    cmake --build --preset Debug
################################################################################

cmake_minimum_required(VERSION 3.22)

# ------------------------------------------------------------------------------
# 工程名称
# ------------------------------------------------------------------------------
set(CMAKE_PROJECT_NAME CW32L012_BLDC_Demo)

# ------------------------------------------------------------------------------
# 器件 / 链接脚本 / 启动文件
# ------------------------------------------------------------------------------
set(CW32_DEVICE    CW32L012C8)                                   # Cortex-M0+ 64K Flash / 8K RAM
set(LINKER_SCRIPT  "${CMAKE_SOURCE_DIR}/CW32L012C8_FLASH.ld")
set(STARTUP_FILE   "${CMAKE_SOURCE_DIR}/startup_cw32l012.s")

# ------------------------------------------------------------------------------
# Cortex-M0+ 内核相关的编译选项
# ------------------------------------------------------------------------------
set(TARGET_SPECIFIC_COMPILE_OPTIONS
    -mcpu=cortex-m0plus
    -mthumb
)

# ------------------------------------------------------------------------------
# 引入工具链文件 (必须在 project() 之前)
# ------------------------------------------------------------------------------
include("${CMAKE_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake")

# ------------------------------------------------------------------------------
# 工程
# ------------------------------------------------------------------------------
project(${CMAKE_PROJECT_NAME} C ASM)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C 语言标准：使用 gnu99（保持与旧版工程源码兼容；GCC 15 默认 gnu23 会把
# 隐式函数声明当作错误处理）
set(CMAKE_C_STANDARD          99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        ON)

# 优化等级
set(CMAKE_C_FLAGS_DEBUG     "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE   "-Os -g0 -DNDEBUG")
set(CMAKE_ASM_FLAGS_DEBUG   "-g3")
set(CMAKE_ASM_FLAGS_RELEASE "")

# ------------------------------------------------------------------------------
# 源码目录
# ------------------------------------------------------------------------------
set(CW32_CORE_DIR   "${CMAKE_SOURCE_DIR}/Core")
set(CW32_BSP_DIR    "${CMAKE_SOURCE_DIR}/BSP")
set(CW32_DRIVER_DIR "${CMAKE_SOURCE_DIR}/Drivers/CW32L012_Driver")
set(CW32_CMSIS_DIR  "${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include"
    CACHE PATH "CMSIS Cortex-M 内核头文件目录 (包含 core_cm0plus.h)")

# ------------------------------------------------------------------------------
# CMSIS 内核头文件检查
#   cw32l012.h 会 #include "core_cm0plus.h"，因此该目录必须存在。
# ------------------------------------------------------------------------------
if(NOT EXISTS "${CW32_CMSIS_DIR}/core_cm0plus.h")
    message(FATAL_ERROR
        "未找到 CMSIS 内核头文件 'core_cm0plus.h'。\n"
        "请将 ARM CMSIS 的 Include 目录 (包含 core_cm0plus.h, cmsis_gcc.h 等)\n"
        "拷贝到：\n"
        "    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include\n"
        "或在配置时指定：\n"
        "    -DCMSIS_INCLUDE_DIR=<CMSIS Include 目录>")
endif()

# ------------------------------------------------------------------------------
# 可执行目标
# ------------------------------------------------------------------------------
add_executable(${CMAKE_PROJECT_NAME} ${STARTUP_FILE})

# 源文件清单 (见 cmake/cw32l012/CMakeLists.txt)
add_subdirectory(cmake/cw32l012)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${CW32_CORE_DIR}/Inc"
    "${CW32_BSP_DIR}/Inc"
    "${CW32_DRIVER_DIR}/Inc"
    "${CW32_CMSIS_DIR}"
)

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    ${TARGET_SPECIFIC_COMPILE_OPTIONS}
    -Wall
    -fdata-sections
    -ffunction-sections
)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    ${TARGET_SPECIFIC_COMPILE_OPTIONS}
    -T${LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -specs=nano.specs
    -specs=nosys.specs
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
    OUTPUT_NAME ${CMAKE_PROJECT_NAME}
    SUFFIX      ".elf"
)

# ------------------------------------------------------------------------------
# 后处理：生成 .hex / .bin 并打印大小
# ------------------------------------------------------------------------------
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMENT "Generating .hex and .bin"
    VERBATIM
)
