# Core/Inc

此目录用于存放 **应用层头文件**。

当前工程的应用入口与中断服务程序均沿用原例程的实现，没有独立的头文件，因此本目录暂时为空。
`Core/Src` 下的源文件包括：

- `main.c` —— 应用程序入口（源自 `BLDC CONTROL/User/main.c`）
- `interrupts_cw32l012.c` —— 中断服务程序（源自 `BLDC CONTROL/User/interrupts_cw32l012.c`）
- `system_cw32l012.c` —— 系统初始化（源自 `Libraries/src/system_cw32l012.c`）

如需新增应用层头文件，放在本目录即可，构建时该目录已在 include 路径中。
