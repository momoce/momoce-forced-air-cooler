#include "modbus_rtu.h"
#include "CRC16.h"
#include "bootloader.h"
#include "ota.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>

/* ========================= 全局变量 ========================= */

volatile uint8_t  modbus_frame_ready = 0;
volatile uint16_t modbus_frame_len   = 0;

TaskHandle_t modbus_task_handle = NULL;

/* modbus_rx_buf 唯一定义 */
uint8_t modbus_rx_buf[MODBUS_RX_BUF_SIZE];

static uint8_t modbus_tx_buf[MODBUS_TX_BUF_SIZE];

bool lianjie1 = 0;

/* ---- 来自 ota.c 的全局量 ---- */
extern uint8_t  uart_rec_buff[];
extern uint16_t uart_rec_len;
extern uint32_t uart_rec_full_len;
extern volatile uint8_t OTA_flag;
extern SemaphoreHandle_t OTA_GetFrameSem(void);

#define OTA_REC_BUF_SIZE  32768U

/* ========================= 本地调试输出 ========================= */

static void MB_Dbg(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, strlen(s), 100);
}

/* ========================= 初始化 ========================= */

void Modbus_Init(void)
{
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    HAL_UARTEx_ReceiveToIdle_IT(&huart1, modbus_rx_buf, MODBUS_RX_BUF_SIZE);
}

/* ========================= Modbus 任务 ========================= */

void ModbusTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        uint16_t len = modbus_frame_len;
        if (len > 0 && len <= MODBUS_RX_BUF_SIZE)
        {
            uint8_t frame_copy[MODBUS_RX_BUF_SIZE];
            memcpy(frame_copy, modbus_rx_buf, len);

            Modbus_ParseFrame(frame_copy, len);
        }
    }
}

/* ========================= 发送 ========================= */

uint8_t frame[4];

void Modbus_SendFrame(uint8_t *buf, uint16_t len)
{
    if (len + 2 > MODBUS_TX_BUF_SIZE)
        return;

    uint16_t crc = Modbus_CRC16(buf, len);

    memcpy(modbus_tx_buf, buf, len);
    modbus_tx_buf[len]     = (uint8_t)(crc & 0xFF);
    modbus_tx_buf[len + 1] = (uint8_t)(crc >> 8);

    HAL_UART_Transmit(&huart1, modbus_tx_buf, len + 2, 100);
}

/* ========================= 协议解析 ========================= */

uint8_t Modbus_ParseFrame(uint8_t *buf, uint16_t len)
{
    if (len < 4 || len > MODBUS_RX_BUF_SIZE)
        return 0;

    uint16_t crc_calc = Modbus_CRC16(buf, len - 2);
    uint16_t crc_recv = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);
    if (crc_calc != crc_recv)
    {
        MB_Dbg("MPF_CRC_ERR\r\n");
        return 0;
    }

    uint8_t slave_addr = buf[0];
    if (slave_addr != MODBUS_BROADCAST_ADDR)
        return 0;

    uint8_t func_code = buf[1];
    uint8_t data_num1 = buf[2];
    uint8_t data_num2 = buf[3];

    if (func_code == 0x01)
    {
        if (data_num1 == 0x00 && data_num2 == 0x00)
        {
            frame[0] = device_id; frame[1] = 0x01;
            frame[2] = 0x00;      frame[3] = 0x00;
            Modbus_SendFrame(frame, 4);
        }
        else if (data_num1 == 0x00 && data_num2 == 0x01)
        {
            frame[0] = device_id; frame[1] = 0x01;
            frame[2] = 0x00;      frame[3] = 0x01;
            Modbus_SendFrame(frame, 4);
            lianjie1 = 1;
        }
        else if (data_num1 == 0x01 && data_num2 == 0x01)
        {
            frame[0] = device_id; frame[1] = 0x01;
            frame[2] = 0x01;      frame[3] = 0x01;
            Modbus_SendFrame(frame, 4);
            lianjie1 = 0;
        }
    }
    else if (func_code == 0x02)
    {
        if (data_num1 == 0x00 && data_num2 == 0x00)   /* 开始 OTA */
        {
            if (len < 12)
                return 0;

            ota_expected_size = ((uint32_t)buf[4] << 24)
                              | ((uint32_t)buf[5] << 16)
                              | ((uint32_t)buf[6] << 8)
                              |  (uint32_t)buf[7];

            ota_expected_crc  = ((uint16_t)buf[8] << 8)
                              |  (uint16_t)buf[9];

            /* 先发响应 */
            frame[0] = device_id; frame[1] = 0x02;
            frame[2] = 0x00;      frame[3] = 0x00;
            Modbus_SendFrame(frame, 4);

            /* 写 FLAG = RECEIVING */
            BootFlag_t flag;
            flag.magic   = BOOT_FLAG_MAGIC;
            flag.version = APP_VERSION;
            flag.length  = ota_expected_size;
            flag.crc16   = ota_expected_crc;
            flag.state   = UPDATE_STATE_RECEIVING;
            flag.target  = TARGET_APP_A;
            memset(flag.reserved, 0, sizeof(flag.reserved));
            Bootloader_SetFlag(&flag);

            OTA_Start();
        }
        else if (data_num1 == 0x01 && data_num2 == 0x01)   /* 重启升级 */
        {
            frame[0] = device_id; frame[1] = 0x02;
            frame[2] = 0x01;      frame[3] = 0x01;
            Modbus_SendFrame(frame, 4);

            MB_Dbg("REBOOT_CMD\r\n");

            vTaskDelay(pdMS_TO_TICKS(100));
            NVIC_SystemReset();
        }
    }

    return 1;
}

void modbus_rtu_achieve(uint8_t device, uint8_t function)
{
    (void)device; (void)function;
}

void Modbus_SetTaskHandle(TaskHandle_t handle)
{
    modbus_task_handle = handle;
}

/* ============================================================
 * 清 Modbus 帧状态
 * ============================================================ */
void Modbus_ResetFrameState(void)
{
    modbus_frame_len   = 0;
    modbus_frame_ready = 0;
}

/* ============================================================
 * 恢复 Modbus 接收
 * 内部完成：Abort + Clear + 清帧状态 + 保险清 OTA_flag + Arm + 重试
 * ============================================================ */
void Modbus_RestartReceive(void)
{
    /* ---- 1. 中止当前接收 ---- */
    HAL_UART_AbortReceive(&huart1);

    /* ---- 2. 清标志 ---- */
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);

    /* ---- 3. 清 Modbus 帧状态 ---- */
    Modbus_ResetFrameState();

    /* ---- 4. 保险：切回 Modbus 分支 ---- */
    OTA_flag = 0;

    /* ---- 5. 重新武装，带重试 ---- */
    HAL_StatusTypeDef r = HAL_BUSY;
    int retry = 3;
    while (retry-- > 0)
    {
        r = HAL_UARTEx_ReceiveToIdle_IT(&huart1, modbus_rx_buf,
                                        MODBUS_RX_BUF_SIZE);
        if (r == HAL_OK) break;
        HAL_Delay(1);
    }

    char dbg[32];
    int n = snprintf(dbg, sizeof(dbg), "MRR_RET=%d\r\n", r);
    HAL_UART_Transmit(&huart1, (uint8_t *)dbg, n, 100);
}

/* ============================================================
 * 统一接收回调
 * ============================================================ */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance != USART1) return;

    if (OTA_flag)
    {
        /* ---- OTA 模式 ---- */
        if (Size >= 9 &&
            uart_rec_buff[0] == 0x00 &&
            uart_rec_buff[1] == 0x02 &&
            uart_rec_buff[2] == 0x01)
        {
            uint16_t payload_len = ((uint16_t)uart_rec_buff[5] << 8)
                                 |  (uint16_t)uart_rec_buff[6];

            ota_payload_ptr = &uart_rec_buff[7];
            ota_payload_len = payload_len;
            uart_rec_len    = payload_len;

            BaseType_t xWoken = pdFALSE;
            SemaphoreHandle_t sem = OTA_GetFrameSem();
            if (sem != NULL) xSemaphoreGiveFromISR(sem, &xWoken);
            portYIELD_FROM_ISR(xWoken);
        }

        HAL_UARTEx_ReceiveToIdle_IT(&huart1, uart_rec_buff, OTA_REC_BUF_SIZE);
    }
    else
    {
        /* ---- Modbus 模式 ---- */
        modbus_frame_len = Size;

        HAL_UARTEx_ReceiveToIdle_IT(&huart1, modbus_rx_buf, MODBUS_RX_BUF_SIZE);

        if (modbus_task_handle != NULL)
        {
            BaseType_t xWoken = pdFALSE;
            vTaskNotifyGiveFromISR(modbus_task_handle, &xWoken);
            portYIELD_FROM_ISR(xWoken);
        }
    }
}