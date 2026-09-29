# Drivers/CMSIS/Include

此目录用于存放 **ARM CMSIS Cortex-M 内核头文件**。

`Drivers/CW32L012_Driver/Inc/cw32l012.h` 中有如下包含：

```c
#include "core_cm0plus.h"
```

因此编译 CW32L012 工程必须能找到 `core_cm0plus.h`。请把 CMSIS 的 `Include` 目录内容拷贝到本目录，至少应包含：

| 文件 | 说明 |
| --- | --- |
| `core_cm0plus.h` | Cortex-M0+ 内核定义（必需） |
| `cmsis_gcc.h` | GCC 编译器抽象（CMSIS 5+） |
| `cmsis_compiler.h` | 编译器选择（CMSIS 5+） |
| `cmsis_version.h` | 版本号（CMSIS 5+） |
| `cmsis_armcc.h` | CMSIS 4.x 编译器抽象（旧版本需要） |

> 若所拷贝的是 CMSIS 4.x（含 `core_cmFunc.h` / `core_cmInstr.h`），请一并拷贝这些文件。

## 来源

- Keil MDK 安装包中的 `CMSIS\Core\Include` 目录
- ARM 官方仓库：<https://github.com/ARM-software/CMSIS_5> 的 `CMSIS/Core/Include`

## 其他方式

如果不想拷贝文件，也可以在配置时直接指定路径：

```bash
cmake --preset Debug -DCMSIS_INCLUDE_DIR="D:/path/to/CMSIS/Include"
```
