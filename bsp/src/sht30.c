#include "sht30.h"

/* SHT30超时时间 */
#define SHT30_TIMEOUT 100

/* 发送命令函数 */
static HAL_StatusTypeDef SHT30_WriteCmd(SHT30_HandleTypeDef *sht30, uint16_t cmd) {
    uint8_t buf[2];
    buf[0] = (cmd >> 8) & 0xFF;   
    buf[1] = cmd & 0xFF;
    /* 发送命令 */
    return HAL_I2C_Master_Transmit(sht30->hi2c, (uint16_t)(sht30->addr << 1), buf, 2, SHT30_TIMEOUT);
}

/* CRC8校验函数 */
static uint8_t SHT30_CRC8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31;
            else crc <<= 1;
        }
    }
    return crc;
}

/**
 * @brief  SHT30初始化
 * @param  sht30 : 句柄
 * @param  hi2c  : I2C句柄 (&hi2c1)
 * @param  addr  : SHT30地址 (SHT30_ADDR)
 */
void SHT30_Init(SHT30_HandleTypeDef *sht30, I2C_HandleTypeDef *hi2c, uint8_t addr) {
    if(sht30 == NULL || hi2c == NULL) return; 
    
    sht30->hi2c = hi2c;
    sht30->addr = addr;
    
    /* 最快也需要 1.5ms */
    SHT30_WriteCmd(sht30, SHT30_CMD_SOFT_RESET);
    HAL_Delay(2); 
}

/**
 * @brief  读取温湿度
 * @param  sht30 : 句柄
 * @param  temp  : 读取的温度
 * @param  humi  : 读取的湿度
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef SHT30_ReadTempHum(SHT30_HandleTypeDef *sht30, float *temp, float *humi) {
    uint8_t buf[6];
    uint16_t rawTemp, rawHumi;
    HAL_StatusTypeDef status;

    if(sht30 == NULL) return HAL_ERROR;

    /* 先进行高重复率测量 */
    status = SHT30_WriteCmd(sht30, SHT30_CMD_MEAS_HIGHREP);
    if (status != HAL_OK) return status;

    /* 最快需要 15ms */
    HAL_Delay(15);

    /* 接收数据 */
    status = HAL_I2C_Master_Receive(sht30->hi2c, (uint16_t)((sht30->addr << 1) | 0x01), buf, 6, SHT30_TIMEOUT);
    if (status != HAL_OK) return status;

    /* 校验数据 */
    if ((SHT30_CRC8(buf, 2) != buf[2]) || (SHT30_CRC8(buf + 3, 2) != buf[5])) {
        return HAL_ERROR;
    }

    /* 拼接数据 */
    rawTemp = ((uint16_t)buf[0] << 8) | buf[1];
    rawHumi = ((uint16_t)buf[3] << 8) | buf[4];

    /* 计算出真实数据 */
    if(temp != NULL) {
        *temp = -45.0f + 175.0f * ((float)rawTemp / 65535.0f);
    }
    if(humi != NULL) {
        *humi = 100.0f * ((float)rawHumi / 65535.0f);
    }

    return HAL_OK;
}
