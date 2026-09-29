# CW32L012 BLDC Demo — CMake 构建框架

参照 **STM32CubeMX 生成 CMake 工程** 的组织方式，为 CW32L012（Cortex-M0+）BLDC 电机控制例程搭建的 CMake 构建框架。

源码已从原 Keil 工程拷贝到本目录，并按分层（Layered）方式重构，工程可独立构建、分发。

> 📄 相关文档：
> - [`REFACTOR_REPORT.md`](REFACTOR_REPORT.md) —— 重构对比报告（新旧文件 / 符号映射、行为等价性说明）
> - [`PORTING.md`](PORTING.md) —— **移植到其他芯片的完整指南**（硬件能力门槛、步骤、陷阱、工作量估算）

## 分层架构

```
        ┌──────────────────────────────────────┐
 Core/  │ main.c   入口（3 行）                │
        │ app.c    初始化编排 / 主循环 / 1ms ISR │  ← 应用层
        └───────────────────┬──────────────────┘
                            │ 只调用公开 API
        ┌───────────────────┴──────────────────┐
 BSP/   │ bsp_init   板级外设初始化               │
        │ motor_state 共享运行时状态（g_*）      │
        │ measure    电压/电流/温度 + 保护       │  ← 板级 / 电机层
        │ motor      六步换相与占空比输出         │
        │ sensorless 无感启动 + 过零换相 ISR      │
        │ motor_control 电机主状态机              │
        │ uart_protocol 调试串口协议              │
        │ pid        速度环（默认不编译）         │
        └───────────────────┬──────────────────┘
                            │
        ┌───────────────────┴──────────────────┐
Drivers/│ CW32L012 官方外设驱动库 + CMSIS       │  ← 芯片层
        └──────────────────────────────────────┘
```

**依赖方向严格单向自下而上**：`Core` → `BSP` → `Drivers`，任何一层都不得反向引用上层。

## 目录结构

```
CW_demo/
├── CMakeLists.txt                     # 顶层工程（手动维护）
├── CMakePresets.json                  # configure / build 预设（Debug / Release）
├── CW32L012C8_FLASH.ld                # GNU ld 链接脚本   ← 对应 CubeMX 的 STM32xxxx_FLASH.ld
├── startup_cw32l012.s                 # GNU as 启动文件   ← 对应 CubeMX 的 startup_stm32xxxx.s
├── cmake/
│   ├── gcc-arm-none-eabi.cmake        # 工具链文件
│   └── cw32l012/
│       └── CMakeLists.txt             # 源文件清单        ← 对应 cmake/stm32cubemx/CMakeLists.txt
├── Core/                              # 应用核心
│   ├── Inc/
│   │   └── app.h
│   └── Src/
│       ├── main.c                     # 入口：App_Init() + App_Run()
│       ├── app.c                      # 应用编排 + 主循环 + BTIM1 1ms 中断
│       ├── interrupts_cw32l012.c      # 内核异常（NMI/HardFault/SVC/PendSV）等
│       └── system_cw32l012.c          # SystemInit / FirmwareDelay
├── BSP/                               # 板级 / 电机控制
│   ├── Inc/
│   │   ├── app_config.h               # ★ 全部硬件常量（引脚、周期、阈值、时序）
│   │   ├── motor_types.h              # 状态 / 故障 / 换相步 / 退磁等枚举
│   │   ├── motor_state.h              # 共享运行时状态 g_* 声明
│   │   ├── bsp_init.h
│   │   ├── motor.h
│   │   ├── sensorless.h
│   │   ├── motor_control.h
│   │   ├── measure.h
│   │   ├── uart_protocol.h
│   │   └── pid.h
│   └── Src/                           # 与上面头文件一一对应的 .c
└── Drivers/                           # MCU 支撑代码
    ├── CMSIS/Include/                 # ARM CMSIS 内核头文件（需自行放置，见其中的 README）
    └── CW32L012_Driver/               # CW32L012 外设驱动库   ← 对应 STM32xxxx_HAL_Driver
        ├── Inc/
        └── Src/
```

### 想改硬件参数？只动 `BSP/Inc/app_config.h`

引脚、波特率、PWM 周期、电流/电压阈值、各段时序（ms）全部集中在 `app_config.h`，
按 1~11 节编号分组。业务代码里不再出现裸的魔数。

## 前置条件

| 依赖 | 版本 / 说明 |
| --- | --- |
| arm-none-eabi-gcc | ARM GNU Toolchain（本机：`D:\LZ1104\Toolchain\arm-gnu-toolchain-15.3`） |
| CMake | ≥ 3.22（本机：3.31.5） |
| Ninja | 默认生成器（本机：`D:\LZ1104\Toolchain\ninja-win`） |
| CMSIS 内核头文件 | **必须** 放入 `Drivers/CMSIS/Include/`，详见该目录下的 `README.md` |

## 构建

在 `CW_demo` 目录下执行：

```bash
cmake --preset Debug
cmake --build --preset Debug
```

生成的文件位于 `build/Debug/`：

- `CW32L012_BLDC_Demo.elf`
- `CW32L012_BLDC_Demo.hex`
- `CW32L012_BLDC_Demo.bin`
- `CW32L012_BLDC_Demo.map`

Release 构建：

```bash
cmake --preset Release
cmake --build --preset Release
```

## 在 VS Code 中使用

安装 **CMake Tools** 扩展后，把 `CW_demo` 加入工作区并打开，
`CMakePresets.json` 会被自动识别；按 `F7` 或执行 `CMake: Build` 即可。

## 源文件对照表（与原 Keil 工程）

| Keil 工程位置 | CW_demo 位置 |
| --- | --- |
| `Libraries/inc` | `Drivers/CW32L012_Driver/Inc` |
| `Libraries/src` | `Drivers/CW32L012_Driver/Src`（`system_cw32l012.c` 归入 `Core/Src`） |
| `BLDC CONTROL/User/main.c` | `Core/Src/main.c` + `Core/Src/app.c` |
| `BLDC CONTROL/MOTOR CONTORL/init.c` | `BSP/Src/bsp_init.c` + `BSP/Src/uart_protocol.c` |
| `BLDC CONTROL/MOTOR CONTORL/global.c` | `BSP/Src/motor_state.c` |
| `BLDC CONTROL/MOTOR CONTORL/compu.c` | `BSP/Src/measure.c` |
| `BLDC CONTROL/MOTOR CONTORL/control.c` | `BSP/Src/motor_control.c` |
| `BLDC CONTROL/MOTOR CONTORL/motor.c` | `BSP/Src/motor.c` |
| `BLDC CONTROL/MOTOR CONTORL/sensorless.c` | `BSP/Src/sensorless.c` |
| `BLDC CONTROL/MOTOR CONTORL/MOTOR.C`（大写扩展名） | 已取消，内容合并进 `BSP/Src/motor.c` |
| `IDEsupport/MDK/startup_cw32l012.s` | `startup_cw32l012.s`（已移植为 GNU as 语法） |
| `Objects/Project.sct` | `CW32L012C8_FLASH.ld` |

## 说明

- **文件名必须全小写。** CMake 按扩展名判定语言，而 Windows 文件名不区分大小写：
  同一个文件写成 `motor.C` 会被当作 C++ 交给 `cc1plus` 编译，出现
  `'GPIO_PIN_15' was not declared in this scope` 之类的假错误。
- **`pid.c`** 未包含在 Keil 目标中，此处默认同样不参与编译；
  如需启用，配置时加 `-DENABLE_PID=ON`。
- Keil 分散加载文件 `Project.sct` 把 `sensorless.o` 的只读内容放入 RAM 运行。
  当前链接脚本未镜像该行为（代码从 FLASH 运行），如需一致，可给相关函数添加
  `__attribute__((section(".ramfunc")))`，并在链接脚本中增加对应的 RAM 段与启动拷贝逻辑。
- **C 语言标准**设为 gnu99：GCC 15 默认 gnu23 会把旧代码中的隐式函数声明当作错误。
- 器件参数：FLASH 64 KiB @ `0x00000000`，SRAM 8 KiB @ `0x20000000`，内核 Cortex-M0+。
- 链接时的 `_read/_write/_close/_lseek is not implemented` 与
  `has a LOAD segment with RWX permissions` 属预期输出：
  前者由 `-specs=nosys.specs` 引入（裸机无文件系统），不会影响运行；
  后者是链接脚本段的权限描述所致，如需消除可加 `-Wl,--no-warn-rwx-segments`。
- **中断服务函数名由 `startup_cw32l012.s` 的向量表固定**，重构时一律保留原名：
  `ADC1_IRQHandler`、`BTIM1_IRQHandler`、`BTIM3_HALLTIM_IRQHandler`、`ADC2_DAC_IRQHandler`。
