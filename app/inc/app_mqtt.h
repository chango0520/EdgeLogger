/**
 * @file    app_mqtt.h
 * @brief   MQTT 连接管理 & OneNet 数据上报应用层接口
 * @note    基于 ESP8266 AT 指令 + FreeRTOS 任务实现。
 *          所有 MQTT 状态封装在模块内部，外部通过本接口查询和控制。
 *
 *          使用流程：
 *          MQTTTask 入口 → App_MQTT_Init() 连接 WiFi 和 OneNet
 *                       → 循环调用 App_MQTT_ProcessIncoming()
 *                       → 每 5s 调用 App_MQTT_Publish_SensorData()
 */

#ifndef __APP_MQTT_H
#define __APP_MQTT_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/* ========== 发布/重连周期常量（唯一源头，供 freertos.c 引用） ========== */

/** @brief MQTT 数据发布间隔（毫秒） */
#define PUBLISH_INTERVAL_MS      5000

/** @brief MQTT 连接断开后重试间隔（毫秒） */
#define RECONNECT_DELAY_MS       5000

/** @brief MQTT 任务主循环延时（毫秒）—— 兼顾响应速度与 CPU 占用 */
#define TASK_LOOP_DELAY_MS       100

/* ========== API 函数声明 ========== */

/**
 * @brief  初始化 ESP8266 并连接 WiFi / OneNet MQTT 服务器
 * @retval true  = 初始化成功（已连接且已订阅回复 Topic）
 * @retval false = 初始化失败（可等待后重试）
 * @note   此函数会阻塞数秒（WiFi 连网 + MQTT 建连），
 *          必须在 FreeRTOS 任务中调用，切勿在调度器启动前调用
 */
bool App_MQTT_Init(void);

/**
 * @brief  检查 MQTT 是否处于已连接状态
 * @retval true  = 已连接到 OneNet Broker
 * @retval false = 未连接或连接已断开
 */
bool App_MQTT_IsConnected(void);

/**
 * @brief  尝试重新连接 WiFi + MQTT（完整重连序列）
 * @retval true  = 重连成功
 * @retval false = 重连失败（调用方可通过 App_MQTT_IsConnected() 轮询）
 */
bool App_MQTT_Reconnect(void);

/**
 * @brief  发布传感器数据到 OneNet 物模型主题
 * @param  temp : 温度（°C）
 * @param  hum  : 湿度（%RH）
 * @note   使用 AT+MQTTPUB 指令，QoS=0，非阻塞发送
 */
void App_MQTT_Publish_SensorData(float temp, float hum);

/**
 * @brief  处理 ESP8266 接收到的下行数据
 * @note   由 MQTT 任务每 100ms 调用一次，检查环形缓冲区中的帧，
 *         识别 +MQTTSUBRECV（云端命令）和 CLOSED/断开事件
 */
void App_MQTT_ProcessIncoming(void);

#endif /* __APP_MQTT_H */
