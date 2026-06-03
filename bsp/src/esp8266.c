/**
 * @file    esp8266.c
 * @brief   ESP8266 AT 指令驱动（基于 UART DMA + IDLE 中断 + 环形缓冲区）
 *
 * @note    数据流：
 *          ESP8266 发送数据 → USART1 RX DMA → IDLE 中断触发回调
 *          → HAL_UARTEx_RxEventCallback() 写入环形缓冲区
 *          → ESP8266_ReadFrame() 供应用层安全读取
 *
 *          错误恢复：
 *          HAL_UART_ErrorCallback() 自动重启 DMA，避免一次噪声打死整个接收链路
 */

#include "esp8266.h"
#include "usart.h"
#include "cmsis_os.h"       /* FreeRTOS CMSIS-RTOS v2: osDelay */

/* ========== 模块内部变量 ========== */

/** @brief DMA 接收缓冲区（不对外暴露细节） */
static uint8_t dma_rx_buf[ESP8266_RX_BUF_SIZE];

/** @brief 环形缓冲区实例，接收和读取的中介 */
ring_buffer_t esp_rb;

/** @brief 最近一帧的数据长度（由回调更新，由读取函数消费） */
volatile uint16_t esp8266_rx_len = 0;

/** @brief 帧就绪标志：1=有新帧可读，0=已消费 */
volatile uint8_t  esp8266_rx_flag = 0;

/* ========== API 实现 ========== */

/**
 * @brief  初始化 ESP8266 驱动
 * @note   清空环形缓冲区和帧状态，启动 UART DMA+IDLE 接收
 *         必须在 FreeRTOS 调度器启动后调用（确保中断系统就绪）
 */
void ESP8266_Init(void)
{
    ring_buffer_init(&esp_rb);
    ESP8266_ClearBuf();
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, dma_rx_buf, ESP8266_RX_BUF_SIZE);
}

/**
 * @brief  清空接收缓冲区与帧状态
 * @note   每次发送 AT 指令前调用，确保不会读到上次的残留数据
 */
void ESP8266_ClearBuf(void)
{
    ring_buffer_clear(&esp_rb);
    esp8266_rx_len = 0;
    esp8266_rx_flag = 0;
}

/**
 * @brief  底层阻塞发送（将原始数据写入 UART）
 * @param  data : 待发送数据
 * @param  len  : 数据长度
 */
void ESP8266_SendData(uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart1, data, len, HAL_MAX_DELAY);
}

/**
 * @brief  发送 AT 指令并等待期望应答（阻塞，带超时）
 * @param  cmd     : AT 指令字符串，必须以 \r\n 结尾
 * @param  ack     : 期望的应答关键词（如 "OK"），NULL 表示不等待
 * @param  timeout : 超时时间（毫秒）
 * @retval 0 = 收到期望应答 / 无需等待；1 = 超时
 *
 * @note   本函数是阻塞的，但每轮循环调用 osDelay(1) 让出 CPU，
 *         不会饿死其他 FreeRTOS 任务。
 */
uint8_t ESP8266_SendCmd(char *cmd, char *ack, uint32_t timeout)
{
    ESP8266_ClearBuf();
    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);

    /* 不需要应答时直接返回 */
    if (ack == NULL) return 0;

    /*
     * 使用小栈缓冲区而非 1KB 数组。
     * 我们只需匹配 "OK"/"ERROR"/"FAIL"/"CLOSED" 等短关键词，
     * 128 字节绰绰有余，且节省约 896 字节任务栈空间。
     */
    uint8_t  reply[AT_REPLY_BUF];
    uint32_t tickstart = HAL_GetTick();

    while ((HAL_GetTick() - tickstart) < timeout)
    {
        /* 检查是否有新帧到达 */
        if (esp8266_rx_flag)
        {
            uint16_t len = esp8266_rx_len;
            esp8266_rx_flag = 0;            /* 消费帧标志 */

            if (len > AT_REPLY_BUF)
                len = AT_REPLY_BUF;

            uint32_t n = ring_buffer_read_multi(&esp_rb, reply, len);

            if (n > 0)
            {
                /* 确保字符串终止，防止 strstr 越界 */
                if (n < AT_REPLY_BUF)
                    reply[n] = '\0';
                else
                    reply[AT_REPLY_BUF - 1] = '\0';

                /* 检查是否包含期望的应答关键词 */
                if (strstr((char *)reply, ack) != NULL)
                    return 0;
            }
        }

        /* 让出 CPU —— 避免忙等待饿死其他任务 */
        osDelay(1);
    }

    return 1;   /* 超时 */
}

/**
 * @brief  应用层读取最近一帧数据（非阻塞，消费式读取）
 * @param  buf     : 用户缓冲区
 * @param  max_len : 缓冲区大小
 * @return 实际读出的字节数（0 表示无数据或已被消费）
 */
uint16_t ESP8266_ReadFrame(uint8_t *buf, uint16_t max_len)
{
    if (!esp8266_rx_flag || buf == NULL || max_len == 0)
        return 0;

    uint16_t len = esp8266_rx_len;

    /* 先清除标志，防止重复消费 */
    esp8266_rx_flag = 0;

    if (len > max_len)
        len = max_len;

    uint32_t n = ring_buffer_read_multi(&esp_rb, buf, len);
    return (uint16_t)n;
}

/* ========== HAL 回调 ========== */

/**
 * @brief  UART 接收事件回调（IDLE 中断 / DMA 接收完成）
 * @note   将 DMA 收到的数据写入环形缓冲区，然后重新启动接收
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        /* 将 DMA 数据写入环形缓冲区 */
        ring_buffer_write_multi(&esp_rb, dma_rx_buf, Size);

        /* 记录帧长度与就绪标志 */
        esp8266_rx_len = Size;
        esp8266_rx_flag = 1;

        /* 重新启动 DMA+IDLE 接收，准备接收下一帧 */
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, dma_rx_buf, ESP8266_RX_BUF_SIZE);
    }
}

/**
 * @brief  UART 错误回调（噪声/断线等导致 UART 错误时自动恢复）
 * @note   无需用户干预，自动重启 DMA 接收链路
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, dma_rx_buf, ESP8266_RX_BUF_SIZE);
    }
}
