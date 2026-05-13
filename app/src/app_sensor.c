#include "app_sensor.h"
#include "sht30.h"      /* 驱动 */
#include "i2c.h"        /* hi2c1 */
#include <stdio.h>      /* printf */

/* 获取温湿度时间间隔 */
#define SENSOR_READ_INTERVAL_MS 2000

/* 静态变量，仅限于该文件访问 */
static SHT30_HandleTypeDef sht30_dev;
static float sys_temperature = 0.0f;
static float sys_humidity = 0.0f;
static bool data_valid = false;

/**
 * @brief SHT30初始化
 */
void App_Sensor_Init(void)
{
    printf("[Sensor] Initializing SHT30 on I2C1...\r\n");
    
    SHT30_Init(&sht30_dev, &hi2c1, SHT30_ADDR);
    
    /* 尝试首次读取温湿度 */
    if (SHT30_ReadTempHum(&sht30_dev, &sys_temperature, &sys_humidity) == HAL_OK) 
    {
        data_valid = true;
        printf("[Sensor] Init Success. T: %.2f C, H: %.2f %%\r\n", sys_temperature, sys_humidity);
    } 
    else 
    {
        data_valid = false;
        printf("[Sensor] Init Failed! Check I2C bus pull-up or wiring.\r\n");
    }
}

/**
 * @brief 循环执行的任务函数，放在main.c的while(1)内
 */
void App_Sensor_Task(void)
{
    static uint32_t last_tick = 0;
    uint32_t current_tick = HAL_GetTick();

    /* 一定间隔采集温湿度 */
    if ((current_tick - last_tick) >= SENSOR_READ_INTERVAL_MS) 
    {
        last_tick = current_tick;
        
        if (SHT30_ReadTempHum(&sht30_dev, &sys_temperature, &sys_humidity) == HAL_OK) 
        {
            data_valid = true;
            printf("[Sensor] Update - T: %.2f C, H: %.2f %%\r\n", sys_temperature, sys_humidity);
        } 
        else 
        {
            /* 读取错误 */
            data_valid = false;
            printf("[Sensor] Read Error! Data invalid.\r\n");
        }
    }
}

/**
 * @brief 暴露给其他外设读取温湿度的接口
 * @param temp：温度
 * @param humi：湿度
 * @return true 获取成功 / false 获取失败
 */
bool App_Sensor_GetData(float *temp, float *humi)
{
    if (data_valid) 
    {
        if (temp != NULL) *temp = sys_temperature;
        if (humi != NULL) *humi = sys_humidity;
        return true;
    }
    return false;
}
