/**
 * @file    app_mqtt.c
 * @brief   MQTT 连接管理 & OneNet 数据上报应用层实现
 *
 * @note    基于 ESP8266 AT 指令 + MQTT AT 扩展指令集。
 *          整个模块设计在 MQTTTask 中运行：
 *
 *          1. 任务启动 → App_MQTT_Init() 连 WiFi + 连 OneNet
 *          2. 主循环：
 *             a. App_MQTT_ProcessIncoming() — 处理云端下行消息
 *             b. 每 5s 读取传感器数据并发布
 *             c. 检测到连接断开时自动重连
 *
 * @硬件依赖 USART1 (DMA+IDLE 中断接收) → ESP8266
 *          USART2 (printf 重定向，调试输出)
 *
 * @设计要点
 *         - App_MQTT_Init() 与重连共享同一连接序列，消除 ~90% 重复代码
 *         - mqtt_connected 状态封装在模块内部，外部通过 App_MQTT_IsConnected() 查询
 */

#include "app_mqtt.h"       /* 本模块头文件 */
#include "esp8266.h"         /* ESP8266 驱动（DMA+环形缓冲区） */
#include "config.h"          /* WiFi / OneNet 凭据 */
#include "app_sensor.h"      /* 获取传感器数据 */
#include "cmsis_os.h"        /* FreeRTOS CMSIS-RTOS v2 API: osDelay */

/* ========== 内部常量 ========== */

/** @brief AT 命令缓冲区大小 —— 足以容纳最长命令（含 MQTT PUB JSON payload） */
#define CMD_BUF_SIZE   350

/* ========== 模块内部状态 ========== */

/** @brief MQTT Broker 连接状态（true=已连接；false=断开）—— 仅本文件读写 */
static bool mqtt_connected = false;

/* ========== 内部辅助函数 ========== */

/**
 * @brief  执行 AT 指令连接序列（共 6 步，不含 ESP8266_Init）
 * @note   本函数被 App_MQTT_Init() 和 App_MQTT_Reconnect() 共用。
 *         不包含 ESP8266_Init() 调用，由调用方按需前置。
 * @retval true  = 全部 6 步成功
 * @retval false = 任意一步失败
 */
static bool mqtt_at_connect_sequence(void)
{
    char cmd[CMD_BUF_SIZE];

    /* ---- 第 1 步：复位 ESP8266 模块 ---- */
    printf("[MQTT] Step 1/6: AT+RST...\r\n");
    ESP8266_ClearBuf();
    if (ESP8266_SendCmd("AT+RST\r\n", "OK", 3000) != 0) {
        printf("[MQTT] FAIL: AT+RST no response\r\n");
        return false;
    }
    osDelay(3000);  /* 等待模块重启完成（数据手册要求） */

    /* ---- 第 2 步：设置 Station 模式 ---- */
    printf("[MQTT] Step 2/6: AT+CWMODE=1...\r\n");
    if (ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK", 2000) != 0) {
        printf("[MQTT] FAIL: AT+CWMODE\r\n");
        return false;
    }

    /* ---- 第 3 步：连接 WiFi（超时 15 秒，确保路由器响应） ---- */
    printf("[MQTT] Step 3/6: Joining WiFi [%s]...\r\n", WIFI_SSID);
    snprintf(cmd, sizeof(cmd),
             "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASSWORD);
    if (ESP8266_SendCmd(cmd, "OK", 15000) != 0) {
        printf("[MQTT] FAIL: WiFi timeout/rejected\r\n");
        return false;
    }

    /* ---- 第 4 步：配置 MQTT 客户端身份 ---- */
    printf("[MQTT] Step 4/6: AT+MQTTUSERCFG...\r\n");
    snprintf(cmd, sizeof(cmd),
             "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n",
             ONENET_CLIENT_ID, ONENET_USERNAME, ONENET_PASSWORD);
    if (ESP8266_SendCmd(cmd, "OK", 3000) != 0) {
        printf("[MQTT] FAIL: AT+MQTTUSERCFG\r\n");
        return false;
    }

    /* ---- 第 5 步：连接到 OneNET MQTT Broker ---- */
    printf("[MQTT] Step 5/6: AT+MQTTCONN [%s:%d]...\r\n",
           ONENET_MQTT_BROKER, ONENET_MQTT_PORT);
    snprintf(cmd, sizeof(cmd),
             "AT+MQTTCONN=0,\"%s\",%d,1\r\n",
             ONENET_MQTT_BROKER, ONENET_MQTT_PORT);
    if (ESP8266_SendCmd(cmd, "OK", 10000) != 0) {
        printf("[MQTT] FAIL: AT+MQTTCONN (check broker/token)\r\n");
        return false;
    }

    /* ---- 第 6 步：订阅云端回复 Topic（失败不致命，仅打日志） ---- */
    printf("[MQTT] Step 6/6: AT+MQTTSUB...\r\n");
    snprintf(cmd, sizeof(cmd),
             "AT+MQTTSUB=0,\"%s\",1\r\n", ONENET_TOPIC_SUB_REPLY);
    if (ESP8266_SendCmd(cmd, "OK", 3000) != 0) {
        printf("[MQTT] WARN: AT+MQTTSUB failed (will retry on reconnect)\r\n");
    }

    ESP8266_ClearBuf();
    return true;
}

/* ========== API 实现 ========== */

/**
 * @brief  初始化 ESP8266 驱动 + 执行完整连接序列
 * @retval true  = 成功
 * @retval false = 失败（调用方应延时重试）
 * @note   阻塞约 15-30 秒，必须在 FreeRTOS 任务中调用
 */
bool App_MQTT_Init(void)
{
    printf("[MQTT] ===== MQTT over ESP8266 Initialization =====\r\n");

    /* 1. 初始化 ESP8266 驱动（启动 DMA+IDLE 接收） */
    ESP8266_Init();

    /* 2. 执行共用连接序列（复位 → 连 WiFi → MQTT 建连 → 订阅） */
    if (!mqtt_at_connect_sequence()) {
        return false;
    }

    mqtt_connected = true;
    printf("[MQTT] ===== Initialization Complete =====\r\n");
    return true;
}

/**
 * @brief  查询 MQTT 连接状态
 * @retval true  = 已连接到 OneNet Broker
 * @retval false = 未连接 / 已断开
 */
bool App_MQTT_IsConnected(void)
{
    return mqtt_connected;
}

/**
 * @brief  完整重连序列（不含 ESP8266_Init，保留 UART 驱动状态）
 * @retval true  = 重连成功
 * @retval false = 重连失败
 * @note   由 MQTTTask 检测到断开后调用，内部复用 mqtt_at_connect_sequence()
 */
bool App_MQTT_Reconnect(void)
{
    printf("[MQTT] Attempting full reconnect...\r\n");

    if (mqtt_at_connect_sequence()) {
        mqtt_connected = true;
        printf("[MQTT] Reconnect successful!\r\n");
        return true;
    }

    printf("[MQTT] Reconnect failed\r\n");
    return false;
}

/**
 * @brief  发布传感器数据到 OneNet 物模型主题
 * @param  temp : 温度（°C）
 * @param  hum  : 湿度（%RH）
 * @note   OneNet 物模型 JSON 格式，AT 指令中需转义 " 和 ,
 *         使用 QoS=0，retain=0，非阻塞发送
 */
void App_MQTT_Publish_SensorData(float temp, float hum)
{
    char payload[200];
    char cmd[CMD_BUF_SIZE];

    /*
     * OneNet 标准物模型属性上报格式。
     * AT 指令中双引号需要 \" 转义，逗号也需要转义为 \, 以防止解析为参数分隔符。
     */
    snprintf(payload, sizeof(payload),
             "{\\\"id\\\":\\\"123\\\"\\,"
             "\\\"version\\\":\\\"1.0\\\"\\,"
             "\\\"params\\\":{"
             "\\\"temperature\\\":{\\\"value\\\":%.1f}\\,"
             "\\\"humidity\\\":{\\\"value\\\":%.1f}"
             "}}",
             temp, hum);

    snprintf(cmd, sizeof(cmd),
             "AT+MQTTPUB=0,\"%s\",\"%s\",0,0\r\n",
             ONENET_TOPIC_PUB_POST, payload);

    /* 非阻塞发送：仅写入 UART 发送寄存器，不等待应答 */
    ESP8266_SendData((uint8_t *)cmd, strlen(cmd));
    printf("[MQTT] Published => T: %.1f C, H: %.1f %%\r\n", temp, hum);
}

/**
 * @brief  处理 ESP8266 接收到的下行数据
 * @note   由 MQTTTask 每 100ms 调用一次，检测两种帧类型：
 *         - +MQTTSUBRECV: 云端下发的属性回复/命令
 *         - +MQTTDISCONNECTED / CLOSED: MQTT 连接断开
 */
void App_MQTT_ProcessIncoming(void)
{
    uint8_t frame[AT_REPLY_BUF];
    uint16_t len;

    /* 非阻塞读一帧（没有数据则立即返回） */
    len = ESP8266_ReadFrame(frame, sizeof(frame));
    if (len == 0) return;

    /* 确保字符串安全终止 */
    if (len < sizeof(frame))
        frame[len] = '\0';
    else
        frame[sizeof(frame) - 1] = '\0';

    /* ---- 按帧类型分发 ---- */

    if (strstr((char *)frame, "+MQTTSUBRECV") != NULL) {
        /* 收到云端下发的属性回复 或 远程命令 */
        printf("[MQTT] <<< Cloud: %s\r\n", frame);
        /* TODO: 可在此解析 JSON 实现远程控制（如 LED 开关） */
    }
    else if (strstr((char *)frame, "+MQTTDISCONNECTED") != NULL
          || strstr((char *)frame, "CLOSED") != NULL) {
        /* MQTT 连接被远端关闭 —— 由主循环触发重连 */
        printf("[MQTT] Connection lost! (%s)\r\n", frame);
        mqtt_connected = false;
    }
}
