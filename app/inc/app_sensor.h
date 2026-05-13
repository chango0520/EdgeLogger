#ifndef __APP_SENSOR_H
#define __APP_SENSOR_H

#include <stdint.h>
#include <stdbool.h>


void App_Sensor_Init(void);
void App_Sensor_Task(void);

bool App_Sensor_GetData(float *temp, float *humi);

#endif 
