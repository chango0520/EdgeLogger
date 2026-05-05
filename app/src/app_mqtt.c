#include "app_mqtt.h"
#include "esp8266.h"
#include "config.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

/* 全局DMA发送缓存，防止栈溢出或局部变量生命周期结束 */
static uint8_t mqtt_tx_buf[ESP8266_TX_MAX_LEN];

/**
 * @brief  初始化WiFi并连接OneNET平台 (阻塞型，仅在系统启动时调用)
 * @retval true: 连接成功 / false: 连接失败
 */
bool App_MQTT_Init(void)
{
		ESP8266_Init();
	
    char cmd_buf[350]; // Password 较长，分配足够大的缓存

    printf("[APP_MQTT] Resetting ESP8266...\r\n");
    ESP8266_SendCmd_Block("AT+RST\r\n", "ready", 3000);
    HAL_Delay(1000);

    /* 1. 设置 WiFi STA 模式 */
    if (!ESP8266_SendCmd_Block("AT+CWMODE=1\r\n", "OK", 1000)) return false;

    /* 2. 连接 WiFi */
    printf("[APP_MQTT] Connecting to WiFi...\r\n");
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASSWORD);
    if (!ESP8266_SendCmd_Block(cmd_buf, "WIFI GOT IP", 3000)) return false;

    /* 3. 配置 MQTT 用户鉴权参数 */
    printf("[APP_MQTT] Configuring MQTT Client...\r\n");
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n", 
             ONENET_CLIENT_ID, ONENET_USERNAME, ONENET_PASSWORD);
    if (!ESP8266_SendCmd_Block(cmd_buf, "OK", 3000)) return false;

    /* 4. 连接 OneNET Broker */
    printf("[APP_MQTT] Connecting to OneNET Broker...\r\n");
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+MQTTCONN=0,\"%s\",%d,1\r\n", 
             ONENET_MQTT_BROKER, ONENET_MQTT_PORT);
    if (!ESP8266_SendCmd_Block(cmd_buf, "OK", 10000)) return false;

    /* 5. 订阅物模型响应 Topic */
    printf("[APP_MQTT] Subscribing to Reply Topic...\r\n");
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+MQTTSUB=0,\"%s\",1\r\n", ONENET_TOPIC_SUB_REPLY);
    if (!ESP8266_SendCmd_Block(cmd_buf, "OK", 3000)) return false;

    printf("[APP_MQTT] OneNET Initialization Completed!\r\n");
    
    /* 恢复DMA接收状态，准备进入轮询非阻塞模式 */
    esp8266_rx_flag = false;
    esp8266_rx_len = 0;
    HAL_UART_Receive_DMA(&huart1, esp8266_rx_buf, ESP8266_RX_MAX_LEN);
    
    return true;
}

/**
 * @brief  封装并发布SHT30温湿度数据至OneNET物模型 (非阻塞DMA发送)
 * @param  temp: SHT30解析出的温度值
 * @param  hum:  SHT30解析出的湿度值
 */
void App_MQTT_Publish_SensorData(float temp, float hum)
{
    char payload[150];
    char cmd[300];

    /* 1. 组装 OneNET 物模型 JSON 字符串 
     * 目标格式: {"id":"123","version":"1.0","params":{"temp":{"value":25.5},"hum":{"value":60.0}}}
     * 避坑：AT指令发送双引号需转义为 \\\"，部分固件逗号需转义为 \\,
     */
    snprintf(payload, sizeof(payload), 
             "{\\\"id\\\":\\\"123\\\"\\,\\\"version\\\":\\\"1.0\\\"\\,\\\"params\\\":{\\\"temperature\\\":{\\\"value\\\":%.1f}\\,\\\"humidity\\\":{\\\"value\\\":%.1f}}}", 
             temp, hum);

    /* 2. 组装 AT+MQTTPUB 指令 */
    snprintf(cmd, sizeof(cmd), "AT+MQTTPUB=0,\"%s\",\"%s\",0,0\r\n", 
             ONENET_TOPIC_PUB_POST, payload);

    /* 3. 安全获取DMA发送锁 */
    uint32_t wait_tick = HAL_GetTick();
    while(!esp8266_tx_ready)
    {
        if(HAL_GetTick() - wait_tick > 200) {  // 200ms超时防御
            HAL_UART_AbortTransmit(&huart1);
            esp8266_tx_ready = true;
            break;
        }
    }

    /* 4. 拷贝到全局缓冲区并启动DMA传输 */
    uint16_t len = strlen(cmd);
    if(len < ESP8266_TX_MAX_LEN)
    {
        memcpy(mqtt_tx_buf, cmd, len);
        ESP8266_Send_DMA(mqtt_tx_buf, len);
        
        /* 调试打印：监控上传的数据 */
        printf("[APP_MQTT] Publish -> T: %.1f C, H: %.1f %%\r\n", temp, hum);
    }
}

/**
 * @brief  MQTT 接收轮询任务，处理平台下发命令或心跳保活
 * @note   需放置在 main() 的 while(1) 循环中调用
 */
void App_MQTT_Task(void)
{
    /* 检查是否触发串口空闲中断并接收到完整数据帧 */
    if (esp8266_rx_flag)
    {
        /* 封堵字符串尾部，防止越界访问 */
        esp8266_rx_buf[esp8266_rx_len] = '\0';
        
        /* 解析订阅下发报文: 格式 +MQTTSUBRECV:0,"topic",length,payload */
        if (strstr((char*)esp8266_rx_buf, "+MQTTSUBRECV") != NULL)
        {
            printf("[APP_MQTT] Receive Cloud Cmd: %s\r\n", esp8266_rx_buf);
            /* TODO: 在此调用 JSON 解析库（如 cJSON）处理远程控制逻辑 */
        }
        else if (strstr((char*)esp8266_rx_buf, "CLOSED") != NULL)
        {
            printf("[APP_MQTT] Disconnected from Cloud! Rebooting...\r\n");
            /* 错误处理：触发重新连接机制或硬件复位 */
            NVIC_SystemReset(); 
        }
        
        /* 清空标志位，重启DMA准备接收下一帧 */
        esp8266_rx_flag = false;
        esp8266_rx_len = 0;
        HAL_UART_Receive_DMA(&huart1, esp8266_rx_buf, ESP8266_RX_MAX_LEN);
    }
}
