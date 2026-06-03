/**
 * @file    app_sensor.c
 * @brief   SHT30 温湿度传感器应用层实现
 *
 * @note    本模块设计为在 FreeRTOS 任务中使用：
 *          SensorTask 入口 → App_Sensor_Init() → 循环 App_Sensor_Update()
 *          其他任务通过 App_Sensor_GetData() 线程安全地读取最新值
 *
 *          数据流：
 *          SensorTask (每 SENSOR_READ_INTERVAL_MS 采集)
 *              → 互斥锁保护 → 共享变量
 *              → MQTTTask (每 PUBLISH_INTERVAL_MS 发布)
 */

#include "app_sensor.h"     /* 本模块头文件 */
#include "sht30.h"           /* SHT30 底层驱动 */
#include "i2c.h"             /* hi2c1 句柄 */
#include "cmsis_os.h"        /* FreeRTOS CMSIS-RTOS v2 API */

/* ========== 模块内部变量 ========== */

/** @brief SHT30 设备句柄（静态分配，仅本文件访问） */
static SHT30_HandleTypeDef sht30_dev;

/** @brief 当前温度值（°C），由互斥锁保护 */
static float sys_temperature = 0.0f;

/** @brief 当前湿度值（%RH），由互斥锁保护 */
static float sys_humidity = 0.0f;

/** @brief 数据有效性标志，由互斥锁保护 */
static bool data_valid = false;

/** @brief 互斥锁句柄 — 保护上面的三个共享变量 */
static osMutexId_t sensor_mutex = NULL;

/** @brief 互斥锁属性配置（递归锁：同一任务可重复加锁） */
static const osMutexAttr_t sensor_mutex_attr = {
    .name      = "SensorMutex",
    .attr_bits = osMutexRecursive,
};

/* ========== 内部辅助函数 ========== */

/**
 * @brief  加锁共享数据（阻塞等待直至获得锁）
 * @return true = 成功，false = 失败
 */
static inline bool lock_sensor_data(void)
{
    return (osMutexAcquire(sensor_mutex, osWaitForever) == osOK);
}

/**
 * @brief  解锁共享数据
 */
static inline void unlock_sensor_data(void)
{
    osMutexRelease(sensor_mutex);
}

/* ========== 应用层接口实现 ========== */

/**
 * @brief  初始化 SHT30 传感器及互斥锁
 * @note   在调度器启动后调用（通常在 SensorTask 中首次执行）
 *         执行一次传感器自检并打印结果
 */
void App_Sensor_Init(void)
{
    printf("[Sensor] ===== SHT30 Initialization =====\r\n");

    /* ---- 1. 创建递归互斥锁 ---- */
    sensor_mutex = osMutexNew(&sensor_mutex_attr);
    if (sensor_mutex == NULL) {
        printf("[Sensor] ERROR: Failed to create mutex!\r\n");
        return;
    }

    /* ---- 2. 初始化 SHT30 硬件（I2C 地址 0x44） ---- */
    SHT30_Init(&sht30_dev, &hi2c1, SHT30_ADDR);

    /* ---- 3. 首次读取验证 —— 确认传感器工作正常 ---- */
    float t = 0.0f, h = 0.0f;
    HAL_StatusTypeDef status = SHT30_ReadTempHum(&sht30_dev, &t, &h);

    if (status == HAL_OK) {
        /* 加锁更新共享变量 */
        if (lock_sensor_data()) {
            sys_temperature = t;
            sys_humidity    = h;
            data_valid      = true;
            unlock_sensor_data();
        }
        printf("[Sensor] Init OK  =>  T: %.2f C, H: %.2f %%\r\n", t, h);
    } else {
        printf("[Sensor] Init FAILED! Check I2C wiring / pull-up resistors.\r\n");
    }
}

/**
 * @brief  采集一次温湿度并更新共享变量
 * @note   由 SensorTask 每 SENSOR_READ_INTERVAL_MS 调用一次
 *         内部自动加锁，可与 App_Sensor_GetData() 安全并发
 */
void App_Sensor_Update(void)
{
    float t = 0.0f, h = 0.0f;

    /* ---- 1. 读取传感器（阻塞约 15ms，但已运行在独立任务中） ---- */
    if (SHT30_ReadTempHum(&sht30_dev, &t, &h) != HAL_OK) {
        printf("[Sensor] Read error!\r\n");

        /* 标记数据无效（加锁写入） */
        if (lock_sensor_data()) {
            data_valid = false;
            unlock_sensor_data();
        }
        return;
    }

    /* ---- 2. 加锁更新共享变量 ---- */
    if (lock_sensor_data()) {
        sys_temperature = t;
        sys_humidity    = h;
        data_valid      = true;
        unlock_sensor_data();
    }

    printf("[Sensor] T: %.2f C, H: %.2f %%\r\n", t, h);
}

/**
 * @brief  获取最新温湿度数据（线程安全）
 * @param  temp : [输出] 温度值，允许传 NULL
 * @param  humi : [输出] 湿度值，允许传 NULL
 * @return true = 数据有效，false = 数据无效
 */
bool App_Sensor_GetData(float *temp, float *humi)
{
    bool valid = false;

    if (lock_sensor_data()) {
        valid = data_valid;
        if (valid) {
            if (temp != NULL) *temp = sys_temperature;
            if (humi != NULL) *humi = sys_humidity;
        }
        unlock_sensor_data();
    }
    return valid;
}
