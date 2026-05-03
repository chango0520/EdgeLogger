#include "app_sensor.h"
#include "sht30.h"      /* 底层驱动 */
#include "i2c.h"        /* hi2c1句柄 */
#include <stdio.h>      /* printf 用于 调试 */

/* 宏定义：传感器采集周期 (单位：毫秒)。建议2秒，SHT30具有自发热特性，高频读取会使温度偏高 */
#define SENSOR_READ_INTERVAL_MS 2000

/* 静态全局变量定义 (局部作用域) */
static SHT30_HandleTypeDef sht30_dev;
static float sys_temperature = 0.0f;
static float sys_humidity = 0.0f;
static bool data_valid = false;

/**
 * @brief 传感器应用层初始化
 */
void App_Sensor_Init(void)
{
    printf("[Sensor] Initializing SHT30 on I2C1...\r\n");
    
    /* 1. 初始化 SHT30 句柄
     * 根据芯片手册与硬件分配：I2C1(PB6/PB7)
     * ADDR 默认接 GND (0x44)
     */
    SHT30_Init(&sht30_dev, &hi2c1, SHT30_ADDR_GND);
    
    /* 2. 执行一次初始读取，确认总线与模块物理连接状态 */
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
 * @brief 传感器业务轮询任务，需在 main.c 的 while(1) 中循环调用
 */
void App_Sensor_Task(void)
{
    static uint32_t last_tick = 0;
    uint32_t current_tick = HAL_GetTick();

    /* 时间片轮询：到达规定周期后执行采集 */
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
            /* 若总线断开或 CRC 校验失败，标记数据无效 */
            data_valid = false;
            printf("[Sensor] Read Error! Data invalid.\r\n");
        }
    }
}

/**
 * @brief 获取最新有效的温湿度数据
 * @param temp 指向存储温度的指针
 * @param humi 指向存储湿度的指针
 * @return true 数据有效 / false 数据无效或总线异常
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
