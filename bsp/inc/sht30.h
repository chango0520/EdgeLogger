#ifndef __SHT30_H
#define __SHT30_H

#include "stm32f4xx_hal.h"


#define SHT30_ADDR        0x44 //  SHT30地址

#define SHT30_CMD_SOFT_RESET   0x30A2 //软复位
#define SHT30_CMD_MEAS_HIGHREP 0x2400 //高性能

typedef struct {
    I2C_HandleTypeDef *hi2c;    // I2C句柄
    uint8_t addr;               // 对应地址
} SHT30_HandleTypeDef;


void SHT30_Init(SHT30_HandleTypeDef *sht30, I2C_HandleTypeDef *hi2c, uint8_t addr);
HAL_StatusTypeDef SHT30_ReadTempHum(SHT30_HandleTypeDef *sht30, float *temp, float *humi);

#endif
