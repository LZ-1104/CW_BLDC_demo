/**
  ******************************************************************************
  * @file    uart_protocol.c
  * @brief   调试串口上报（取代原 init.c 中的 SendDataToUsart）。
  *
  * @note    原实现把帧内容放在全局 SendData[10] 中，其中只有前 7 个元素被
  *          使用，且校验和是对 32bit 变量求和后再截断。
  *          此处改为局部 uint8_t 数组，求和结果按 8bit 自然回绕，
  *          数值与原实现完全一致（(sum mod 2^32) mod 256 == sum mod 256）。
  ******************************************************************************
  */

#include "uart_protocol.h"

#include <stdint.h>

#include "cw32l012.h"
#include "cw32l012_uart.h"

#include "app_config.h"
#include "motor_state.h"

void Uart_SendStatus(void)
{
    uint8_t frame[UART_FRAME_LEN];
    uint8_t i;

    frame[0] = UART_FRAME_HEAD0;
    frame[1] = UART_FRAME_HEAD1;
    frame[2] = UART_FRAME_TYPE;
    frame[3] = (uint8_t)g_speed_level;
    frame[4] = (uint8_t)g_bus_voltage_dv;
    frame[5] = (uint8_t)g_power_off;

    frame[UART_FRAME_LEN - 1U] = 0U;
    for (i = 0U; i < UART_FRAME_PAYLOAD_LEN; i++)
    {
        frame[UART_FRAME_LEN - 1U] += frame[i];
    }

    for (i = 0U; i < UART_FRAME_LEN; i++)
    {
        UART_SendData(DEBUG_UART, frame[i]);
        while (UART_GetFlagStatus(DEBUG_UART, UART_FLAG_TXE) == RESET)
        {
            /* 等待发送寄存器空 */
        }
    }
}
