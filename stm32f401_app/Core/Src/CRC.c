#include "crc16.h"
#include "bootloader.h"

#define MODBUS_RX_BUF_SIZE 256
uint8_t modbus_rx_buf[MODBUS_RX_BUF_SIZE];

/**
  * @brief  计算 CRC16/MODBUS
  * @param  data: 数据指针
  * @param  len:  数据长度
  * @retval CRC16 值
  */
uint16_t Modbus_CRC16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }

    return crc;
}