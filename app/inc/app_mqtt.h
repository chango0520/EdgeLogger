#ifndef __APP_MQTT_H
#define __APP_MQTT_H

#include "main.h"
#include <stdbool.h>

/* 平台连接参数 */
#define ONENET_MQTT_BROKER  "mqtts.heclouds.com"
#define ONENET_MQTT_PORT    1883

/* 订阅与发布 Topic */
#define ONENET_TOPIC_SUB_REPLY  "$sys/"ONENET_USERNAME"/"ONENET_CLIENT_ID"/thing/property/post/reply"
#define ONENET_TOPIC_PUB_POST   "$sys/"ONENET_USERNAME"/"ONENET_CLIENT_ID"/thing/property/post"

/* 外部接口声明 */
bool App_MQTT_Init(void);
void App_MQTT_Publish_SensorData(float temp, float hum);
void App_MQTT_Task(void);

#endif 
