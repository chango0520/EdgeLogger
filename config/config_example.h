#ifndef CONFIG_EXAMPLE_H
#define CONFIG_EXAMPLE_H

/* WIFI */
#define WIFI_SSID           "wifi_ssid"
#define WIFI_PASSWORD       "password"

/* OneNET */
#define ONENET_CLIENT_ID    "test"                                          //此处填设备名称
#define ONENET_USERNAME     "123"                                    //此处填设备ID
#define ONENET_PASSWORD     "123456" //此处填设备Token

/* 平台连接参数 */
#define ONENET_MQTT_BROKER  "mqtts.heclouds.com"
#define ONENET_MQTT_PORT    1883

/* 订阅与发布 Topic */
#define ONENET_TOPIC_SUB_REPLY  "$sys/"ONENET_USERNAME"/"ONENET_CLIENT_ID"/thing/property/post/reply"
#define ONENET_TOPIC_PUB_POST   "$sys/"ONENET_USERNAME"/"ONENET_CLIENT_ID"/thing/property/post"


#endif
