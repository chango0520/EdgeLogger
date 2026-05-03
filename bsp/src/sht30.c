#include "sht30.h"

/* 定义标准 I2C 超时时间，单位：ms，防止硬件死锁 */
#define SHT30_TIMEOUT 100

/* 内部辅助：向SHT30发送16位命令 */
static HAL_StatusTypeDef SHT30_WriteCommand(SHT30_HandleTypeDef *sht30, uint16_t cmd) {
    uint8_t buf[2];
    buf[0] = (cmd >> 8) & 0xFF;   
    buf[1] = cmd & 0xFF;
    /* HAL库要求传入8位地址，即7位地址左移1位 */
    return HAL_I2C_Master_Transmit(sht30->hi2c, (uint16_t)(sht30->addr << 1), buf, 2, SHT30_TIMEOUT);
}

/* 内部辅助：CRC-8 校验 (多项式 0x31) */
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
 * @brief  初始化SHT30
 * @param  sht30 : 驱动句柄指针
 * @param  hi2c  : I2C外设句柄 (如 &hi2c1)
 * @param  addr  : SHT30的7位地址 (SHT30_ADDR_GND)
 */
void SHT30_Init(SHT30_HandleTypeDef *sht30, I2C_HandleTypeDef *hi2c, uint8_t addr) {
    if(sht30 == NULL || hi2c == NULL) return; 
    
    sht30->hi2c = hi2c;
    sht30->addr = addr;
    
    /* 发送软复位命令并等待传感器重启 (规定最高耗时 1.5ms) */
    SHT30_WriteCommand(sht30, SHT30_CMD_SOFT_RESET);
    HAL_Delay(2); 
}

/**
 * @brief  读取温湿度（单次测量，非时钟拉伸模式）
 * @param  sht30 : 驱动句柄
 * @param  temp  : 输出温度（°C）
 * @param  humi  : 输出相对湿度（%RH）
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef SHT30_ReadTempHum(SHT30_HandleTypeDef *sht30, float *temp, float *humi) {
    uint8_t buf[6];
    uint16_t rawTemp, rawHumi;
    HAL_StatusTypeDef status;

    if(sht30 == NULL) return HAL_ERROR;

    /* 1. 发送测量命令（高重复性，非时钟拉伸） */
    status = SHT30_WriteCommand(sht30, SHT30_CMD_MEAS_HIGHREP);
    if (status != HAL_OK) return status;

    /* 2. 软件延时等待测量完成 (高重复性模式测量最大耗时 15ms) */
    HAL_Delay(15);

    /* 3. 读取 6 字节数据。注：HAL 库要求读操作的从机地址最低位为 1 */
    status = HAL_I2C_Master_Receive(sht30->hi2c, (uint16_t)((sht30->addr << 1) | 0x01), buf, 6, SHT30_TIMEOUT);
    if (status != HAL_OK) return status;

    /* 4. 数据一致性 CRC 校验 */
    if ((SHT30_CRC8(buf, 2) != buf[2]) || (SHT30_CRC8(buf + 3, 2) != buf[5])) {
        return HAL_ERROR;
    }

    /* 5. 换算真实物理量 */
    rawTemp = ((uint16_t)buf[0] << 8) | buf[1];
    rawHumi = ((uint16_t)buf[3] << 8) | buf[4];

    /* 指针判空保护，允许使用者传入 NULL 表示不需要该项数据 */
    if(temp != NULL) {
        *temp = -45.0f + 175.0f * ((float)rawTemp / 65535.0f);
    }
    if(humi != NULL) {
        *humi = 100.0f * ((float)rawHumi / 65535.0f);
    }

    return HAL_OK;
}
