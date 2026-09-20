#ifndef __MODBUS_RTU_H
#define __MODBUS_RTU_H

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

/* 本机 Modbus 从站地址（按实际修改） */




#define MODBUS_RX_BUF_SIZE      256
#define MODBUS_TX_BUF_SIZE      256

/* 功能码 */
#define MODBUS_FUNC_READ_HOLDING    0x03
#define MODBUS_FUNC_WRITE_SINGLE    0x06
#define MODBUS_FUNC_WRITE_MULTI     0x10

/* 对外接口 */
void Modbus_Init(void);
void ModbusTask(void *argument);
void Modbus_RxByteFromISR(uint8_t byte);
void Modbus_FrameCompleteFromISR(void);
uint8_t Modbus_ParseFrame(uint8_t *buf, uint16_t len);
void modbus_rtu_achieve(uint8_t device,uint8_t function);
/* ★ 新增：把创建好的任务句柄交给 Modbus 模块 */
void Modbus_SetTaskHandle(TaskHandle_t handle);
#endif /* __MODBUS_RTU_H */