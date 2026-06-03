#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f4xx_hal.h"
#include "ringbuffer.h"
#include <string.h>
#include <stdio.h>

/** @brief DMA 接收缓冲区大小 */
#define ESP8266_RX_BUF_SIZE 1024

/** @brief AT 指令回复检查缓冲区大小（只需匹配 "OK" 等短词） */
#define AT_REPLY_BUF   128

extern ring_buffer_t esp_rb;

extern volatile uint16_t esp8266_rx_len;
extern volatile uint8_t esp8266_rx_flag;

void ESP8266_Init(void);
void ESP8266_ClearBuf(void);
uint8_t ESP8266_SendCmd(char *cmd, char *ack, uint32_t timeout);
void ESP8266_SendData(uint8_t *data, uint16_t len);

// 提供给应用层的安全帧读取接口
uint16_t ESP8266_ReadFrame(uint8_t *buf, uint16_t max_len);

#endif /* __ESP8266_H */
