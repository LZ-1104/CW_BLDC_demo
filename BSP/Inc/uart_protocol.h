/**
  ******************************************************************************
  * @file    uart_protocol.h
  * @brief   调试串口上报（取代原 init.c 中的 SendDataToUsart）。
  ******************************************************************************
  */

#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  发送一帧状态数据。
 *
 *         帧格式（7 字节）：
 *           [0] 0x43 'C'
 *           [1] 0x57 'W'
 *           [2] 0x04        类型
 *           [3] 转速档位
 *           [4] 母线电压（0.1V 单位的低 8 位）
 *           [5] 关机标志
 *           [6] 前 6 字节之和的低 8 位
 *
 * @note   本函数按字节轮询发送（等待 TXE），单帧约 0.6ms @115200。
 *         由于原本就在主循环中调用，未改为中断/DMA 发送，以保持时序不变。
 */
void Uart_SendStatus(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_PROTOCOL_H */
