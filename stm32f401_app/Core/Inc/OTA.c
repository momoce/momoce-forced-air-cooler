#include "ota.h"
#include "modbus_rtu.h"
#include "bootloader.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>

/* ============================================================
 * 全局变量（定义只在这里出现一次）
 * ============================================================ */
volatile uint8_t OTA_flag = 0;

uint8_t  ota_rx_buf[OTA_RX_BUF_SIZE];
volatile uint16_t ota_rx_len = 0;

TaskHandle_t ota_receive_task_handle = NULL;

static SemaphoreHandle_t ota_frame_sem = NULL;

/* 预期长度和 CRC（由上位机 OTA 开始命令携带） */
uint32_t ota_expected_size = 0;
uint16_t ota_expected_crc  = 0;

/* 已接收的 bin 数据总量 */
static uint32_t ota_data_offset = 0;

/* 传输完成标志 */
static volatile uint8_t ota_transfer_done = 0;

/* 内部函数 */
static void OTA_SuspendOtherTasks(void);
static void OTA_ResumeOtherTasks(void);
static int  OTA_ParseDataPacket(uint8_t *buf, uint16_t len);
static void OTA_VerifyAndFinalize(void);

/* ============================================================
 * 给中断用的 getter
 * ============================================================ */
SemaphoreHandle_t OTA_GetFrameSem(void)
{
    return ota_frame_sem;
}

/* ============================================================
 * 启动 OTA
 *   - 设置 OTA_flag = 1
 *   - 挂起 ModbusTask
 *   - 创建 OTA 接收线程
 *   - ★ 不调用 HAL_UART_Receive_IT，中断一直在跑
 * ============================================================ */
void OTA_Start(void)
{
    if (OTA_flag) return;

    OTA_flag = 1;

    OTA_SuspendOtherTasks();

    ota_rx_len        = 0;
    ota_data_offset   = 0;
    ota_transfer_done = 0;

    if (ota_frame_sem == NULL)
        ota_frame_sem = xSemaphoreCreateBinary();

    if (ota_receive_task_handle == NULL)
    {
        xTaskCreate(OTA_ReceiveTask, "OTA_RX", 1024, NULL,
                    tskIDLE_PRIORITY + 3, &ota_receive_task_handle);
    }

    /* ★ 不调用 HAL_UART_Receive_IT，
       中断在 Modbus_Init 里已经启动，一直用 modbus_rx_byte */
}

/* ============================================================
 * 停止 OTA
 *   - 清除 OTA_flag
 *   - 删除 OTA 线程
 *   - 恢复 ModbusTask
 *   - ★ 不调用 HAL_UART_Receive_IT
 * ============================================================ */
void OTA_Stop(void)
{
    if (!OTA_flag) return;

    OTA_flag = 0;
    ota_rx_len = 0;
    ota_data_offset = 0;

    if (ota_receive_task_handle != NULL)
    {
        vTaskDelete(ota_receive_task_handle);
        ota_receive_task_handle = NULL;
    }

    OTA_ResumeOtherTasks();

    /* ★ 不调用 HAL_UART_Receive_IT */
}

/* ============================================================
 * 挂起 / 恢复其它任务
 * ============================================================ */
static void OTA_SuspendOtherTasks(void)
{
    if (modbus_task_handle != NULL)
        vTaskSuspend(modbus_task_handle);
    /* 如果有其它任务，继续挂 */
}

static void OTA_ResumeOtherTasks(void)
{
    if (modbus_task_handle != NULL)
        vTaskResume(modbus_task_handle);
}

/* ============================================================
 * OTA 接收线程
 *   - 擦除缓存区
 *   - 收到第一个包之前：200ms 超时不做处理，继续等
 *   - 收到第一个包之后：200ms 无新包 → 判定传输完成
 *   - 校验后写 BootFlag，退出，恢复 ModbusTask
 * ============================================================ */
void OTA_ReceiveTask(void *argument)
{
    (void)argument;

    ota_data_offset   = 0;
    ota_transfer_done = 0;

    /* 1. 擦除缓存区（不是 App 区） */
    if (!Bootloader_FlashErase(CACHE_ADDR, CACHE_SIZE))
    {
        OTA_Stop();
        vTaskDelete(NULL);
        return;
    }

    /* 2. 主循环：等帧 + 超时检测 */
    while (1)
    {
        if (xSemaphoreTake(ota_frame_sem, pdMS_TO_TICKS(OTA_TIMEOUT_MS)) == pdTRUE)
        {
            /* ---- 收到一帧 ---- */
            uint16_t len = ota_rx_len;
            ota_rx_len = 0;

            if (len > 0 && len <= OTA_RX_BUF_SIZE)
            {
                if (OTA_ParseDataPacket(ota_rx_buf, len) == 0)
                {
                    /* 解析失败，回 NACK */
                    uint8_t nack[4];
                    nack[0] = device_id;
                    nack[1] = 0x02;
                    nack[2] = 0x00;
                    nack[3] = 0x02;   /* 子命令 0x02 = 数据包错误 */
                    Modbus_SendFrame(nack, 4);
                }
            }
        }
        else
        {
            /* ---- 超过 200ms 没收到新帧 ---- */

            /* 只有收到过至少一个包，才判定传输完成 */
            if (ota_data_offset > 0 && !ota_transfer_done)
            {
							OTA_flag=0;
                ota_transfer_done = 1;
                OTA_VerifyAndFinalize();
                break;   /* ★ 校验完退出循环 */
            }

            /* ota_data_offset == 0：还没收到第一个包，继续等 */
        }
    }

    /* ============================================================
     * 3. ★ OTA 结束，交还给 Modbus
     * ============================================================ */
    ota_rx_len = 0;

    /* ★ 先清零 OTA_flag，中断回调才会存到 modbus_rx_buf */
    OTA_flag = 0;

    /* ★ 恢复 ModbusTask */
    OTA_ResumeOtherTasks();

    /* ★ 不调用 HAL_UART_Receive_IT，
       中断一直在跑，用同一个 modbus_rx_byte */

    /* 4. 删除自己 */
    ota_receive_task_handle = NULL;
    vTaskDelete(NULL);
}

/* ============================================================
 * 解析一包 bin 数据
 * 帧格式： [00][02][01][seqH][seqL][lenH][lenL][data...][CRC]
 * 返回 1 成功，0 失败
 * ============================================================ */
static int OTA_ParseDataPacket(uint8_t *buf, uint16_t len)
{
    if (len < 9) return 0;

    /* ---- CRC 校验 ---- */
    uint16_t crc_calc = Modbus_CRC16(buf, len - 2);
    uint16_t crc_recv = (uint16_t)buf[len - 2]
                      | ((uint16_t)buf[len - 1] << 8);
    if (crc_calc != crc_recv)
        return 0;

    /* ---- 解析字段 ---- */
    uint8_t  func = buf[1];
    uint8_t  sub  = buf[2];
    uint16_t seq  = ((uint16_t)buf[3] << 8) | buf[4];
    uint16_t dlen = ((uint16_t)buf[5] << 8) | buf[6];

    if (func != 0x02 || sub != 0x01)
        return 0;

    if (len < 9 + dlen)
        return 0;

    /* ---- 写入缓存区，从 CACHE_ADDR 开始，不带固件头 ---- */
    const uint8_t *data = &buf[7];
    if (!Bootloader_FlashWrite(CACHE_ADDR + ota_data_offset,
                               data, dlen))
        return 0;

    ota_data_offset += dlen;

    /* ---- 回 ACK ---- */
    uint8_t ack[5];
    ack[0] = device_id;
    ack[1] = 0x02;
    ack[2] = 0x01;             /* 子命令 0x01 = 数据包 ACK */
    ack[3] = (seq >> 8) & 0xFF;
    ack[4] = seq & 0xFF;
    Modbus_SendFrame(ack, 5);

    return 1;
}

/* ============================================================
 * 超时后：校验缓存区 + 写 BootFlag
 *   - ★ 不回复、不复位，交给 ModbusTask 处理完成命令
 * ============================================================ */
static void OTA_VerifyAndFinalize(void)
{
    uint32_t total = ota_data_offset;

    /* ---- 1. 从缓存区计算 bin 数据的 CRC16 ---- */
    const uint8_t *fw = (const uint8_t *)CACHE_ADDR;
    uint16_t crc_calc = Modbus_CRC16((uint8_t *)fw, total);

    /* ---- 2. 校验：优先和上位机给的预期值比较 ---- */
    bool ok = false;
    if (ota_expected_crc != 0 && ota_expected_size != 0)
    {
        ok = (crc_calc == ota_expected_crc) &&
             (total    == ota_expected_size);
    }
    else
    {
        ok = (total > 0 && total <= APP_MAX_SIZE);
    }

    if (!ok)
    {
        /* 校验失败，标记失败，下次可重来 */
        ota_transfer_done = 0;
        return;
    }

    /* ---- 3. 校验成功，写 BootFlag ---- */
    BootFlag_t flag;
    memset(&flag, 0, sizeof(flag));
    flag.magic   = BOOT_FLAG_MAGIC;
    flag.state   = UPDATE_STATE_PENDING;
    flag.target  = TARGET_APP_A;
    flag.version = 1;
    flag.length  = total;
    flag.crc16   = crc_calc;

    Bootloader_SetFlag(&flag);

    /* ★ 不回复、不复位，交给 ModbusTask 处理完成命令 */
}