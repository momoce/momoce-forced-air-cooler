/**
 ******************************************************************************
 * @file    bootloader.h
 * @brief   STM32F401RCT6 OTA Bootloader 头文件
 * @note    芯片: STM32F401RCT6 (Flash 256KB, 扇区: 16/16/16/16/64/128 KB)
 *
 *          分区布局 (A/B 双区 + 缓存区方案):
 *          ---------------------------------------------------------------
 *          地址范围                    名称         大小      用途
 *          ---------------------------------------------------------------
 *          0x08000000 ~ 0x08003FFF    Bootloader   16KB      引导程序
 *          0x08004000 ~ 0x08007FFF    FLAG         16KB      升级标志/固件信息
 *          0x08008000 ~ 0x0800FFFF    APP_A        32KB      当前运行区
 *          0x08010000 ~ 0x08017FFF    APP_B        32KB      升级备份区
 *          0x08020000 ~ 0x08027FFF    CACHE        32KB      OTA 缓存区
 *          ---------------------------------------------------------------
 *
 *          注意:
 *          - APP_B 落在 Sector 4 (64KB), 实际擦除会擦掉整个 64KB;
 *          - CACHE 落在 Sector 5 (128KB), 实际擦除会擦掉整个 128KB;
 *          - App 工程的链接起始地址必须与 APP_A_ADDR / APP_B_ADDR 一致,
 *            并在 App 启动时重定位中断向量表 (SCB->VTOR)。
 ******************************************************************************
 */

#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define device_id                        0x01    /* 本机地址 */
#define MODBUS_BROADCAST_ADDR            0x00    /* 广播地址 */

/* ============================================================================
 * 分区地址配置
 * ==========================================================================*/
#define FLASH_BASE_ADDR         0x08000000UL

#define APP_VERSION             0x001UL         /* 初始版本 v0.01 */

/* Bootloader 区 (Sector 0, 16KB) */
#define BOOTLOADER_ADDR         (FLASH_BASE_ADDR + 0x0000UL)   /* 0x08000000 */
#define BOOTLOADER_SIZE         (16 * 1024UL)

/* 升级标志区 (Sector 1, 16KB) */
#define FLAG_ADDR               (FLASH_BASE_ADDR + 0x4000UL)   /* 0x08004000 */
#define FLAG_SIZE               (16 * 1024UL)

/* App 运行区 A (Sector 2 + 3, 32KB) */
#define APP_A_ADDR              (FLASH_BASE_ADDR + 0x8000UL)   /* 0x08008000 */
#define APP_A_SIZE              (32 * 1024UL)

/* App 备份区 B (Sector 4 前半, 32KB, 实占 64KB) */
#define APP_B_ADDR              (FLASH_BASE_ADDR + 0x10000UL)  /* 0x08010000 */
#define APP_B_SIZE              (32 * 1024UL)

/* OTA 缓存区 (Sector 5 前半, 32KB, 实占 128KB) */
#define CACHE_ADDR              (FLASH_BASE_ADDR + 0x20000UL)  /* 0x08020000 */
#define CACHE_SIZE              (32 * 1024UL)

/* 固件最大长度 (应小于 App 区大小, 留出固件头空间) */
#define APP_MAX_SIZE            (APP_A_SIZE - 16UL)

/* ============================================================================
 * 固件头 (固件包的前 16 字节, 由上位机打包工具生成)
 * ==========================================================================*/
#define FW_HEADER_MAGIC         0xA5A55A5AUL        /* 固件包魔数 */

typedef struct
{
    uint32_t magic;             /* 魔数, 固定为 FW_HEADER_MAGIC          */
    uint32_t version;           /* 固件版本号 (递增, 如 0x0102 = v1.2)   */
    uint32_t length;            /* 固件实际长度 (不含头, 单位字节)       */
    uint16_t crc16;             /* 固件数据 (不含头) 的 CRC16/MODBUS 值  */
    uint16_t reserved;          /* 对齐保留, 填 0                        */
} FirmwareHeader_t;

#define FW_HEADER_SIZE          ((uint32_t)sizeof(FirmwareHeader_t))  /* 16 */

/* ============================================================================
 * 升级标志 (存于 FLAG 区, 用于断电恢复和 A/B 切换判断)
 * ==========================================================================*/
#define BOOT_FLAG_MAGIC         0x55AA55AAUL        /* 标志有效魔数 */

/* 升级状态 */
typedef enum
{
    UPDATE_STATE_IDLE      = 0x00,   /* 无升级任务, 直接启动 App                 */
    UPDATE_STATE_PENDING   = 0x01,   /* 已收到完整固件, 待校验切换 (升级成功标记) */
    UPDATE_STATE_RECEIVING = 0x02,   /* 正在接收固件 (中途断电则恢复为需重传)     */
    UPDATE_STATE_ABORT     = 0xFF    /* 上次升级失败, 回退到旧固件               */
} UpdateState_t;

/* 目标分区 */
typedef enum
{
    TARGET_NONE  = 0,
    TARGET_APP_A = 1,                /* 升级目标为 A 区 */
    TARGET_APP_B = 2                 /* 升级目标为 B 区 */
} TargetArea_t;

typedef struct
{
    uint32_t magic;             /* BOOT_FLAG_MAGIC      */
    uint32_t version;           /* 新固件版本号         */
    uint32_t length;            /* 新固件长度           */
    uint16_t crc16;             /* 新固件 CRC16/MODBUS  */
    uint8_t  state;             /* UpdateState_t        */
    uint8_t  target;            /* TargetArea_t         */
    uint8_t  reserved[4];       /* 对齐保留             */
} BootFlag_t;

#define BOOT_FLAG_SIZE          ((uint32_t)sizeof(BootFlag_t))

/* ============================================================================
 * 函数声明
 * ==========================================================================*/

/**
 * @brief   Bootloader 初始化 (时钟、Flash、串口/WiFi 外设等)
 * @note    在 main() 中首先调用
 */
void Bootloader_Init(void);

/**
 * @brief   Bootloader 主流程: 检查升级标志
 *          - 有 PENDING 标志: 校验缓存区固件 -> 成功则搬运到 App 区并启动,
 *            失败则回退旧固件
 *          - 无升级任务: 直接跳转 App
 */
void Bootloader_Run(void);

/**
 * @brief   跳转到指定 App 地址执行
 * @param   app_addr: App 起始地址 (APP_A_ADDR 或 APP_B_ADDR)
 * @note    跳转前需关闭中断、设置 MSP 和 VTOR
 */
void Bootloader_JumpToApp(uint32_t app_addr);

/**
 * @brief   擦除 Flash 指定区域 (按扇区擦除)
 * @param   addr: 起始地址
 * @param   size: 需要擦除的总字节数
 * @retval  true: 成功, false: 失败
 */
bool Bootloader_FlashErase(uint32_t addr, uint32_t size);

/**
 * @brief   向 Flash 写入数据 (自动 32 位对齐写)
 * @param   addr: 写入地址
 * @param   data: 数据指针
 * @param   len:  数据长度
 * @retval  true: 成功, false: 失败
 */
bool Bootloader_FlashWrite(uint32_t addr, const uint8_t *data, uint32_t len);

/**
 * @brief   校验 App 区固件完整性 (长度 + CRC16, 与固件头比对)
 * @param   app_addr: 待校验的 App 区地址
 * @param   hdr:      固件头指针 (包含 magic/length/crc16)
 * @retval  true: 校验通过
 */
bool Bootloader_VerifyFirmware(uint32_t app_addr, const FirmwareHeader_t *hdr);

/**
 * @brief   解析接收缓冲区中的固件头
 * @param   buf: 固件包缓冲区
 * @param   hdr: 输出解析结果
 * @retval  true: 头有效 (magic 正确)
 */
bool Bootloader_ParseHeader(const uint8_t *buf, FirmwareHeader_t *hdr);

/**
 * @brief   写入升级标志到 FLAG 区
 * @param   flag: 标志结构体
 * @retval  true: 成功
 */
bool Bootloader_SetFlag(const BootFlag_t *flag);

/**
 * @brief   清除升级标志 (升级完成后调用)
 * @retval  true: 成功
 */
bool Bootloader_ClearFlag(void);

/**
 * @brief   读取升级标志
 * @param   flag: 输出读取结果
 * @retval  true: 标志有效 (magic 正确)
 */
bool Bootloader_GetFlag(BootFlag_t *flag);

#ifdef __cplusplus
}
#endif

#endif /* __BOOTLOADER_H */