# CW32L012 BLDC 固件 —— 移植指南

> 面向对象：要把本工程搬到另一颗 MCU（STM32 / GD32 / HC32 / MM32 / 其他 CW32 型号 …）的人。

---

## 0. 一句话结论

**分层已经做对了，但离"只改一层就能换芯片"还差一步。**

- ✅ **纯算法层（状态机、无感换相逻辑、共享状态、串口帧格式）是 100% 纯 C，可以原样搬走**，
  不依赖任何寄存器、CMSIS 或厂商头文件。
- ⚠️ **但寄存器操作没有收敛到单一目录**。目前有 **4 个 `.c` 文件**在直接操作芯片外设，
  它们分散在 BSP 里，而不是集中在一个"端口层"。换芯片必须逐个改。
- 📌 因此现在的移植成本是：**改 4 个 .c + 2 个头 + 2 个构建文件**，而不是"改 1 个 `bsp_init.c`"。

下文的 §7 给出把移植面收敛到 1 个目录的改造方法（约半天工作量，之后每次换芯片只改那里）。

---

## 1. 可移植性体检

按文件统计芯片专有标识符（`CW_ATIM` / `CW_BTIM` / `CW_ADC` / `__SYSCTRL_` / `NVIC_` / `cw32l012` 等）的占比：

| 文件 | 总行数 | 芯片相关行 | 占比 | 可移植性 |
| --- | ---: | ---: | ---: | --- |
| `BSP/Src/motor_state.c` | 93 | 0 | 0% | 🟢 零改动 |
| `BSP/Src/pid.c` | 63 | 0 | 0% | 🟢 零改动 |
| `BSP/Src/motor_control.c` | 208 | 3 | 1% | 🟢 仅 include 需改 |
| `BSP/Src/measure.c` | 146 | 2 | 1% | 🟡 1 处出厂校准地址 |
| `BSP/Src/uart_protocol.c` | 41 | 2 | 5% | 🟡 2 个串口 API |
| `BSP/Src/sensorless.c` | 307 | 29 | 9% | 🟠 需改 ISR 与定时器操作 |
| `BSP/Src/bsp_init.c` | 274 | 48 | 18% | 🔴 **主战场，几乎重写** |
| `BSP/Src/motor.c` | 82 | 22 | 27% | 🔴 直写 PWM 比较寄存器 |
| `Core/Src/app.c` | 257 | 6 | 2% | 🟠 BTIM1 ISR + ADC2 触发 |
| `BSP/Inc/app_config.h` | 229 | 12 | 5% | 🟠 引脚 / 端口 / 串口宏 |
| `Core/Src/system_cw32l012.c` | 67 | — | — | 🔴 时钟树 + `FirmwareDelay` |
| `Core/Src/interrupts_cw32l012.c` | 339 | — | — | 🔴 内核异常处理 |
| `startup_cw32l012.s` | — | — | — | 🔴 向量表（**决定 ISR 名字**） |
| `CW32L012C8_FLASH.ld` | — | — | — | 🔴 存储区布局 |

---

## 2. 文件分类

### A 类 — 零改动，直接复制

| 文件 | 说明 |
| --- | --- |
| `BSP/Inc/motor_types.h` | 5 个 `enum`，纯 C |
| `BSP/Inc/motor_state.h` + `BSP/Src/motor_state.c` | 共享全局变量 |
| `BSP/Inc/motor_control.h` + `BSP/Src/motor_control.c` | 8 个状态机函数 |
| `BSP/Inc/pid.h` + `BSP/Src/pid.c` | 默认不编译 |

> 这四组是控制逻辑的全部，**移植时一行都不用动**。这是本次重构最大的收益。

### B 类 — 改 include 与几个宏

| 文件 | 要改什么 |
| --- | --- |
| `BSP/Inc/app_config.h` | §2 串口、§3~5 引脚、`DEBUG_UART`、`*_AF_*()` 复用宏 |
| `BSP/Src/measure.c` | `ADC_BGR_VOL_ADDRESS`（出厂基准校准地址）→ 换成新芯片的等效值 |
| `BSP/Src/uart_protocol.c` | `UART_SendData()` / `UART_GetFlagStatus()` → 新芯片的串口 API |

### C 类 — 必须重写

| 文件 | 要改什么 |
| --- | --- |
| `BSP/Src/bsp_init.c` | 9 个 `Bsp_InitXxx()`，全部外设初始化 |
| `BSP/Src/motor.c` | 22 处 `CW_ATIM->CCR1..4` 直写 |
| `BSP/Src/sensorless.c` | BTIM2/BTIM3 操作 + `ADC1_IRQHandler` |
| `Core/Src/app.c` | `BTIM1_IRQHandler` 计数/清标志 + `ADC_SoftwareStartConvCmd` |
| `Core/Src/system_*.c`、`interrupts_*.c` | 时钟树、异常处理 |
| `startup_*.s`、`*.ld` | 向量表、存储区 |

---

## 3. 新芯片必须满足的硬件能力

移植**之前**先逐条核对，任何一条不满足都要改架构而不只是改代码：

| # | 需求 | 本工程用法 | 不满足时的替代方案 |
| --- | --- | --- | --- |
| 1 | **高级定时器 ×1**，≥4 个比较通道 | CH1/2/3 出三相 PWM；**CH4 配置为触发 ADC** | 若 CH4 不能触发 ADC，改用 CC1 触发（需重排占空比写入时机），或用另一个基本定时器做触发源 |
| 2 | **基本定时器 ×3**（或 1 个多通道定时器） | BTIM1 = 1ms 心跳；BTIM2 = 换相步长测量；BTIM3 = 换相延时 | 可合并到 1 个定时器的 3 个通道 |
| 3 | **ADC ×2**（或 1 个 ADC 多通道） | ADC1 = 定时器硬件触发，同步采样 3 相反电动势；ADC2 = 5 路慢速量 | 单 ADC 需分时复用，会改变采样时序（**风险点**） |
| 4 | **DMA ×1 通道**，支持外设触发 | 搬 ADC2 的 5 个结果到 `g_adc2_raw[]` | 不用 DMA 则改中断逐通道读，会引入延迟 |
| 5 | **运放 OPA ×1** | 电流采样，增益由外部电阻设定 | ⚠️ 多数 MCU 无此模块 → 用外部运放，或选带 PGA 的 ADC |
| 6 | **内部基准 + 出厂校准值** | `ADC2_IN4 = 内部 1.2V`，`ADC_BGR_VOL_ADDRESS` 读校准码 | 无校准值则改固定系数换算，**精度下降** |
| 7 | **GPIO ≥ 12 个** | 3 上桥 + 3 下桥 + LED + KEY + UART TX/RX + 4 路模拟输入 | — |
| 8 | 内核 ≥ Cortex-M0+，有 NVIC | 4 个外设中断 | CMSIS 通用，厂商无关 |

> 🔴 **第 1、3、5 条是硬约束。** 特别是第 1 条：**PWM 通道 4 硬件触发 ADC 采样的做法，
> 决定了反电动势的采样时刻恰好落在续流结束后的窗口**。如果新芯片做不到，
> 无感过零检测的可靠性会直接受影响，需要重新设计采样时机。

---

## 4. 移植步骤

### Step 1 — 搭骨架

1. 复制 `cmake/` 目录，把 `gcc-arm-none-eabi.cmake` 里的 `CMAKE_C_FLAGS`
   改成新芯片的 `-mcpu=` / `-mfpu=` / `-mfloat-abi=`。
2. 换掉 `startup_*.s` 和 `*.ld`。
   - ⚠️ 启动文件里的**向量表决定了 ISR 函数名**，见 Step 8。
3. **保持 `-std=gnu99`**（GCC 15 默认 gnu23 会把旧代码的隐式函数声明当错误）。
4. 保持文件名**全小写 `.c`**（见 §6 陷阱）。

### Step 2 — 时钟与延时（改 `system_*.c`）

| 要求 | 说明 |
| --- | --- |
| `SystemInit()` | 配到 `SYSTEM_CORE_CLOCK_HZ`（当前 96MHz） |
| `FirmwareDelay(n)` | 阻塞延时，单位约 1ms。**必须重写**，其它模块依赖它 |
| `ADC_BGR_VOL_ADDRESS` | 新芯片的出厂校准值地址；没有就删掉 `AdcCodeToMillivolt()` 里的校准分支 |

### Step 3 — GPIO（改 `app_config.h` + `Bsp_InitLed/Key`）

按 §3~5 节替换端口/引脚宏。三种宏形式：

```c
#define LED_GPIO_PORT   CW_GPIOC              /* → 新芯片的端口基址   */
#define LED_GPIO_PIN    GPIO_PIN_13           /* → 新芯片的引脚掩码   */
#define LED_ON()        GPIO_WritePin(...)    /* → 新芯片的写引脚 API */
#define DEBUG_UART_AF_TX()  PB12_AFx_UART1TXD()  /* → 新芯片的复用配置宏 */
```

> 💡 **建议**：不要直接用厂商的 `GPIO_WritePin`，而是在 `app_config.h` 里定义
> `BOARD_GPIO_SET(port, pin)` / `BOARD_GPIO_RESET(port, pin)` 两个自己的宏，
> 这样引脚宏在换芯片时完全不用动。当前代码是直接用厂商 API 的。

### Step 4 — PWM（改 `Bsp_InitPwm` + `motor.c`）

这是**拓扑敏感**的部分：

| 本工程做法 | 说明 |
| --- | --- |
| 上桥臂（AH/BH/CH）= 定时器比较输出**复用引脚** | CH1/CH2/CH3，`PWM1` 模式，高电平有效，预装载 **ENABLE** |
| 下桥臂（AL/BL/CL）= **普通推挽 GPIO** | 不用互补输出，软件在换相时开关 |
| `CCR4` = 触发 ADC 的比较值 | 换相点移到 `PWM_CCR4_OFF_VALUE`，正常时 `PWM_CCR4_ON_VALUE` |
| 载波 96MHz / 4800 = **20 kHz** | `PWM_PERIOD 4800` |

**`motor.c` 里的 22 处直写是刻意的，不要"规范化"成库函数：**

```c
CW_ATIM->CCR1 = 0;                  /* ← 对应新芯片的"直接写比较寄存器" */
```

> ⚠️ 库函数（如 `ATIM_OC1Init` / HAL 的 `__HAL_TIM_SET_COMPARE` 若带影子寄存器刷新）
> 有预装载和事件同步，在换相点使用会**引入额外一个 PWM 周期的延迟**。
> 移植时也必须在换相处直写比较寄存器。

### Step 5 — ADC1（最关键，改 `Bsp_InitAdc1` + `ADC1_IRQHandler`）

| 本工程配置 | 值 |
| --- | --- |
| 触发源 | `ATIM_CC4` 硬件触发 |
| 转换模式 | 单次多通道（`SqrEns0to3`，4 通道） |
| 通道映射 | IN0=CH8 电流、IN1=CH0 BEMF-A、IN2=CH1 BEMF-B、IN3=CH2 BEMF-C |
| 中断 | `ADC_IT_EOS`（序列结束） |
| 结果保存 | `g_adc1_raw[6]`，**只用前 4 个** |

ISR 里做的是：判断退磁是否结束 → 找过零点 → 换相。
**这段逻辑在 `sensorless.c` 里，是纯算法，可以原样保留**，只需把读取结果的方式换掉。

### Step 6 — ADC2 + DMA（改 `Bsp_InitAdc2`）

5 路顺序采样 → DMA 搬运到 `g_adc2_raw[]`：

```c
dma.DataSize      = DMA_DATA_SIZE_32BITS;      /* ⚠️ 必须 32 位 */
dma.TransferCount = ADC2_SAMPLE_COUNT;         /* 5 */
dma.BlockCount    = 1;                         /* ⚠️ 必须为 1 */
dma.SrcAddress    = (uint32_t)&(CW_ADC2->RESULT0);  /* 结果寄存器连续排列 */
dma.DstAddress    = (uint32_t)&g_adc2_raw;
dma.TriggerSource = DMA_TRIGGER_SRC_ADC2_SEQ;
dma.RestartEnable = TRUE;
```

> ⚠️ `g_adc2_raw[]` 声明为 `uint32_t` 是**必须**的（对应 32 位 DMA 传输宽度），
> 换成 `uint16_t` 会导致数据错位。新芯片若 DMA 宽度不同，需同步改数组类型。

### Step 7 — 定时器（3 个）

| 定时器 | 用途 | 周期 | 移植要点 |
| --- | --- | --- | --- |
| BTIM1 | **1ms 心跳**，驱动所有 ms 级计时 | 1 ms | 见 Step 8，是全局时序基准 |
| BTIM2 | 换相步长测量 | 计数模式 | 换相时读 `CNT` 得 `g_comm_step_time`，然后清零 |
| BTIM3 | 换相延时（过零后延时 30° 电角度） | 单次 | `Autoreload = g_comm_step_time >> 3` |

### Step 8 — 中断向量（**最容易踩坑**）

ISR 函数名由启动文件的向量表**硬性固定**，不能重命名：

| ISR 名 | 位置 | 对应外设 |
| --- | --- | --- |
| `BTIM1_IRQHandler` | `app.c` | 1ms 心跳 |
| `BTIM3_HALLTIM_IRQHandler` | `sensorless.c` | 换相延时到期 |
| `ADC1_IRQHandler` | `sensorless.c` | 反电动势采样完成 |
| `ADC2_DAC_IRQHandler` | `interrupts_cw32l012.c` | ADC2 |
| `NMI_Handler` / `HardFault_Handler` / `SVC_Handler` / `PendSV_Handler` | `interrupts_cw32l012.c` | 内核异常 |

**移植时两种做法：**

- **做法 A（推荐）**：保留函数名不变，改启动文件里的向量表条目指向新芯片的中断号。
  这样 `app.c` / `sensorless.c` 一行都不用改。
- **做法 B**：把函数名改成新芯片的（如 `TIM1_UP_IRQHandler`），同时改所有调用点。

> 注意：`ADC1_IRQHandler` 这个名字在新芯片上通常被占用或不存在，
> 若新芯片有同名但不同用途的中断，**必须**走做法 B 改名，否则会静默产生错误行为。

### Step 9 — 应用层适配（改 `app.c`，仅 6 处）

| 位置 | 本工程 | 替换为 |
| --- | --- | --- |
| `BTIM1_IRQHandler` | `BTIM_GetITStatus` / `BTIM_ClearITPendingBit` | 新芯片的取/清中断标志 API |
| `BTIM1_IRQHandler` | `ADC_SoftwareStartConvCmd(CW_ADC2, ENABLE)` | 新芯片的软件触发 ADC API |
| `App_Init()` | `FirmwareDelay(APP_POWER_ON_DELAY)` | 保持，只依赖 Step 2 |

其余（状态机调度、按键、测速、堵转、自动关机）都是纯 C。

### Step 10 — 验证（按顺序）

1. **时钟**：点灯，确认 `FirmwareDelay(1000)` ≈ 1 秒。
2. **1ms 心跳**：`BTIM1_IRQHandler` 里翻转一个空闲 GPIO，示波器验证是 1kHz。
   > ⚠️ **这一步必须先过**。所有时序（400ms 启动延时、3000ms 爬坡、100ms 量测、
   > 10s 自动关机）都基于它。
3. **PWM**：示波器看上下桥，确认 20kHz、死区、换相顺序（6 步循环）。
4. **ADC1**：确认能采到三相反电动势，且采样点落在续流结束后的窗口。
5. **ADC2 + DMA**：确认 `g_adc2_raw[0..4]` 都在合理范围。
6. **串口**：用上位机按 `0x43 0x57 0x04 level voltage closeflag checksum` 解帧（7 字节）。
7. **整机**：加电机，从 0 档逐级升档，观察是否正常闭环。

---

## 5. 换算系数（换硬件必改）

`measure.c` 里的换算链路：

```
mV   = BGR × adc / Vref_code                               ← ADC 基准校准
mA   = mV × 1000 / (CURRENT_AMP_GAIN × CURRENT_SHUNT_MILLIOHM)
0.1V = mV × (VOLTAGE_DIV_RV1 + VOLTAGE_DIV_RV2) / VOLTAGE_DIV_RV1 / 100
```

> ⚠️ **已知不一致**：`measure.c` 中电流换算写的是硬编码的 `* 10U`，
> 而不是引用 `CURRENT_AMP_GAIN` / `CURRENT_SHUNT_MILLIOHM` 两个宏。
> 数值上等价（`1000 / (10 × 10) = 10`），但**改硬件参数时容易漏改**。
> 建议改为 `* (1000U / (CURRENT_AMP_GAIN * CURRENT_SHUNT_MILLIOHM))`。

换板子时逐项核对：`VOLTAGE_DIV_RV1/RV2`、`CURRENT_SHUNT_MILLIOHM`、`CURRENT_AMP_GAIN`、
`BUS_CURRENT_BIAS_DEFAULT`（0A 时的 ADC 码值，需实测）、`NTC_OVERTEMP_ADC_THRESHOLD`（换 NTC 必改）。

---

## 6. 陷阱清单

| # | 陷阱 | 后果 |
| --- | --- | --- |
| 1 | **ISR 名与向量表不匹配** | 中断静默失效，电机不转，且**不报错** |
| 2 | `g_adc2_raw[]` 改成 `uint16_t` | DMA 数据错位，电压/电流全乱 |
| 3 | 把 `CW_ATIM->CCRx = v` 换成库函数 | 换相延迟一个 PWM 周期，高速时失步 |
| 4 | `g_pwm_duty` 超过 `PWM_PERIOD` | 占空比 100% 后无法再调，或比较值溢出 |
| 5 | 上桥臂 PWM + 下桥臂 GPIO 的拓扑改成互补输出 | 换相逻辑全部作废，需重写 `motor.c` |
| 6 | BTIM1 不是精确 1ms | 所有时间常量等比失真 |
| 7 | 用 C++ 编译器编 `.c`（如文件名写成 `.C`） | CMake 按扩展名判语言，Windows 不区分大小写，会报莫名其妙的 "was not declared in this scope" |
| 8 | 改回 `-std=gnu23` | GCC 15 把隐式函数声明当错误，编译失败 |
| 9 | 忘记 `PWM_CCR4_OFF_VALUE` 的语义 | 停机时 ADC 仍被触发，或触发点落在开关噪声上 |
| 10 | `app_config.h` 里 `PWM_AH_ON()` / `PWM_BH_ON()` / `PWM_CH_ON()` 是**未使用的死宏** | 上桥由定时器硬件驱动，**不要**照抄到新工程去手动开关上桥，会上下桥直通 |
| 11 | `HARD_FAULT_ADC_THRESHOLD` / `VOLTAGE_FAULT_UNDER_V` / `CURRENT_SHUNT_MILLIOHM` / `CURRENT_AMP_GAIN` 目前**只在配置头里声明、代码未引用** | 不要误以为它们生效了；见 §5 |

---

## 7. 建议：加一层 HAL，把移植面收敛到 1 个目录

现状是 4 个 `.c` 直接碰寄存器。若后续还要换芯片（或维护多个型号），
建议插入一个 `BSP/Port/` 目录，把芯片相关代码全部关进去：

```
BSP/Port/             ← 换芯片只改这个目录
    port_gpio.c       HalGpio_Init / HalGpio_Set / HalGpio_Reset / HalGpio_Read
    port_pwm.c        HalPwm_Init / HalPwm_SetCompare(ch, ccr) / HalPwm_SetTrigger(ccr)
    port_time.c       HalTime_Init / HalTime_GetMs / HalTime_StepTimerReset / ...
    port_adc.c        HalAdc_Init / HalAdc_GetRaw(ch) / HalAdc_StartSoftTrig
    port_uart.c       HalUart_Init / HalUart_SendByte / HalUart_IsTxEmpty
    port_sys.c        HalSys_Init / HalSys_DelayMs / HalSys_GetCalibValue
```

对应的改造动作：

| 改造 | 说明 |
| --- | --- |
| `motor.c` 的 `CW_ATIM->CCRx = v` → `HalPwm_SetCompare(1, v)` | 仍是**直写寄存器**，只是多一层转发，不影响时序 |
| `sensorless.c` 的 `CW_BTIM2/3` 操作 → `HalTime_*` | 定时器语义抽象 |
| `app.c` 的 `BTIM_GetITStatus` / `ADC_SoftwareStartConvCmd` → `HalTime_PollTickIrq()` / `HalAdc_StartSoftTrig()` | |
| `app_config.h` 的厂商宏 → `HalGpio_*` 常量 | 引脚定义仍留在 `app_config.h` |
| ADC1/ADC2 的 ISR **仍留在 `sensorless.c` / `interrupts_*.c`** | ISR 名由向量表决定，无法下移；但 ISR 体只调 `HalAdc_*` 取值 |

改造后：**换芯片 = 重写 `BSP/Port/` 目录（6 个文件）+ 换 `startup*.s` / `*.ld` / 工具链文件**，
`motor_control.c` / `motor_state.c` / `pid.c` / `sensorless.c` 的算法部分**完全不动**。

> 💡 如果想进一步降低风险：在 `Port/` 里对每个函数写 `assert` 或注释说明时序约束
> （例如 `HalPwm_SetCompare` 必须"立即生效，不得有影子寄存器延迟"），
> 否则下一个移植的人很容易又换回库函数。

---

## 8. 工作量估算

| 场景 | 工作量 | 说明 |
| --- | --- | --- |
| **同系列芯片**（如 CW32L010 / CW32L011） | **0.5 ~ 1 天** | 改引脚宏 + `bsp_init.c` 通道号，算法不动 |
| **同厂商、不同外设结构** | **3 ~ 5 天** | 需重写 ADC/PWM 初始化，逻辑不变 |
| **跨厂商**（STM32 / GD32 / HC32） | **1 ~ 2 周** | 含运放替代方案、校准方案、时序重新验证 |
| **目标芯片无 OPA / 无内部基准** | **+3 ~ 5 天** | 硬件需要改板，属架构级变更 |
| 先做 §7 的 HAL 改造 | **+0.5 ~ 1 天（一次性）** | 之后每次换芯片省 60~70% 工作量 |

**风险最高的三处**（时间要留足）：

1. **反电动势采样时机** —— 依赖"PWM 通道 4 硬件触发"，新芯片若机制不同，需重新验证过零检测。
2. **1ms 心跳精度** —— 所有时间常量都挂在上面。
3. **中断向量名映射** —— 错了不报错，只是不动。

---

## 附：移植检查表

```
[ ] 新芯片有高级定时器，第 4 通道能触发 ADC
[ ] 有 ≥1 个可用 DMA 通道，支持外设触发
[ ] 运放方案确定（片内 OPA / 外部运放 / 带 PGA 的 ADC）
[ ] 基准与校准方案确定（内部 1.2V + 出厂值 / 外部基准 / 固定系数）
[ ] .c 文件名全小写
[ ] -std=gnu99 已设置
[ ] SYSTEM_CORE_CLOCK_HZ 与实际时钟一致
[ ] FirmwareDelay 精度已实测
[ ] 启动文件向量表 ↔ ISR 函数名 已逐条核对
[ ] g_adc2_raw[] 仍为 uint32_t
[ ] motor.c 仍为直写比较寄存器
[ ] 分压电阻 / 采样电阻 / 增益 / NTC 曲线 / 电流零偏 已按新硬件重算
[ ] BTIM1 实测为精确 1ms
[ ] 串口 7 字节帧已用上位机验证
```
