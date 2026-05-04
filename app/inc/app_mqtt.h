#ifndef __APP_MQTT_H
#define __APP_MQTT_H

#include "main.h"
#include <stdbool.h>

/* WIFI配置 */
#define WIFI_SSID           "1103"
#define WIFI_PASSWORD       "13880233049"

/* 鉴权三元组 */
#define ONENET_CLIENT_ID    "test"                                          // 对应 设备名称
#define ONENET_USERNAME     "5vsUPgQ778"                                    // 对应 产品ID
#define ONENET_PASSWORD     "version=2018-10-31&res=products%2F5vsUPgQ778&et=1840888016&method=sha1&sign=Yri%2F5VHaQJWLy0aVHs2%2FQofoP04%3D" // 对应 Token

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
