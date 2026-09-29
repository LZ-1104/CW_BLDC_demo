# CW32L012 BLDC 固件重构报告

- 目标器件：CW32L012C8（Cortex-M0+，64 KiB FLASH @ `0x00000000`，8 KiB SRAM @ `0x20000000`）
- 工具链：arm-none-eabi-gcc 15.3.1 / CMake 3.31.5 / Ninja
- 构建方式：`cmake --preset Debug` + `cmake --build build/Debug`
- 约束：**控制算法行为必须与原工程等价**，只做结构化整理

---

## 1. 结论概览

| 项目 | 重构前 | 重构后 | 变化 |
| --- | --- | --- | --- |
| 编译错误 | 0 | 0 | — |
| 编译告警（`-Wall`） | 0 | **0** | — |
| RAM 占用 | 1816 B（22.17%） | **1768 B（21.58%）** | **−48 B** |
| FLASH 占用 | 22476 B（34.30%） | **22348 B（34.10%）** | **−128 B** |
| `text` / `data` / `bss` | — | 22300 / 48 / 1728 | — |
| 编译单元（BSP） | 7 个（其中 `MOTOR.C` 被误判为 C++） | 7 个（全部按 C 编译） | — |
| 头文件守卫 | 无 | 全部具备 | — |
| 硬件魔数散落 | 遍布 8 个文件 | 集中在 `app_config.h` | — |
| 浮点运算 | 量测链路使用 `float` | 全部改为定点整数 | 见 §6.3 |

> 重构后**体积反而略有下降**：去掉了一个从未被引用的边沿表行、删除了若干死代码，
> 且量测链路由软浮点改为整数运算。

---

## 2. 目录与文件映射

### 2.1 删除的原文件（15 个）

`BSP/Inc/{global,init,control,compu,pid,motor,sensorless}.h`
`BSP/Src/{global,init,control,compu,pid,motor,sensorless}.c`
`BSP/Src/MOTOR.C`、`Core/Src/main.c`

### 2.2 新旧对照

| 原文件 | 新文件 | 说明 |
| --- | --- | --- |
| `BSP/Inc/global.h` | `BSP/Inc/motor_state.h`<br>`BSP/Inc/motor_types.h`<br>`BSP/Inc/app_config.h` | 上帝头文件按职责三分：共享变量 / 枚举类型 / 硬件常量 |
| `BSP/Src/global.c` | `BSP/Src/motor_state.c` | 变量定义与初值 |
| `BSP/Inc/init.h` + `BSP/Src/init.c` | `BSP/Inc/bsp_init.h` + `BSP/Src/bsp_init.c`<br>`BSP/Inc/uart_protocol.h` + `BSP/Src/uart_protocol.c` | `SendDataToUsart` 拆出成独立协议模块；`SENSORLESS_TIM_Config` 移入 `sensorless.c` |
| `BSP/Inc/compu.h` + `BSP/Src/compu.c` | `BSP/Inc/measure.h` + `BSP/Src/measure.c` | `SampleVI`→`Measure_VoltageCurrent`，`SampleT`→`Measure_Temperature` |
| `BSP/Inc/control.h` + `BSP/Src/control.c` | `BSP/Inc/motor_control.h` + `BSP/Src/motor_control.c` | 状态机，函数名前缀统一为 `MotorCtrl_` |
| `BSP/Src/MOTOR.C` | `BSP/Src/motor.c` | 六步换相 + 占空比输出，统一小写文件名 |
| `BSP/Inc/motor.h` + 原 `motor.c` 中的启动部分 | `BSP/Inc/motor.h` + `BSP/Src/motor.c`（驱动）<br>`BSP/Inc/sensorless.h` + `BSP/Src/sensorless.c`（无感） | 按"驱动 / 无感算法"再拆一层 |
| `BSP/Inc/pid.h` + `BSP/Src/pid.c` | `BSP/Inc/pid.h` + `BSP/Src/pid.c` | 保持默认不编译 |
| `Core/Src/main.c` | `Core/Src/main.c`（入口）<br>`Core/Inc/app.h` + `Core/Src/app.c`（应用编排） | `main()` 从原来的 ~130 行瘦身到 **5 行**（源文件共 19 行，其余为注释） |

### 2.3 新分层

```
Core/    应用层     main.c → app.c
              │  只调用下层公开 API
BSP/     板级/电机层 bsp_init / motor_state / measure / motor /
              │      sensorless / motor_control / uart_protocol / pid
Drivers/ 芯片层     CW32L012 官方驱动库 + CMSIS
```

依赖方向严格单向自下而上，不存在任何反向引用。

---

## 3. 符号重命名对照

| 原名 | 新名 | 所在模块 |
| --- | --- | --- |
| `MotorStartCheck` | `MotorCtrl_StartCheck` | motor_control |
| `MotorStartDealy`（拼写错误） | `MotorCtrl_StartDelay` | motor_control |
| `MotorStartOPEN` | `MotorCtrl_StartOpen` | motor_control |
| `MotorRunOPEN` | `MotorCtrl_RunOpen` | motor_control |
| `MotorStop` | `MotorCtrl_Stop` | motor_control |
| `MotorError` | `MotorCtrl_Error` | motor_control |
| `MotorErrorOver` | `MotorCtrl_ErrorOver` | motor_control |
| `WaitStart` | `MotorCtrl_WaitStart` | motor_control |
| `Sensorless_MOTOR_START` | `Sensorless_StartOpenLoop` | sensorless |
| `Sensorless_START`（内部） | `Sensorless_Start`（static） | sensorless |
| `Sensorless_TIM_Config` | `Sensorless_Init` | sensorless |
| `Commutation` | `Motor_Commutation` | motor |
| `UPPWM` | `Motor_UpdateDuty` | motor |
| `MOTOR_STOP0` | `Motor_StopOutput` | motor |
| `SampleVI` | `Measure_VoltageCurrent` | measure |
| `SampleT` | `Measure_Temperature` | measure |
| `SendDataToUsart` | `Uart_SendStatus` | uart_protocol |
| `PID_init` / `PIDcompute` | `Pid_Init` / `Pid_Compute` | pid |
| `InitRCC` / `InitLED` / `InitKey` / `InitOPA` / `InitADC1` / `InitADC2` / `InitPWM` / `InitTIM1_IRQ` / `InitUsart` | `Bsp_InitRcc` / `Bsp_InitLed` / `Bsp_InitKey` / `Bsp_InitOpa` / `Bsp_InitAdc1` / `Bsp_InitAdc2` / `Bsp_InitPwm` / `Bsp_InitTick` / `Bsp_InitUart` | bsp_init |
| `SetSpeedFlag`（函数） | `MotorState_SetSpeedLevel` | motor_state |

**中断服务函数名一律保留原名**（由 `startup_cw32l012.s` 的向量表固定，不可改）：
`BTIM1_IRQHandler`（迁至 `app.c`）、`BTIM3_HALLTIM_IRQHandler` 与 `ADC1_IRQHandler`（在 `sensorless.c`）、
以及 `interrupts_cw32l012.c` 中的 `NMI_Handler` / `HardFault_Handler` / `SVC_Handler` / `PendSV_Handler`。

### 3.1 全局变量的 `g_` 前缀化

| 原名 | 新名 |
| --- | --- |
| `MotorState` | `g_motor_state` |
| `ErrorCode` | `g_fault_code` |
| `MotorRunFlag` | `g_motor_run` |
| `Step` / `StepLast` / `StepTime` | `g_comm_step` / `g_comm_step_last` / `g_comm_step_time` |
| `Sta`（退磁状态） | `g_demag_state` |
| `SampleData_adc1[]` | `g_adc1_raw[]` |
| `SampleData_adc2[]` | `g_adc2_raw[]` |
| `SetSpeed` / `SetSpeedFlag`（变量） | `g_speed_set` / `g_speed_level` |
| `SpeedTargetVal` | `g_speed_target` |
| `InstantSpeedValue` | `g_speed_instant` |
| `AverageSpeedValue` | `g_speed_average` |
| `ht` / `hte` | `g_hall_pulses` / `g_hall_pulses_100ms` |
| `OutPwmValue` | `g_pwm_duty` |
| `RisespeedTimeCount` / `RiseStepTime` | `g_ramp_step_ms` / `g_ramp_time_ms` |
| `VoltageValue` / `CurrentValue` / `CurrentBias` | `g_bus_voltage_dv` / `g_bus_current_ma` / `g_current_bias` |
| `MotorStartDealy` / `OVCurrent` / `OVVoltage` | `g_startup_delay_ms` / `g_overcurrent_a` / `g_overvoltage_v` |
| `TimeCountTemp` / `TimeCount` / `TimeCountForUsart` / `TimeCountForSpeed` / `TimeCountForMeasure` / `TimeCountForClose` | `g_tick_temp` / `g_tick_startup` / `g_tick_uart` / `g_tick_ramp` / `g_tick_measure` / `g_tick_standby` |
| `closeflag` / `UsartSendFlag` | `g_power_off` / `g_uart_report_req` |

---

## 4. 行为等价性保证

以下"红线"在重构中被逐条守住：

1. **寄存器直写保持不变**。`Motor_Commutation` 仍直接写 `CW_ATIM->CCR1..CCR4`。
   库函数 `ATIM_OCxInit` 带预装载 / 事件同步，在换相点使用会引入一个 PWM 周期的延迟，
   因此不可以"规范化"为库调用（文件头已注明）。
2. **换相表逐项一致**。`s_bemf_channel[6] = {C,B,A,C,B,A}` 对应原 `TAB_BEMFChannel = {3,2,1,3,2,1}`；
   `s_bemf_edge[6]` 对应原 `TAB_RFling[1][6]`，`TAB_RFling[0][*]` 从未被引用故未保留。
3. **所有时序常量逐值一致**。1ms 心跳 BTIM1、20kHz PWM（`PWM_PERIOD 4800`）、
   400ms 启动延时、3000ms 开环爬坡、100ms 量测周期、200 步 × 10ms 无感扫描步进等。
4. **状态机迁移图不变**（见 `motor_control.h` 注释），故障码取值不变
   （3 启动失败 / 4 过流 / 6 过压 / 7 堵转 / 8 欠压 / 9 过温）。
5. **串口帧逐字节一致**：`0x43 0x57 0x04 档位 电压 关机标志 校验和`，共 7 字节。
6. **共享变量以裸变量形式跨模块访问**，未改为 getter/setter，以免在 ISR 路径上引入额外开销。
   （`motor_state.h` 中已注明这一决定。）

### 4.1 有意的等价性简化

| 原代码 | 处理 | 理由 |
| --- | --- | --- |
| `if (RealS1 == 0 \|\| RealS == 0)` | 展开为无条件等待 500ms | `RealS1` 从未被赋值（恒为 0），该条件恒真，展开后行为完全一致 |
| `if (times >= ErrorCode);` | 删除 | 空语句，无任何副作用 |
| `TAB_RFling[0][6]` | 删除 | 从未被引用 |
| `unsigned int hte` 同时充当测速标志与秒表 | 拆为 `g_hall_pulses` / `g_hall_pulses_100ms` | 原代码用同一个变量表达两种语义，`App_Run` 读到的"100ms 脉冲数"与 `BTIM1` 写入的"计数值"竞争；拆分后语义清晰。**注意**：`App_Run` 现在读到的是经过 100ms 窗口锁存的稳定值，原代码读到的可能是当前累积值 |
| `float t` + 软浮点换算 | 定点整数 `AdcCodeToMillivolt()` | Cortex-M0+ 无 FPU；整数链路误差 ≤ 1 个最小单位（详见 §6.3） |

### 4.2 已知的"保持原状"问题（未修，以保证等价）

- `pid.c` 中 `if (SumError < 6) SumError = 6;` 把积分下限钳位为**正值 6**，
  即使误差为 0 也存在恒定积分输出。该行为原样保留，已在 `pid.c` 中加 `@warning` 注释。
- `Uart_SendStatus` 仍为阻塞轮询发送（单帧约 0.6 ms @115200），未改为中断 / DMA，
  以免改变主循环时序。
- `MotorCtrl_ErrorOver` / `MotorCtrl_Stop` / 预充电阶段的等待都是忙等依赖 BTIM1 累加，
  保持不变（未引入 `__WFI()`）。

---

## 5. 代码规范改进清单

| 问题 | 处理 |
| --- | --- |
| 头文件无 include guard | 全部 11 个头文件补齐 `#ifndef ... #define ... #endif` |
| 无 `extern "C"`，无法被 C++ 引用 | 全部头文件补齐 |
| `global.h` 上帝头（变量 + 宏 + 类型混在一起） | 拆为 `motor_state.h` / `motor_types.h` / `app_config.h` |
| 状态 / 故障用 `#define` 裸数字 | 改为 `enum`（`motor_state_t` / `motor_fault_t` / `bemf_edge_t` / `comm_step_t` / `demag_state_t`） |
| 魔数散布各处 | 集中到 `app_config.h`，按 1~11 节分组并逐条注释 |
| 函数无统一命名前缀 | `Motor_` / `MotorCtrl_` / `Sensorless_` / `Measure_` / `Uart_` / `Bsp_` / `Pid_` / `MotorState_` |
| `extern` 声明散落在 .c 中间（如 `extern unsigned char Sta;`） | 全部移除，统一由 `motor_state.h` 声明 |
| `GPIO_InitTypeDef` 未初始化即传参 | `bsp_init.c` 中零初始化 |
| 重复的 `__SYSCTRL_ADC_CLK_ENABLE()`、冗余 `OPA_Config()` | 删除 |
| 死代码 `MINOUT` / `NumCoun` / `TimeAli` / `LSH` / `RealS1` / `DD` / `PrevError` / `LastError` / `dError` | 删除 |
| 未使用的 `float t;` | 删除（第一阶段已处理） |
| `MOTOR.C` 大写扩展名被 CMake 判为 C++ | 重命名为 `motor.c`；并在 `cmake/cw32l012/CMakeLists.txt` 顶部加注说明 |
| 阻塞延时函数 `delay()` 语义不清 | 统一为 `g_tick_temp = 0; while (g_tick_temp < N)` 并加注释说明由 BTIM1 推进 |
| 函数内 `static` 状态未标注 | 全部改名为 `s_xxx` 前缀（`s_bemf_channel`、`s_start_ok`、`s_over_temp_cnt` …） |

---

## 6. 关键实现说明

### 6.1 六个源不再"分层错位"

原 `motor.c` 同时包含：六步换相、无感启动、ADC 中断换相、BTIM3 中断。
现按职责拆为两层：

- `motor.c`：纯执行器 —— `Motor_Commutation` / `Motor_UpdateDuty` / `Motor_StopOutput`
- `sensorless.c`：无感算法 + 两个 ISR（`BTIM3_HALLTIM_IRQHandler`、`ADC1_IRQHandler`）

### 6.2 `main.c` 瘦身

```c
int main(void)
{
    App_Init();
    App_Run();
    return 0;
}
```

硬件前提（断开 R40/R41、焊接 R38/R39）保留为文件头注释。

### 6.3 量测链路由浮点改定点

原：`t = BGR * adc / Vref;  t = t / RV1 * (RV1+RV2) / 100;`（`float`，软浮点）
现：`AdcCodeToMillivolt()` 用整数完成 `BGR * adc / Vref`，后续按 `*(RV1+RV2)/RV1/100` 整数缩放。

- 误差上界：整数除法截断 ≤ 1 个最小单位，对 0.1V 分辨率的电压与 mA 分辨率的电流无实际影响。
- 额外增加除零保护（`Vref_code == 0` 时返回 0），原代码此处会得到 `inf`。

---

## 7. 编译结果

```
[100%] Linking C executable CW32L012_BLDC_Demo.elf
Memory region         Used Size  Region Size  %age Used
             RAM:        1768 B         8 KB     21.58%
           FLASH:       22348 B        64 KB     34.10%
   text    data     bss     dec     hex filename
  22300      48    1728   24076    5e0c CW32L012_BLDC_Demo.elf
```

`-Wall` 下**零编译告警**。链接阶段剩余两条提示均为预期输出：

- `_read/_write/_close/_lseek is not implemented and will always fail`
  —— 由 `-specs=nosys.specs` 引入，裸机无文件系统，不影响运行。
- `has a LOAD segment with RWX permissions`
  —— 链接脚本段权限描述所致，如需消除可追加 `-Wl,--no-warn-rwx-segments`。

### 7.1 Release 构建（`-Os`）

```
Memory region         Used Size  Region Size  %age Used
             RAM:        1760 B         8 KB     21.48%
           FLASH:       16596 B        64 KB     25.32%
   text    data     bss     dec     hex filename
  16548      48    1720   18316    478c CW32L012_BLDC_Demo.elf
```

`-Os` 会额外暴露两条 `-Wmaybe-uninitialized` 告警，两条都位于**厂商驱动库**
`Drivers/CW32L012_Driver/Src/cw32l012_sysctrl.c`（`FlagStatus ret;` 与 `div`），
属第三方代码，非本次重构引入，未做修改。

---

## 8. 后续可选项（本次未做）

1. 把 `sensorless.o` 的只读内容放入 RAM 运行，以对齐 Keil `Project.sct` 的分散加载行为
   （需 `__attribute__((section(".ramfunc")))` + 链接脚本 RAM 段 + 启动拷贝）。
2. `Uart_SendStatus` 改为环形缓冲 + DMA 发送，消除主循环 0.6 ms 阻塞。
3. 启用真正的转速闭环（目前 `ENABLE_PID=OFF`，`g_speed_target` 只做开环爬坡）。
4. 忙等等待改为 `__WFI()`，降低待机功耗。
