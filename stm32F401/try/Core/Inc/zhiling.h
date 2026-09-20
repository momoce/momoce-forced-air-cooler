#ifndef __ZHILING_H
#define __ZHILING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* 指令回调函数类型：无参数，无返回值 */
typedef void (*ZL_CmdHandler)(void);

/* 指令表条目 */
typedef struct {
    const char   *cmd;      // 指令字符串（区分大小写）
    ZL_CmdHandler handler;  // 对应的处理函数
    const char   *desc;     // 简短描述（用于 HELP 指令）
} ZL_CmdEntry;

/*----------------------------------------------
 * 初始化串口指令接收（中断方式）
 * 在 main() 中 MX_USART1_UART_Init() 之后调用
 *----------------------------------------------*/
void ZL_Init(void);

/* 注册自定义指令表（NULL 表示使用内置默认表） */
void ZL_SetCmdTable(const ZL_CmdEntry *table, uint8_t count);

#ifdef __cplusplus
}
#endif

#endif /* __ZHILING_H */
