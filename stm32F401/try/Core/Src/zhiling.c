/* 串口指令解析模块 — 中断方式接收，匹配指令触发对应函数 */

#include "zhiling.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* 外部引用：main.c 中定义的 UART 句柄 */
extern UART_HandleTypeDef huart1;

/*----------------------------------------------
 * 接收缓冲区
 *----------------------------------------------*/
#define ZL_BUF_SIZE  128          // 单条指令最大长度
static uint8_t  zl_rx_buf[ZL_BUF_SIZE];
static uint8_t  zl_rx_idx = 0;   // 当前已接收字节数

/* 单字节缓冲区，供中断接收使用 */
static uint8_t  zl_one_byte;

/*----------------------------------------------
 * 指令表
 *----------------------------------------------*/
static const ZL_CmdEntry *zl_cmd_table    = NULL;
static uint8_t            zl_cmd_count    = 0;

/*==============================================
 * 默认指令处理函数（用户可替换）
 *==============================================*/

static void fan_on(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET); // PA0 低电平 → 开启
    const char *msg = "FAN: ON\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), 100);
}

static void fan_off(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);   // PA0 高电平 → 关闭
    const char *msg = "FAN: OFF\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), 100);
}

static void show_status(void)
{
    char buf[64];
    GPIO_PinState pa0 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
    uint32_t tick = HAL_GetTick();
    int len = snprintf(buf, sizeof(buf),
        "--- STATUS ---\r\n"
        "PA0 (Fan): %s\r\n"
        "Uptime: %lu ms\r\n",
        (pa0 == GPIO_PIN_RESET) ? "ON" : "OFF",
        tick);
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 100);
}

static void show_help(void)
{
    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "--- COMMANDS ---\r\n"
        "FAN_ON    -> turn on fan\r\n"
        "FAN_OFF   -> turn off fan\r\n"
        "STATUS    -> show system status\r\n"
        "HELP      -> show this help\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 100);
}

/*==============================================
 * 默认指令表
 *==============================================*/
static const ZL_CmdEntry default_cmds[] = {
    { "FAN_ON",   fan_on,      "turn on fan"        },
    { "FAN_OFF",  fan_off,     "turn off fan"       },
    { "STATUS",   show_status, "show system status" },
    { "HELP",     show_help,   "show this help"     },
};

/*==============================================
 * 公开 API
 *==============================================*/

/**
 * @brief  启动中断串口接收
 * @note   在 MX_USART1_UART_Init() 之后调用一次
 */
void ZL_Init(void)
{
    /* 若未注册指令表，使用默认表 */
    if (zl_cmd_table == NULL) {
        zl_cmd_table = default_cmds;
        zl_cmd_count = sizeof(default_cmds) / sizeof(default_cmds[0]);
    }

    /* 首次启动接收一个字节 */
    zl_rx_idx = 0;
    HAL_UART_Receive_IT(&huart1, &zl_one_byte, 1);
}

/**
 * @brief  注册自定义指令表
 */
void ZL_SetCmdTable(const ZL_CmdEntry *table, uint8_t count)
{
    zl_cmd_table = table;
    zl_cmd_count = count;
}

/*==============================================
 * 内部：指令解析
 *==============================================*/

/**
 * @brief  收到完整一行（以 \r\n 或 \n 结尾）时调用
 *         NULL 跳过干扰字节
 */
static void zl_parse_command(uint8_t *data, uint8_t len)
{
    if (len == 0 || data == NULL) return;

    /* 去掉尾部换行符 */
    while (len > 0 && (data[len - 1] == '\n' || data[len - 1] == '\r')) {
        len--;
    }
    data[len] = '\0';

    if (len == 0) return;  // 空行忽略

    /* 遍历指令表匹配 */
    for (uint8_t i = 0; i < zl_cmd_count; i++) {
        if (strcmp((const char *)data, zl_cmd_table[i].cmd) == 0) {
            if (zl_cmd_table[i].handler) {
                zl_cmd_table[i].handler();
            }
            return;  // 匹配成功，结束
        }
    }

    /* 未匹配到任何指令 */
    const char *err = "ERR: unknown command\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)err, strlen(err), 100);
}

/*==============================================
 * HAL 串口接收完成回调
 *==============================================*/

/**
 * @brief  每接收一个字节就进这里一次
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;

    uint8_t ch = zl_one_byte;

    /* 收到换行 → 处理完整指令，然后重置缓冲区 */
    if (ch == '\n') {
        zl_parse_command(zl_rx_buf, zl_rx_idx);
        zl_rx_idx = 0;
    }
    /* 收到回车 → 忽略（与 \n 配对时）或有终端的裸 \r 也算 */
    else if (ch == '\r') {
        /* CRT 模式：裸 \r 也提交 */
        if (zl_rx_idx > 0) {
            zl_parse_command(zl_rx_buf, zl_rx_idx);
            zl_rx_idx = 0;
        }
    }
    /* 普通字符 → 存入缓冲区（防溢出） */
    else if (zl_rx_idx < ZL_BUF_SIZE - 1) {
        zl_rx_buf[zl_rx_idx++] = ch;
    }
    /* 溢出 → 重置 */
    else {
        zl_rx_idx = 0;
        const char *err = "ERR: cmd too long\r\n";
        HAL_UART_Transmit(&huart1, (uint8_t *)err, strlen(err), 100);
    }

    /* 重新启动下一次中断接收 */
    HAL_UART_Receive_IT(&huart1, &zl_one_byte, 1);
}
