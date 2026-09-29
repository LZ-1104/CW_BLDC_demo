################################################################################
# gcc-arm-none-eabi.cmake  --  CMake 工具链文件 (裸机 ARM Cortex-M)
#
# 用法：在根 CMakeLists.txt 中 project() 之前 include 本文件，
#       或在 CMakePresets 中通过 "toolchainFile" 指定。
################################################################################

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 允许通过环境变量 / 缓存变量覆盖工具链前缀，便于指定自定义安装路径
if(NOT DEFINED TOOLCHAIN_PREFIX)
    set(TOOLCHAIN_PREFIX "arm-none-eabi-")
endif()

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}ar)
set(CMAKE_RANLIB       ${TOOLCHAIN_PREFIX}ranlib)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_OBJDUMP      ${TOOLCHAIN_PREFIX}objdump)
set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size)
set(CMAKE_GDB          ${TOOLCHAIN_PREFIX}gdb)

# 裸机环境：目标文件不是可执行程序，避免 CMake 试图链接测试程序
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 仅在工具链根目录下查找库 / 头文件
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
