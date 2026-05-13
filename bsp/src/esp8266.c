#include "esp8266.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

// 内部 DMA 接收缓冲区（不对外暴露）
static uint8_t dma_rx_buf[ESP8266_RX_BUF_SIZE];

// 环形缓冲区实例
ring_buffer_t esp_rb;

// 帧状态
volatile uint16_t esp8266_rx_len = 0;
volatile uint8_t  esp8266_rx_flag = 0;

/**
 * @brief  ESP8266 初始化
 */
void ESP8266_Init(void)
{
    ring_buffer_init(&esp_rb);
    ESP8266_ClearBuf();
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, dma_rx_buf, ESP8266_RX_BUF_SIZE);
}

/**
 * @brief  清空接收缓冲区与帧状态
 */
void ESP8266_ClearBuf(void)
{
    ring_buffer_clear(&esp_rb);
    esp8266_rx_len = 0;
    esp8266_rx_flag = 0;
}

/**
 * @brief  底层阻塞发送
 */
void ESP8266_SendData(uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart1, data, len, HAL_MAX_DELAY);
}

/**
 * @brief  发送 AT 指令并等待期望应答（阻塞）
 */
uint8_t ESP8266_SendCmd(char *cmd, char *ack, uint32_t timeout)
{
    ESP8266_ClearBuf();
    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);

    if (ack == NULL) return 0;

    uint32_t tickstart = HAL_GetTick();
    uint8_t  frame[ESP8266_RX_BUF_SIZE];

    while ((HAL_GetTick() - tickstart) < timeout)
    {
        if (esp8266_rx_flag)
        {
            uint16_t len = esp8266_rx_len;
            esp8266_rx_flag = 0;            // 消费帧标志

            if (len > ESP8266_RX_BUF_SIZE)
                len = ESP8266_RX_BUF_SIZE;

            uint32_t n = ring_buffer_read_multi(&esp_rb, frame, len);

            if (n > 0)
            {
                // 确保字符串结束（防止 strstr 越界）
                if (n < ESP8266_RX_BUF_SIZE)
                    frame[n] = '\0';
                else
                    frame[ESP8266_RX_BUF_SIZE - 1] = '\0';
								
								printf("[ESP8266 RX] %s\r\n", (char *)frame);

                if (strstr((char *)frame, ack) != NULL)
                    return 0;
            }
        }
    }

    return 1;   // 超时
}

/**
 * @brief  应用层读取最近一帧数据（安全，消费数据）
 * @param  buf: 用户缓冲区
 * @param  max_len: 缓冲区大小
 * @return 实际读出的字节数（0 表示无数据或已被消费）
 */
uint16_t ESP8266_ReadFrame(uint8_t *buf, uint16_t max_len)
{
    if (!esp8266_rx_flag || buf == NULL || max_len == 0)
        return 0;

    uint16_t len = esp8266_rx_len;

    // 先清除标志，避免重复消费（后续即使中断更新也不影响本次读取）
    esp8266_rx_flag = 0;

    if (len > max_len)
        len = max_len;

    uint32_t n = ring_buffer_read_multi(&esp_rb, buf, len);
    return (uint16_t)n;
}

/**
 * @brief  UART 接收事件回调（IDLE/DMA 满中断）
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        // 将 DMA 数据写入环形缓冲区
        ring_buffer_write_multi(&esp_rb, dma_rx_buf, Size);

        // 记录帧长度与标志
        esp8266_rx_len = Size;
        esp8266_rx_flag = 1;

        // 重新启动 DMA+IDLE 接收
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, dma_rx_buf, ESP8266_RX_BUF_SIZE);
    }
}

/**
 * @brief  UART 在每次被噪声打死后能立即复活
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        // 重启 DMA 接收
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, dma_rx_buf, ESP8266_RX_BUF_SIZE);
    }
}

