#ifndef __APP_MQTT_H
#define __APP_MQTT_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

bool App_MQTT_Init(void);
void App_MQTT_Task(void);
void App_MQTT_Publish_SensorData(float temp, float hum);
#endif 
