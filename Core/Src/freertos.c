/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : FreeRTOS 任务创建与任务函数定义
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_sensor.h"     /* SHT30 温湿度传感器应用接口 */
#include "app_mqtt.h"       /* MQTT 连接管理 & OneNet 数据上报 */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* ---- 任务函数内部使用的延时/周期常量 ----
 * 这些常量的"唯一源头"定义在 app_sensor.h 和 app_mqtt.h 中，
 * 这里直接引用，避免两处定义不同步。
 *   - SENSOR_READ_INTERVAL_MS  → app_sensor.h
 *   - PUBLISH_INTERVAL_MS      → app_mqtt.h
 *   - RECONNECT_DELAY_MS       → app_mqtt.h
 *   - TASK_LOOP_DELAY_MS       → app_mqtt.h
 */

/* USER CODE END Variables */

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for sensorTask */
osThreadId_t sensorTaskHandle;
const osThreadAttr_t sensorTask_attributes = {
  .name = "sensorTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for mqttTask */
osThreadId_t mqttTaskHandle;
const osThreadAttr_t mqttTask_attributes = {
  .name = "mqttTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void SensorTask(void *argument);
void MQTTTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of sensorTask */
  sensorTaskHandle = osThreadNew(SensorTask, NULL, &sensorTask_attributes);

  /* creation of mqttTask */
  mqttTaskHandle = osThreadNew(MQTTTask, NULL, &mqttTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /*
   * ====== 系统健康监控任务 ======
   * 每 10 秒打印一次堆栈和堆使用情况，
   * 表明 FreeRTOS 调度器正常工作，各个任务正常运行。
   */
  for(;;)
  {
    printf("[SYS] ====== System Alive ======\r\n");
    printf("[SYS] Free Heap: %u bytes\r\n",
           (unsigned int)xPortGetFreeHeapSize());

    osDelay(10000);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_SensorTask */
/**
  * @brief  Function implementing the sensorTask thread.
  * @note   负责 SHT30 温湿度传感器初始化与周期性数据采集。
  *         采集到的数据通过互斥锁保护，供 MQTTTask 等任务读取。
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_SensorTask */
void SensorTask(void *argument)
{
  /* USER CODE BEGIN SensorTask */
  /*
   * ---- 1. 初始化 SHT30 传感器 ----
   * 包括内部互斥锁创建、传感器自检、首次读取。
   */
  App_Sensor_Init();

  /*
   * ---- 2. 主循环：每 SENSOR_READ_INTERVAL_MS 采集一次温湿度 ----
   */
  for(;;)
  {
    App_Sensor_Update();                        /* 采集并更新共享变量 */
    osDelay(SENSOR_READ_INTERVAL_MS);           /* 等待周期 + 让出 CPU */
  }
  /* USER CODE END SensorTask */
}

/* USER CODE BEGIN Header_MQTTTask */
/**
  * @brief  Function implementing the mqttTask thread.
  * @note   负责 ESP8266 初始化、WiFi 连接、OneNET MQTT 连接，
  *         以及传感器数据的定时上报和云端下行消息处理。
  *
  *         失败重试机制：
  *         - 初次连网失败会每隔 RECONNECT_DELAY_MS 重试
  *         - 运行中检测到 MQTT 断开通过 App_MQTT_Reconnect() 自动重连
  *         - MQTT 连接状态通过 App_MQTT_IsConnected() 查询（封装在模块内）
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_MQTTTask */
void MQTTTask(void *argument)
{
  /* USER CODE BEGIN MQTTTask */

  uint32_t last_pub_tick = 0;   /* 上次发布时间戳（osKernelGetTickCount） */
  float    current_temp  = 0.0f;
  float    current_hum   = 0.0f;

  /*
   * ---- 1. 初始化 MQTT 连接（带重试） ----
   * 如果首次连接失败，每隔 RECONNECT_DELAY_MS 重试一次。
   */
  printf("[MQTT] Task started. Connecting to OneNET...\r\n");

  while (App_MQTT_Init() != true)
  {
    printf("[MQTT] Initialization failed, retry in %d ms...\r\n",
           RECONNECT_DELAY_MS);
    osDelay(RECONNECT_DELAY_MS);
  }

  /* 记录当前时刻，用于后续发布计时 */
  last_pub_tick = osKernelGetTickCount();

  /*
   * ---- 2. 主循环：接收下行 + 定时上报 + 断线重连 ----
   */
  for(;;)
  {
    /*
     * 2a. 处理 ESP8266 接收到的下行数据
     *     非阻塞，无数据时立即返回。内部检测到断开时会更新连接状态。
     */
    App_MQTT_ProcessIncoming();

    /*
     * 2b. 每 PUBLISH_INTERVAL_MS 发布一次传感器数据
     *     基于 osKernelGetTickCount() 实现非阻塞定时，不阻塞 osDelay。
     */
    uint32_t now = osKernelGetTickCount();
    if ((now - last_pub_tick) >= PUBLISH_INTERVAL_MS)
    {
      last_pub_tick = now;

      /* 线程安全地读取传感器最新数据 */
      if (App_Sensor_GetData(&current_temp, &current_hum))
      {
        App_MQTT_Publish_SensorData(current_temp, current_hum);
      }
      else
      {
        printf("[MQTT] Sensor data invalid, publish skipped.\r\n");
      }
    }

    /*
     * 2c. 检测到 MQTT 连接断开时尝试自动重连
     *     连接状态由 App_MQTT_ProcessIncoming() 在收到断开帧时更新。
     */
    if (!App_MQTT_IsConnected())
    {
      printf("[MQTT] Reconnecting...\r\n");
      if (App_MQTT_Reconnect())
      {
        /* 重连成功，重置发布计时 */
        last_pub_tick = osKernelGetTickCount();
      }
      else
      {
        printf("[MQTT] Reconnect failed, retry in %d ms\r\n",
               RECONNECT_DELAY_MS);
        osDelay(RECONNECT_DELAY_MS);
      }
    }

    /* 2d. 让出 CPU，等待下一次调度 */
    osDelay(TASK_LOOP_DELAY_MS);
  }
  /* USER CODE END MQTTTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
