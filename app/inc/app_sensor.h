/**
 * @file    app_sensor.h
 * @brief   SHT30 温湿度传感器应用层接口
 * @note    提供线程安全的传感器数据访问（互斥锁保护）。
 *          SensorTask 每 2 秒采集一次，其他任务通过 App_Sensor_GetData() 安全读取。
 */

#ifndef __APP_SENSOR_H
#define __APP_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

/* ========== 常量（唯一源头，供 freertos.c 引用） ========== */

/** @brief 传感器采集间隔（毫秒） */
#define SENSOR_READ_INTERVAL_MS  2000

/* ========== 应用层接口 ========== */

/**
 * @brief  初始化 SHT30 传感器及内部互斥锁
 * @note   必须在 FreeRTOS 调度器启动后调用（通常在 SensorTask 入口执行）
 *         函数内部会创建互斥锁并执行一次传感器自检
 */
void App_Sensor_Init(void);

/**
 * @brief  采集一次温湿度并更新内部共享变量
 * @note   由 SensorTask 每 2 秒循环调用
 *         内部使用互斥锁保护，可安全与 App_Sensor_GetData() 并发
 */
void App_Sensor_Update(void);

/**
 * @brief  获取最新温湿度数据（线程安全）
 * @param  temp : [输出] 温度值（°C），允许传入 NULL
 * @param  humi : [输出] 湿度值（%RH），允许传入 NULL
 * @return true  = 数据有效；false = 数据无效（传感器故障）
 */
bool App_Sensor_GetData(float *temp, float *humi);

#endif /* __APP_SENSOR_H */
