/*
********************************************************************************
*  CW32L012C8 (Cortex-M0+)  --  GNU ld 链接脚本
*
*  FLASH : 64 KiB @ 0x00000000
*  SRAM  :  8 KiB @ 0x20000000
*
*  与 Keil 工程 "BLDC CONTROL/Objects/Project.sct" 的存储布局对应。
*  (注：Keil 分散加载文件把 sensorless.o 的 RO 内容放入 RAM 运行，
*   如需同样的行为，请给相应函数添加 __attribute__((section(".ramfunc")))
*   并在下文中启用 .ramfunc 段与启动拷贝逻辑。)
********************************************************************************
*/

ENTRY(Reset_Handler)

/* 栈顶地址（SRAM 最高地址），供启动文件和向量表使用 */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* 堆 / 栈的最小保留空间 */
_Min_Heap_Size  = 0x200;   /* 512 Bytes */
_Min_Stack_Size = 0x400;   /* 1 KiB     */

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 8K
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 64K
}

SECTIONS
{
  /* 中断向量表，必须位于 FLASH 起始位置 */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* 代码 */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;
  } >FLASH

  /* 只读数据 */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab : {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >FLASH

  .ARM : {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >FLASH

  .preinit_array : {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .init_array : {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .fini_array : {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  /* .data 段在 FLASH 中的加载地址（启动时拷贝到 RAM） */
  _sidata = LOADADDR(.data);

  /* 已初始化数据：运行于 RAM，加载于 FLASH */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    __data_start__ = _sdata;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
    __data_end__ = _edata;
  } >RAM AT> FLASH

  /* 未初始化数据 */
  . = ALIGN(4);
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* 检查堆栈是否超出 RAM 剩余空间 */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /DISCARD/ :
  {
    libc.a  ( * )
    libm.a  ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
