#ifndef __ESP8266_H
#define __ESP8266_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define ESP8266_RX_MAX_LEN 1024 
#define ESP8266_TX_MAX_LEN 512

extern uint8_t esp8266_rx_buf[ESP8266_RX_MAX_LEN];
extern volatile uint16_t esp8266_rx_len;
extern volatile bool esp8266_rx_flag;
extern volatile bool esp8266_tx_ready;

/* 硬件与中断初始化 */
void ESP8266_Init(void);
void ESP8266_IDLE_Callback(UART_HandleTypeDef *huart);

/* 核心发送接口 */
bool ESP8266_SendCmd_Block(const char *cmd, const char *ack, uint32_t timeout_ms);
HAL_StatusTypeDef ESP8266_Send_DMA(uint8_t *pData, uint16_t Size);

#endif 
