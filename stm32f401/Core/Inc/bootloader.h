/**
 ******************************************************************************
 * @file    bootloader.h
 * @brief   STM32F401RCT6 OTA Bootloader 头文件
 * @note    芯片: STM32F401RCT6 (Flash 256KB, 页大小 1KB)
 *
 *          分区布局 (A/B 双区方案):
 *          ---------------------------------------------------------------
 *          地址范围                    名称         大小      用途
 *          ---------------------------------------------------------------
 *          0x08000000 ~ 0x08003FFF    Bootloader   16KB      引导程序(本文件)
 *          0x08004000 ~ 0x08017FFF    APP_A        80KB      当前运行区
 *          0x08018000 ~ 0x0802BFFF    APP_B        80KB      升级备份区
 *          0x0802C000 ~ 0x0802FFFF    FLAG         16KB      升级标志/固件信息
 *          0x08030000 ~ 0x0803FFFF    预留         64KB      备用
 *          ---------------------------------------------------------------
 *
 *          App 工程的链接起始地址必须与 APP_A_ADDR / APP_B_ADDR 一致,
 *          并在 App 启动时重定位中断向量表 (SCB->VTOR)。
 ******************************************************************************
 */

#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>


#define device_id 0x01							//本机地址
#define MODBUS_BROADCAST_ADDR            0x00//广播地址
/* ============================================================================
 * 分区地址配置 (如需要可自行调整)
 * ==========================================================================*/
#define FLASH_BASE_ADDR         0x08000000UL


#define APP_VERSION   0x001UL    /* 初始版本 v0.01 */

/* Bootloader 区 */
#define BOOTLOADER_ADDR         (FLASH_BASE_ADDR + 0x0000UL)
#define BOOTLOADER_SIZE         (16 * 1024UL)

/* App 运行区 (A) */
#define APP_A_ADDR              (BOOTLOADER_ADDR + BOOTLOADER_SIZE)       /* 0x08004000 */
#define APP_A_SIZE              (80 * 1024UL)

/* App 备份区 (B) */
#define APP_B_ADDR              (APP_A_ADDR + APP_A_SIZE)                /* 0x08018000 */
#define APP_B_SIZE              (80 * 1024UL)

/* 升级标志区 (存 BootFlag_t) */
#define FLAG_ADDR               (APP_B_ADDR + APP_B_SIZE)                /* 0x0802C000 */
#define FLAG_SIZE               (16 * 1024UL)

/* 升级标志写入的扇区页号 (每页 1KB) */
#define FLAG_PAGE               (FLAG_ADDR / 1024UL)

/* 固件最大长度 (应小于 App 区大小) */
#define APP_MAX_SIZE            (APP_A_SIZE - 4UL)

/* ============================================================================
 * 固件头 (固件包的前 16 字节, 由上位机打包工具生成)
 * ==========================================================================*/
#define FW_HEADER_MAGIC         0xA5A55A5AUL        /* 固件包魔数, 用于识别有效固件 */

typedef struct
{
    uint32_t magic;             /* 魔数, 固定为 FW_HEADER_MAGIC */
    uint32_t version;           /* 固件版本号 (递增, 如 0x0102 = v1.2) */
    uint32_t length;            /* 固件实际长度 (不含头, 单位字节, <= APP_MAX_SIZE) */
    uint32_t crc32;             /* 固件数据 (不含头) 的 CRC32 校验值 */
} FirmwareHeader_t;

#define FW_HEADER_SIZE          ((uint32_t)sizeof(FirmwareHeader_t))  /* 16 */

/* ============================================================================
 * 升级标志 (存于 FLAG 区, 用于断电恢复和 A/B 切换判断)
 * ==========================================================================*/
#define BOOT_FLAG_MAGIC         0x55AA55AAUL        /* 标志有效魔数 */

/* 升级状态 */
typedef enum
{
    UPDATE_STATE_IDLE      = 0x00,   /* 无升级任务, 直接启动 App */
    UPDATE_STATE_PENDING   = 0x01,   /* 已收到完整固件, 待校验切换 (升级成功标记) */
    UPDATE_STATE_RECEIVING = 0x02,   /* 正在接收固件 (中途断电则恢复为需重传) */
    UPDATE_STATE_ABORT     = 0xFF    /* 上次升级失败, 回退到旧固件 */
} UpdateState_t;

/* 目标分区 */
typedef enum
{
    TARGET_NONE = 0,
    TARGET_APP_A = 1,               /* 升级目标为 A 区 */
    TARGET_APP_B = 2                /* 升级目标为 B 区 */
} TargetArea_t;

typedef struct
{
    uint32_t magic;                 /* BOOT_FLAG_MAGIC */
    uint32_t version;               /* 新固件版本号 */
    uint32_t length;                /* 新固件长度 */
    uint32_t crc32;                 /* 新固件 CRC32 */
    uint8_t  state;                 /* UpdateState_t */
    uint8_t  target;                /* TargetArea_t: 新固件写入哪个区 */
    uint8_t  reserved[6];           /* 对齐保留 */
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
 *          - 有 PENDING 标志: 校验新固件 -> 成功则切换启动, 失败则回退旧固件
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
 * @brief   擦除 Flash 指定区域 (按页擦除)
 * @param   addr: 起始地址 (页对齐)
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
 * @brief   计算 CRC32 (软件实现, 上位机打包工具需使用相同算法)
 * @param   data: 数据指针
 * @param   len:  数据长度
 * @retval  CRC32 值
 */
uint32_t Bootloader_CRC32(const uint8_t *data, uint32_t len);

/**
 * @brief   校验 App 区固件完整性 (长度 + CRC32, 与固件头比对)
 * @param   app_addr: 待校验的 App 区地址
 * @param   hdr:      固件头指针
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
