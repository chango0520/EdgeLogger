/**
 * @file    sht30.c
 * @brief   SHT30 温湿度传感器 I2C 驱动
 *
 * @note    本驱动直接操作 HAL I2C 外设，不依赖 FreeRTOS。
 *          延时使用 HAL_Delay()（基于 SysTick），
 *          无论是否运行 RTOS 均可正常工作。
 *
 * @引脚依赖 I2C1: SCL=PB6, SDA=PB7
 * @地址     SHT30 默认 7-bit 地址 0x44
 */

#include "sht30.h"

/** @brief I2C 通信超时时间（毫秒） */
#define SHT30_TIMEOUT   100

/* ========== 内部辅助函数 ========== */

/**
 * @brief  向 SHT30 发送 16 位命令
 * @param  sht30 : 设备句柄
 * @param  cmd   : 16 位命令字（如 SHT30_CMD_MEAS_HIGHREP）
 * @retval HAL_OK = 成功，其他 = I2C 错误
 */
static HAL_StatusTypeDef SHT30_WriteCmd(SHT30_HandleTypeDef *sht30, uint16_t cmd)
{
    uint8_t buf[2];
    buf[0] = (cmd >> 8) & 0xFF;       /* 命令高字节 */
    buf[1] = cmd & 0xFF;              /* 命令低字节 */
    return HAL_I2C_Master_Transmit(sht30->hi2c,
                                   (uint16_t)(sht30->addr << 1),
                                   buf, 2, SHT30_TIMEOUT);
}

/**
 * @brief  SHT30 专用的 CRC8 校验
 * @param  data : 待校验数据指针
 * @param  len  : 数据长度（SHT30 中为 2 字节）
 * @return 计算出的 8 位 CRC 值
 *
 * @note   SHT30 使用多项式 0x31（x^8 + x^5 + x^4 + 1），
 *         初始值 0xFF，与标准 CRC-8/MAXIM 相同。
 */
static uint8_t SHT30_CRC8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* ========== API 实现 ========== */

/**
 * @brief  初始化 SHT30 传感器
 * @param  sht30 : 设备句柄（输出参数）
 * @param  hi2c  : I2C 句柄（如 &hi2c1）
 * @param  addr  : 设备 7 位地址（通常为 SHT30_ADDR = 0x44）
 *
 * @note   执行软复位，需要至少 1.5ms 等待（这里延时 2ms 保余量）
 */
void SHT30_Init(SHT30_HandleTypeDef *sht30, I2C_HandleTypeDef *hi2c, uint8_t addr)
{
    if (sht30 == NULL || hi2c == NULL) return;

    sht30->hi2c = hi2c;
    sht30->addr = addr;

    /* 软复位 —— 清除传感器内部状态 */
    SHT30_WriteCmd(sht30, SHT30_CMD_SOFT_RESET);
    HAL_Delay(2);   /* 数据手册要求 >= 1.5ms */
}

/**
 * @brief  读取 SHT30 温湿度数据
 * @param  sht30 : 设备句柄
 * @param  temp  : [输出] 温度值（°C），范围 -40 ~ +125°C，允许传 NULL
 * @param  humi  : [输出] 湿度值（%RH），范围 0 ~ 100%，允许传 NULL
 * @retval HAL_OK       = 读取成功
 * @retval HAL_ERROR    = CRC 校验失败（数据不可信）
 * @retval HAL_TIMEOUT  = I2C 总线超时
 * @retval HAL_BUSY     = I2C 总线忙
 *
 * @note   内部流程：发测量命令 → 等待 15ms → 接收 6 字节 → CRC 校验 → 换算
 *         高重复性测量精度：温度 ±0.2°C，湿度 ±2%RH
 */
HAL_StatusTypeDef SHT30_ReadTempHum(SHT30_HandleTypeDef *sht30, float *temp, float *humi)
{
    uint8_t  buf[6];
    uint16_t rawTemp, rawHumi;
    HAL_StatusTypeDef status;

    if (sht30 == NULL) return HAL_ERROR;

    /* 1. 发出高重复率测量命令（最大测量时间 15ms） */
    status = SHT30_WriteCmd(sht30, SHT30_CMD_MEAS_HIGHREP);
    if (status != HAL_OK) return status;

    /* 2. 等待测量完成 —— 数据手册要求 14ms，用 15ms 保余量 */
    HAL_Delay(15);

    /* 3. 读取 6 字节：温度高/低/CRC + 湿度高/低/CRC */
    status = HAL_I2C_Master_Receive(sht30->hi2c,
                                    (uint16_t)((sht30->addr << 1) | 0x01),
                                    buf, 6, SHT30_TIMEOUT);
    if (status != HAL_OK) return status;

    /* 4. 校验 CRC：分别校验温度 2 字节和湿度 2 字节 */
    if ((SHT30_CRC8(buf, 2) != buf[2]) ||
        (SHT30_CRC8(buf + 3, 2) != buf[5])) {
        return HAL_ERROR;   /* 数据损坏，丢弃 */
    }

    /* 5. 拼接原始值 */
    rawTemp = ((uint16_t)buf[0] << 8) | buf[1];
    rawHumi = ((uint16_t)buf[3] << 8) | buf[4];

    /* 6. 换算为物理量
     *    温度：-45 + 175 * (rawTemp / 65535)
     *    湿度：100 * (rawHumi / 65535)
     */
    if (temp != NULL) {
        *temp = -45.0f + 175.0f * ((float)rawTemp / 65535.0f);
    }
    if (humi != NULL) {
        *humi = 100.0f * ((float)rawHumi / 65535.0f);
    }

    return HAL_OK;
}
