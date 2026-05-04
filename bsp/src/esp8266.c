#include "esp8266.h"
#include "usart.h"
#include <string.h>

uint8_t esp8266_rx_buf[ESP8266_RX_MAX_LEN];
volatile uint16_t esp8266_rx_len = 0;
volatile bool esp8266_rx_flag = false;
volatile bool esp8266_tx_ready = true; 

/**
 * @brief  初始化ESP8266底层串口DMA和中断
 */
void ESP8266_Init(void)
{
    esp8266_tx_ready = true;
    esp8266_rx_flag = false;
    esp8266_rx_len = 0;
    memset(esp8266_rx_buf, 0, ESP8266_RX_MAX_LEN);

    /* 开启串口DMA接收 */
    HAL_UART_Receive_DMA(&huart1, esp8266_rx_buf, ESP8266_RX_MAX_LEN);
    /* 使能USART1空闲中断 */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
}

/**
 * @brief  阻塞式发送AT指令并等待特定应答 (适用于初始化阶段)
 * @param  cmd: AT指令字符串
 * @param  ack: 期望的应答字符串 (如 "OK")
 * @param  timeout_ms: 超时时间
 * @retval true: 成功收到应答 / false: 超时或失败
 */
bool ESP8266_SendCmd_Block(const char *cmd, const char *ack, uint32_t timeout_ms)
{
    /* 确保上一次DMA发送完成 */
    uint32_t wait_tick = HAL_GetTick();
    while(!esp8266_tx_ready) {
        if(HAL_GetTick() - wait_tick > 500) {
            HAL_UART_AbortTransmit(&huart1);
            esp8266_tx_ready = true;
        }
    }

    esp8266_rx_flag = false;
    esp8266_rx_len = 0;
    memset(esp8266_rx_buf, 0, ESP8266_RX_MAX_LEN);

    /* 重新开启DMA接收，准备接收应答 */
    HAL_UART_Receive_DMA(&huart1, esp8266_rx_buf, ESP8266_RX_MAX_LEN);

    /* 使用轮询方式发送指令，确保指令发完 */
    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);

    /* 等待接收应答 */
    uint32_t start_tick = HAL_GetTick();
    while((HAL_GetTick() - start_tick) < timeout_ms)
    {
        if(esp8266_rx_flag)
        {
            esp8266_rx_buf[esp8266_rx_len] = '\0'; // 确保字符串结束符
            if(strstr((char *)esp8266_rx_buf, ack) != NULL)
            {
                esp8266_rx_flag = false;
                return true; 
            }
            /* 收到数据但不是期望的应答，继续清空并接收 */
            esp8266_rx_flag = false;
            HAL_UART_Receive_DMA(&huart1, esp8266_rx_buf, ESP8266_RX_MAX_LEN);
        }
    }
    return false;
}

/**
 * @brief  DMA非阻塞发送数据 (适用于MQTT心跳、发布报文)
 */
HAL_StatusTypeDef ESP8266_Send_DMA(uint8_t *pData, uint16_t Size)
{
    if (!esp8266_tx_ready) return HAL_BUSY;
    
    esp8266_tx_ready = false; 
    return HAL_UART_Transmit_DMA(&huart1, pData, Size);
}

/**
 * @brief  ESP8266 串口空闲中断处理回调逻辑 (在stm32f4xx_it.c中调用)
 */
void ESP8266_IDLE_Callback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE) != RESET)
        {
            __HAL_UART_CLEAR_IDLEFLAG(huart);
            HAL_UART_DMAStop(huart);
            
            esp8266_rx_len = ESP8266_RX_MAX_LEN - __HAL_DMA_GET_COUNTER(huart->hdmarx);
            esp8266_rx_flag = true;
        }
    }
}

/**
 * @brief 串口发送完成回调函数 (由系统DMA2_Stream7中断底层调用)
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        esp8266_tx_ready = true; 
    }
}
