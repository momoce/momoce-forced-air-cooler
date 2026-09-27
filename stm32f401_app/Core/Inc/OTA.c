#include "ota.h"
#include "modbus_rtu.h"
#include "bootloader.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>

/* ========================= 全局量 ========================= */

const char *msg = "123";

volatile uint8_t OTA_flag = 0;

uint8_t  ota_rx_buf[OTA_RX_BUF_SIZE];
volatile uint16_t ota_rx_len = 0;

TaskHandle_t ota_receive_task_handle = NULL;

static SemaphoreHandle_t ota_frame_sem = NULL;

uint32_t ota_expected_size = 0;
uint16_t ota_expected_crc  = 0;

uint8_t *ota_payload_ptr = NULL;
uint16_t ota_payload_len = 0;

static uint32_t ota_data_offset = 0;
static volatile uint8_t ota_transfer_done = 0;
static uint16_t ota_next_seq = 0;

/* ---- 串口整帧接收缓冲 ---- */
#define max_ota_size 32768
uint8_t  uart_rec_buff[max_ota_size] = {0};
uint16_t uart_rec_len      = 0;
uint32_t uart_rec_full_len = 0;

/* ========================= 静态声明 ========================= */

static void OTA_ResumeOtherTasks(void);
static int  OTA_ParseDataPacket(uint8_t *buf, uint16_t len);
static bool OTA_VerifyAndFinalize(void);
static void OTA_DebugMsg(const char *s);

extern void Modbus_RestartReceive(void);

/* ========================= 调试输出 ========================= */

static void OTA_DebugMsg(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, strlen(s), 100);
}

/* ========================= CRC16 ========================= */

static uint16_t OTA_CRC16(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < len; i++)
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

/* ========================= 信号量 & 参数 ========================= */

SemaphoreHandle_t OTA_GetFrameSem(void)
{
    return ota_frame_sem;
}

void OTA_SetExpected(uint32_t size, uint16_t crc)
{
    ota_expected_size = size;
    ota_expected_crc  = crc;
}

/* ========================= OTA 启动 ========================= */

void OTA_Start(void)
{
    if (OTA_flag) return;

    OTA_flag = 1;

    ota_rx_len        = 0;
    ota_data_offset   = 0;
    ota_transfer_done = 0;
    ota_next_seq      = 0;
    ota_payload_ptr   = NULL;
    ota_payload_len   = 0;

    uart_rec_len      = 0;
    uart_rec_full_len = 0;

    if (ota_frame_sem == NULL)
        ota_frame_sem = xSemaphoreCreateBinary();
    else
        xSemaphoreTake(ota_frame_sem, 0);

    if (ota_receive_task_handle == NULL)
    {
        xTaskCreate(OTA_ReceiveTask, "OTA_RX", 1024, NULL,
                    tskIDLE_PRIORITY + 1, &ota_receive_task_handle);
    }
}

/* ========================= OTA 停止 ========================= */

void OTA_Stop(void)
{
    TaskHandle_t current;

    if (!OTA_flag) return;

    OTA_DebugMsg("OTA_STOP_ENTER\r\n");

    /* ---- 1. 先中止 OTA 接收 ---- */
    HAL_UART_AbortReceive(&huart1);
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);

    /* ---- 2. 清 OTA 遗留状态 ---- */
    ota_rx_len        = 0;
    ota_data_offset   = 0;
    ota_next_seq      = 0;
    ota_payload_ptr   = NULL;
    ota_payload_len   = 0;

    /* ---- 3. 清 Modbus 帧状态 ---- */
    Modbus_ResetFrameState();

    /* ---- 4. 最后才切 flag ---- */
    OTA_flag = 0;

    /* ---- 5. 恢复 Modbus 任务 ---- */
    OTA_ResumeOtherTasks();

    /* ---- 6. 武装 Modbus 接收 ---- */
    Modbus_RestartReceive();

    OTA_DebugMsg("OTA_STOP_DONE\r\n");

    /* ---- 7. 删除 OTA 任务 ---- */
    current = xTaskGetCurrentTaskHandle();
    if (ota_receive_task_handle != NULL &&
        ota_receive_task_handle != current)
    {
        vTaskDelete(ota_receive_task_handle);
    }
    ota_receive_task_handle = NULL;
}

/* ========================= 恢复 Modbus 任务 ========================= */

static void OTA_ResumeOtherTasks(void)
{
    if (modbus_task_handle != NULL)
    {
        vTaskResume(modbus_task_handle);
        OTA_DebugMsg("RESUME_MB\r\n");
    }
    else
    {
        OTA_DebugMsg("RESUME_SKIP\r\n");
    }
}

/* ========================= OTA 接收任务 ========================= */

void OTA_ReceiveTask(void *argument)
{
    const TickType_t FIRST_FRAME_TIMEOUT = pdMS_TO_TICKS(10000);
    const TickType_t INTER_FRAME_TIMEOUT = pdMS_TO_TICKS(5000);
    TickType_t timeout = FIRST_FRAME_TIMEOUT;

    vTaskDelay(pdMS_TO_TICKS(50));

    /* ---- 1. 擦除 CACHE 区 ---- */
    if (!Bootloader_FlashErase(CACHE_ADDR, APP_MAX_SIZE))
    {
        OTA_DebugMsg("OTA_ERASE_FAIL\r\n");
        OTA_Stop();
        vTaskDelete(NULL);
    }

    /* ---- 2. 中止旧接收 ---- */
    HAL_UART_AbortReceive(&huart1);
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);

    /* ---- 3. 武装 OTA 接收 ---- */
    if (HAL_UARTEx_ReceiveToIdle_IT(&huart1, uart_rec_buff, max_ota_size) != HAL_OK)
    {
        OTA_DebugMsg("RX_ARM_FAIL\r\n");
        OTA_Stop();
        vTaskDelete(NULL);
    }

    /* ---- 4. 通知上位机 ---- */
    OTA_DebugMsg("OTA_READY\r\n");

    /* ---- 5. 循环处理每一帧 ---- */
    while (1)
    {
        if (xSemaphoreTake(OTA_GetFrameSem(), timeout) == pdTRUE)
        {
            timeout = INTER_FRAME_TIMEOUT;

            if (ota_payload_ptr != NULL && ota_payload_len > 0)
            {
                if (Bootloader_FlashWrite(CACHE_ADDR + ota_data_offset,
                                          ota_payload_ptr,
                                          ota_payload_len))
                {
                    ota_data_offset   += ota_payload_len;
                    uart_rec_full_len += ota_payload_len;

                    char buf[64];
                    int n = sprintf(buf, "payload=%u, total=%lu\r\n",
                                    ota_payload_len,
                                    (unsigned long)uart_rec_full_len);
                    HAL_UART_Transmit(&huart1, (uint8_t *)buf, n, 100);

                    /* ---- 收到完整固件 ---- */
                    if (ota_expected_size > 0 &&
                        uart_rec_full_len >= ota_expected_size)
                    {
                        BootFlag_t flag;
                        flag.magic   = BOOT_FLAG_MAGIC;
                        flag.version = APP_VERSION;
                        flag.length  = ota_expected_size;
                        flag.crc16   = ota_expected_crc;
                        flag.state   = UPDATE_STATE_PENDING;
                        flag.target  = TARGET_APP_A;
                        memset(flag.reserved, 0, sizeof(flag.reserved));

                        if (Bootloader_SetFlag(&flag))
                            OTA_DebugMsg("FLAG_SET_OK\r\n");
                        else
                            OTA_DebugMsg("FLAG_SET_FAIL\r\n");

                        OTA_DebugMsg("ALL_RECEIVED\r\n");

                        ota_receive_task_handle = NULL;
                        OTA_Stop();

                        OTA_DebugMsg("WAIT_REBOOT\r\n");
                        vTaskDelete(NULL);
                    }
                }
                else
                {
                    OTA_DebugMsg("FLASH_WR_ERR\r\n");
                }

                ota_payload_ptr = NULL;
                ota_payload_len = 0;
            }
        }
        else
        {
            /* 超时：恢复 Modbus */
            OTA_DebugMsg("OTA_TIMEOUT\r\n");

            HAL_UART_AbortReceive(&huart1);
            __HAL_UART_CLEAR_OREFLAG(&huart1);
            __HAL_UART_CLEAR_IDLEFLAG(&huart1);

            ota_receive_task_handle = NULL;
            OTA_Stop();

            vTaskDelete(NULL);
        }
    }
}

/* ========================= 占位函数 ========================= */

static int OTA_ParseDataPacket(uint8_t *buf, uint16_t len)
{
    (void)buf; (void)len;
    return 0;
}

static bool OTA_VerifyAndFinalize(void)
{
    return true;
}