#ifndef OTA_H
#define OTA_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* OTA 接收缓冲区大小 */
#define OTA_RX_BUF_SIZE   1024

/* 超时时间（毫秒）：收到第一个包后，超过此时间无新包则判定完成 */
#define OTA_TIMEOUT_MS    200

/* 状态标志 */
extern volatile uint8_t OTA_flag;

/* 接收缓冲 */
extern uint8_t  ota_rx_buf[OTA_RX_BUF_SIZE];
extern volatile uint16_t ota_rx_len;

/* 线程句柄 */
extern TaskHandle_t ota_receive_task_handle;

/* 预期长度和 CRC（由上位机 OTA 开始命令携带） */
extern uint32_t ota_expected_size;
extern uint16_t ota_expected_crc;

/* 接口 */
void OTA_Start(void);
void OTA_Stop(void);
void OTA_ReceiveTask(void *argument);

/* 给中断分发用 */
SemaphoreHandle_t OTA_GetFrameSem(void);

#endif