#include "app_mqtt.h"
#include "esp8266.h"
#include "config.h"
#include "sht30.h"

bool App_MQTT_Init(void)
{
    printf("[MQTT] Initializing ESP8266 on USART1...\r\n");
    ESP8266_Init();

    char cmd_buf[350];

		printf("[MQTT] Restarting...\r\n");
    if (ESP8266_SendCmd("AT+RST\r\n", "OK", 3000) != 0) return false;
    HAL_Delay(3000);
	
		printf("[MQTT] Setting Mode...\r\n");
    if (ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK", 3000) != 0) return false;
	
    printf("[MQTT] Connecting to WiFi...\r\n");
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+CWJAP=\"%s\",\"%s\"\r\n",
             WIFI_SSID, WIFI_PASSWORD);
    if (ESP8266_SendCmd(cmd_buf, "OK", 3000) != 0) return false;

    printf("[MQTT] Configuring MQTT Client...\r\n");
    snprintf(cmd_buf, sizeof(cmd_buf),
             "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n",
             ONENET_CLIENT_ID, ONENET_USERNAME, ONENET_PASSWORD);
    if (ESP8266_SendCmd(cmd_buf, "OK", 3000) != 0) return false;

    printf("[MQTT] Connecting to OneNET Broker...\r\n");
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+MQTTCONN=0,\"%s\",%d,1\r\n",
             ONENET_MQTT_BROKER, ONENET_MQTT_PORT);
    if (ESP8266_SendCmd(cmd_buf, "OK", 10000) != 0) return false;

    printf("[MQTT] Subscribing to Reply Topic...\r\n");
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+MQTTSUB=0,\"%s\",1\r\n",
             ONENET_TOPIC_SUB_REPLY);
    if (ESP8266_SendCmd(cmd_buf, "OK", 3000) != 0) return false;

    // 清除可能残留的标志
    ESP8266_ClearBuf();
    return true;
}

void App_MQTT_Publish_SensorData(float temp, float hum)
{
    char payload[150];
    char cmd[300];

    snprintf(payload, sizeof(payload),
             "{\\\"id\\\":\\\"123\\\"\\,\\\"version\\\":\\\"1.0\\\"\\,"
             "\\\"params\\\":{\\\"temperature\\\":{\\\"value\\\":%.1f}\\,"
             "\\\"humidity\\\":{\\\"value\\\":%.1f}}}",
             temp, hum);

    snprintf(cmd, sizeof(cmd), "AT+MQTTPUB=0,\"%s\",\"%s\",0,0\r\n",
             ONENET_TOPIC_PUB_POST, payload);

    ESP8266_SendData((uint8_t *)cmd, strlen(cmd));
    printf("[MQTT] Publish -> T: %.1f C, H: %.1f %%\r\n", temp, hum);
}

/**
 * @brief  MQTT 任务：处理云端主动推送的数据
 */
void App_MQTT_Task(void)
{
    uint8_t frame[ESP8266_RX_BUF_SIZE];
    uint16_t len = ESP8266_ReadFrame(frame, sizeof(frame));

    if (len > 0)
    {
        // 确保字符串结束
        if (len < sizeof(frame))
            frame[len] = '\0';
        else
            frame[sizeof(frame) - 1] = '\0';

        if (strstr((char *)frame, "+MQTTSUBRECV") != NULL)
        {
            printf("[MQTT] Receive Cloud Cmd: %s\r\n", frame);
            // TODO: 解析 JSON 并执行控制指令
        }
        else if (strstr((char *)frame, "CLOSED") != NULL)
        {
            printf("[MQTT] Disconnected from Cloud! Rebooting...\r\n");
            NVIC_SystemReset();
        }
    }
}

