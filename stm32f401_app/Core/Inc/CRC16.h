/**
 ******************************************************************************
 * @file    crc16.h
 * @brief   CRC-16/MODBUS 校验 (与下位机通信用)
 * @note    算法参数:
 *          - 多项式: 0x8005 (反射后参与运算 0xA001)
 *          - 初值:   0xFFFF
 *          - RefIn:  true,  RefOut: true,  XorOut: 0x0000
 *          - 标准测试串 "123456789" 计算结果应为 0x4B37
 ******************************************************************************
 */

#ifndef __CRC16_H
#define __CRC16_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief   计算 CRC-16/MODBUS 校验值
 * @param   data: 待校验数据指针
 * @param   len:  数据长度 (字节)
 * @retval  CRC16 校验值
 */
uint16_t CRC16_Modbus(const uint8_t *data, uint32_t len);
uint16_t Modbus_CRC16(const uint8_t *data, uint16_t len);
#ifdef __cplusplus
}
#endif

#endif /* __CRC16_H */
