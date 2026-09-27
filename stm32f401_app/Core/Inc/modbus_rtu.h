#ifndef __MODBUS_RTU_H
#define __MODBUS_RTU_H

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/* ========================= 宏定义 ========================= */

#define MODBUS_RX_BUF_SIZE      256
#define MODBUS_TX_BUF_SIZE      256

/* ========================= 全局变量声明 ========================= */

extern volatile uint8_t  modbus_frame_ready;
extern volatile uint16_t modbus_frame_len;
extern TaskHandle_t      modbus_task_handle;
extern bool              lianjie1;

/* modbus_rx_buf 只声明，定义在 modbus_rtu.c 里 */
extern uint8_t           modbus_rx_buf[];

/* ========================= 函数声明 ========================= */

void              Modbus_Init(void);
void              ModbusTask(void *argument);
void              Modbus_SetTaskHandle(TaskHandle_t handle);
void              Modbus_RestartReceive(void);
void              Modbus_ResetFrameState(void);
uint8_t           Modbus_ParseFrame(uint8_t *buf, uint16_t len);
void              Modbus_SendFrame(uint8_t *buf, uint16_t len);
void              modbus_rtu_achieve(uint8_t device, uint8_t function);

#endif /* __MODBUS_RTU_H */