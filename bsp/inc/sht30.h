#ifndef __SHT30_H
#define __SHT30_H

#include "stm32f4xx_hal.h"


#define SHT30_ADDR_GND         0x44 //addr接地默认为0x44
#define SHT30_ADDR_VDD         0x45

#define SHT30_CMD_SOFT_RESET   0x30A2
#define SHT30_CMD_MEAS_HIGHREP 0x2400  // 高重复性，非时钟拉伸使能


typedef struct {
    I2C_HandleTypeDef *hi2c;    // 指向 I2C 外设句柄
    uint8_t addr;               // 器件 7 位地址
} SHT30_HandleTypeDef;


void SHT30_Init(SHT30_HandleTypeDef *sht30, I2C_HandleTypeDef *hi2c, uint8_t addr);
HAL_StatusTypeDef SHT30_ReadTempHum(SHT30_HandleTypeDef *sht30, float *temp, float *humi);

#endif
