#include "modbus_rtu.h"
#include "CRC16.h"
#include "bootloader.h"
/* 可选：用来通知主循环有合法帧到达 */
volatile uint8_t modbus_frame_ready = 0;
volatile uint16_t modbus_frame_len = 0;
/* Modbus 任务句柄 */
static TaskHandle_t modbus_task_handle = NULL;
/* 给 HAL_UART_Receive_IT 用的单字节缓冲 */
uint8_t modbus_rx_byte;          /* 或者写成 static uint8_t modbus_rx_byte; */
static uint8_t  modbus_rx_buf[MODBUS_RX_BUF_SIZE];   /* 接收缓冲区 */
static volatile uint16_t modbus_rx_len = 0;          /* 当前已收字节数 */
static uint8_t modbus_tx_buf[MODBUS_TX_BUF_SIZE];    //发送缓冲区






/**
  * @brief  初始化 Modbus RTU：创建任务、使能 IDLE 中断、启动单字节接收
  * @note   在 main() 里调度器启动前调用
  */
void Modbus_Init(void)
{
	  /* ----  配置 NVIC：让 CPU 能响应 USART1 中断 ---- */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    /* 使能 USART1 的 IDLE 中断 */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    /* 启动单字节接收中断 */
    HAL_UART_Receive_IT(&huart1, &modbus_rx_byte, 1);
}
uint8_t tx_buf[] = {0x01, 0x03, 0x02, 0x00, 0x0A, 0x79, 0x84};


void ModbusTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        /* ★ 这里阻塞，不占 CPU，等通知 */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* ★ 只有收到通知才会执行下面 */
			//HAL_UART_Transmit(&huart1, tx_buf, sizeof(tx_buf), 100);
        uint16_t len = modbus_frame_len;
        if (len > 0 && len <= MODBUS_RX_BUF_SIZE)
        {
            uint8_t frame[MODBUS_RX_BUF_SIZE];
            memcpy(frame, modbus_rx_buf, len);
            Modbus_ParseFrame(frame, len);
        }
    }
}
/**
  * @brief  发送一帧 Modbus RTU（自动加 CRC）
  * @param  buf: 数据区（不含 CRC）
  * @param  len: 数据区长度
  */
uint8_t frame[5];//发送帧数据用
void Modbus_SendFrame(uint8_t *buf, uint16_t len)
{
    if (len + 2 > MODBUS_TX_BUF_SIZE)
        return;

    /* 1. 计算 CRC，范围是 buf[0..len-1] */
    uint16_t crc = Modbus_CRC16(buf, len);

    /* 2. 拷贝数据区到发送缓冲 */
    memcpy(modbus_tx_buf, buf, len);

    /* 3. 追加 CRC：低字节在前，高字节在后 */
    modbus_tx_buf[len]     = (uint8_t)(crc & 0xFF);
    modbus_tx_buf[len + 1] = (uint8_t)(crc >> 8);

    /* 4. 发送 */
    HAL_UART_Transmit(&huart1, modbus_tx_buf, len + 2, 100);
}
/**
  * @brief  解析 Modbus RTU 帧
  * @param  buf: 接收缓冲区
  * @param  len: 帧长度
  * @retval 1: 合法帧；0: 非法帧或 CRC 错误
  */

uint8_t Modbus_ParseFrame(uint8_t *buf, uint16_t len)
{
    /* ---- ① 长度检查：最短 4 字节（地址 + 功能码 + CRC） ---- */
    if (len < 4 || len > MODBUS_RX_BUF_SIZE)
        return 0;

    /* ---- ② CRC 校验 ---- */
    uint16_t crc_calc = Modbus_CRC16(buf, len - 2);
    uint16_t crc_recv = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);

    if (crc_calc != crc_recv)
    {
        /* CRC 错误，丢弃，不回复 */
        return 0;
    }

    /* ---- ③ 读设备号 ---- */
    uint8_t slave_addr = buf[0];

    /* 确保是广播地址 */
		if(slave_addr==MODBUS_BROADCAST_ADDR)
		{
		/* ---- ④ 读功能码 ---- */
    uint8_t func_code = buf[1];
		/* -----读数据位-----*/
			uint8_t data_num1 =buf[2];
			uint8_t data_num2 =buf[3];
			if(func_code==0x01)//握手指令
			{
				if(data_num1==0x00&&data_num2==0x00)//广播寻找设备
				{
			frame[0] = device_id;   /* 设备号 */
			frame[1] = 0x01;   /* 功能码 */
			frame[2] = 0x01;   /* 字节数 */
			frame[3] = 0x00;   /* 数据高字节 */
			frame[4] = 0x00;   /* 数据低字节 */
			Modbus_SendFrame(frame, 5);
				}
				else if(data_num1==0x00&&data_num2==0x01)//与设备连接
				{
			frame[0] = device_id;   /* 设备号 */
			frame[1] = 0x01;   /* 功能码 */
			frame[2] = 0x01;   /* 字节数 */
			frame[3] = 0x00;   /* 数据高字节 */
			frame[4] = 0x01;   /* 数据低字节 */
			Modbus_SendFrame(frame, 5);
				}
				else if(data_num1==0x01&&data_num2==0x01)//与设备断开连接
				{
			frame[0] = device_id;   /* 设备号 */
			frame[1] = 0x01;   /* 功能码 */
			frame[2] = 0x01;   /* 字节数 */
			frame[3] = 0x01;   /* 数据高字节 */
			frame[4] = 0x01;   /* 数据低字节 */
			Modbus_SendFrame(frame, 5);
				}

			}
		}


    /* ---- ⑤ 按功能码分发 ---- */

    return 1;
}
void modbus_rtu_achieve(uint8_t device,uint8_t function)
{
	if(device==device_id)
	{
		
	}
	else
	{
		
	}
}

/**
  * @brief  IDLE 中断里调用：一帧接收完成，通知 Modbus 任务
  */
void Modbus_FrameCompleteFromISR(void)
{

    if (modbus_rx_len > 0)
    {
        /* 保存本帧长度，清零接收长度，准备下一帧 */
        modbus_frame_len = modbus_rx_len;
        modbus_rx_len    = 0;

        /* 通知任务 */
        if (modbus_task_handle != NULL)
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            vTaskNotifyGiveFromISR(modbus_task_handle, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

/**
  * @brief  串口接收完成回调：每收到 1 字节调用一次
  * @note   由 HAL_UART_IRQHandler → UART_Receive_IT 内部调用
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)   /* 按实际串口修改 */
    {
        /* 存进缓冲区，长度 +1 */
        if (modbus_rx_len < MODBUS_RX_BUF_SIZE)
        {
            modbus_rx_buf[modbus_rx_len++] = modbus_rx_byte;
        }

        /* 重新开启下一次单字节接收 */
        HAL_UART_Receive_IT(&huart1, &modbus_rx_byte, 1);
    }
}

void Modbus_SetTaskHandle(TaskHandle_t handle)
{
    modbus_task_handle = handle;
}