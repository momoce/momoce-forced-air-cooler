#include "bootloader.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* ============================================================================
 * CRC16/MODBUS 计算 (支持长度 > 65535)
 * 与 crc16.h 中的 Modbus_CRC16 算法完全一致:
 *   多项式 0x8005 (反射 0xA001), 初值 0xFFFF
 * ==========================================================================*/
static uint16_t Bootloader_CRC16_Calc(const uint8_t *data, uint32_t len)
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

/* ============================================================================
 * 根据地址返回扇区编号，失败返回 -1
 * STM32F401RCT6 (256KB): S0~S3 = 16KB, S4 = 64KB, S5 = 128KB
 * ==========================================================================*/
static int32_t Bootloader_GetSector(uint32_t addr)
{
    if (addr >= 0x08000000 && addr < 0x08004000) return FLASH_SECTOR_0;
    if (addr >= 0x08004000 && addr < 0x08008000) return FLASH_SECTOR_1;
    if (addr >= 0x08008000 && addr < 0x0800C000) return FLASH_SECTOR_2;
    if (addr >= 0x0800C000 && addr < 0x08010000) return FLASH_SECTOR_3;
    if (addr >= 0x08010000 && addr < 0x08020000) return FLASH_SECTOR_4;
    if (addr >= 0x08020000 && addr < 0x08040000) return FLASH_SECTOR_5;
    return -1;
}

/* ============================================================================
 * 擦除 Flash 指定区域 (逐扇区)
 * ==========================================================================*/
bool Bootloader_FlashErase(uint32_t addr, uint32_t size)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error = 0;
    uint32_t start_addr = addr;
    uint32_t end_addr   = addr + size;

    if (size == 0) return true;

    HAL_FLASH_Unlock();

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR |
                           FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                           FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    while (start_addr < end_addr)
    {
        int32_t sector = Bootloader_GetSector(start_addr);
        if (sector < 0)
        {
            HAL_FLASH_Lock();
            return false;
        }

        erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
        erase.Sector       = (uint32_t)sector;
        erase.NbSectors    = 1;
        erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;   /* 2.7V ~ 3.6V */

        if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return false;
        }

        /* 跳到下一个扇区的起始地址 */
        if      (sector <= FLASH_SECTOR_3) start_addr += 0x4000;   /* 16KB */
        else if (sector == FLASH_SECTOR_4) start_addr  = 0x08020000; /* 64KB */
        else if (sector == FLASH_SECTOR_5) start_addr  = 0x08040000; /* 128KB */
        else { HAL_FLASH_Lock(); return false; }
    }

    HAL_FLASH_Lock();
    return true;
}

/* ============================================================================
 * 向 Flash 写数据
 * 优化: 先按字 (4 字节) 写, 剩余不足 4 字节的按字节写
 * ==========================================================================*/
bool Bootloader_FlashWrite(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t i = 0;

    if (data == NULL || len == 0) return true;

    HAL_FLASH_Unlock();

    /* 按字写 (地址必须 4 字节对齐才能用) */
    while ((i + 4 <= len) && ((addr + i) % 4 == 0))
    {
        uint32_t word = (uint32_t)data[i]
                      | ((uint32_t)data[i + 1] << 8)
                      | ((uint32_t)data[i + 2] << 16)
                      | ((uint32_t)data[i + 3] << 24);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return false;
        }
        i += 4;
    }

    /* 剩余字节按字节写 */
    while (i < len)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr + i, data[i]) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return false;
        }
        i++;
    }

    HAL_FLASH_Lock();
    return true;
}

/* ============================================================================
 * 校验 App 区固件 (magic + length + CRC16)
 * ==========================================================================*/
bool Bootloader_VerifyFirmware(uint32_t app_addr, const FirmwareHeader_t *hdr)
{
    if (hdr == NULL) return false;
    if (hdr->magic != FW_HEADER_MAGIC) return false;
    if (hdr->length == 0 || hdr->length > APP_MAX_SIZE) return false;
    if (app_addr < APP_A_ADDR || app_addr >= FLAG_ADDR) return false;

    /* CRC 只对固件数据部分 (跳过 16 字节头) 计算 */
    const uint8_t *fw_data = (const uint8_t *)(app_addr + FW_HEADER_SIZE);
    uint16_t crc_calc = Bootloader_CRC16_Calc(fw_data, hdr->length);

    return (crc_calc == hdr->crc16);
}

/* ============================================================================
 * 解析接收缓冲区中的固件头
 * ==========================================================================*/
bool Bootloader_ParseHeader(const uint8_t *buf, FirmwareHeader_t *hdr)
{
    if (buf == NULL || hdr == NULL) return false;

    memcpy(hdr, buf, sizeof(FirmwareHeader_t));
    return (hdr->magic == FW_HEADER_MAGIC);
}

/* ============================================================================
 * 内部辅助: 检查 App 区是否具备基本可跳转条件
 * ==========================================================================*/
static bool Bootloader_IsAppValid(uint32_t app_addr)
{
    uint32_t stack_top = *(__IO uint32_t *)app_addr;
    uint32_t reset_vec = *(__IO uint32_t *)(app_addr + 4);

    /* STM32F401RC RAM: 0x20000000 ~ 0x2000FFFF (64KB) */
    if (stack_top < 0x20000000 || stack_top > 0x20010000)
        return false;

    /* 复位向量必须在 Flash 区, 且 Thumb 位为 1 */
    if (reset_vec < 0x08000000 || reset_vec > 0x08040000)
        return false;
    if ((reset_vec & 1) == 0)
        return false;

    return true;
}

/* ============================================================================
 * 跳转到指定 App 地址执行
 * ==========================================================================*/
void Bootloader_JumpToApp(uint32_t app_addr)
{
    typedef void (*pFunction)(void);
    pFunction JumpToApplication;
    uint32_t stack_top;
    uint32_t jump_addr;

    if (!Bootloader_IsAppValid(app_addr))
    {
        /* App 无效, 停留在 Bootloader, 可加 LED 提示 */
        return;
    }

    stack_top = *(__IO uint32_t *)app_addr;
    jump_addr = *(__IO uint32_t *)(app_addr + 4);

    /* 关闭全局中断 */
    __disable_irq();

    /* 关闭 SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* 清除所有中断使能与挂起标志 */
    for (uint8_t i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    /* 设置主堆栈指针 */
    __set_MSP(stack_top);

    /* 设置向量表偏移 */
    SCB->VTOR = app_addr;

    /* 跳转 (在 App 的 main 里会重新初始化中断) */
    JumpToApplication = (pFunction)jump_addr;
    JumpToApplication();

    /* 不会返回 */
}

/* ============================================================================
 * Bootloader 主流程: 判断是否需要升级
 * ==========================================================================*/
void Bootloader_Run(void)
{
    BootFlag_t flag;
    FirmwareHeader_t hdr;
    uint32_t target_addr   = APP_A_ADDR;
    uint32_t fallback_addr = APP_B_ADDR;

    /* 1. 读取 FLAG 区升级标志 */
    if (Bootloader_GetFlag(&flag))
    {
        switch (flag.state)
        {
        /* ----------------------------------------------------
         * 已收到完整固件, 待校验并切换
         * -------------------------------------------------- */
        case UPDATE_STATE_PENDING:
            if (flag.target == TARGET_APP_A)
            {
                target_addr   = APP_A_ADDR;
                fallback_addr = APP_B_ADDR;
            }
            else if (flag.target == TARGET_APP_B)
            {
                target_addr   = APP_B_ADDR;
                fallback_addr = APP_A_ADDR;
            }
            else
            {
                Bootloader_ClearFlag();
                Bootloader_JumpToApp(APP_A_ADDR);
                break;
            }

            /* 用升级标志中的信息构造固件头用于校验 */
            hdr.magic    = FW_HEADER_MAGIC;
            hdr.version  = flag.version;
            hdr.length   = flag.length;
            hdr.crc16    = flag.crc16;
            hdr.reserved = 0;

            if (Bootloader_VerifyFirmware(target_addr, &hdr))
            {
                Bootloader_ClearFlag();
                Bootloader_JumpToApp(target_addr);
            }
            else
            {
                /* 校验失败: 标记 ABORT, 回退旧固件 */
                flag.state = UPDATE_STATE_ABORT;
                Bootloader_SetFlag(&flag);
                Bootloader_JumpToApp(fallback_addr);
            }
            break;

        /* ----------------------------------------------------
         * 上次正在接收固件, 中途断电 / 未完成
         * 这里选择清除标志并回退到旧固件
         * -------------------------------------------------- */
        case UPDATE_STATE_RECEIVING:
            Bootloader_ClearFlag();
            Bootloader_JumpToApp(APP_A_ADDR);
            break;

        /* ----------------------------------------------------
         * 上次升级失败, 回退旧固件
         * -------------------------------------------------- */
        case UPDATE_STATE_ABORT:
            Bootloader_ClearFlag();
            Bootloader_JumpToApp(APP_A_ADDR);
            break;

        /* ----------------------------------------------------
         * 无升级任务, 直接启动 App
         * -------------------------------------------------- */
        case UPDATE_STATE_IDLE:
        default:
            Bootloader_ClearFlag();
            Bootloader_JumpToApp(APP_A_ADDR);
            break;
        }
    }
    else
    {
        /* 标志无效, 无升级任务, 直接启动默认 App */
        Bootloader_JumpToApp(APP_A_ADDR);
    }

    /* 如果跳转失败, 停留在 Bootloader */
    while (1)
    {
        /* 可加入 LED 闪烁指示错误 */
    }
}

/* ============================================================================
 * FLAG 区读写
 * ==========================================================================*/

bool Bootloader_GetFlag(BootFlag_t *flag)
{
    if (flag == NULL) return false;

    memcpy(flag, (const void *)FLAG_ADDR, sizeof(BootFlag_t));
    return (flag->magic == BOOT_FLAG_MAGIC);
}

bool Bootloader_SetFlag(const BootFlag_t *flag)
{
    if (flag == NULL) return false;

    if (!Bootloader_FlashErase(FLAG_ADDR, FLAG_SIZE))
        return false;

    return Bootloader_FlashWrite(FLAG_ADDR,
                                 (const uint8_t *)flag,
                                 sizeof(BootFlag_t));
}

bool Bootloader_ClearFlag(void)
{
    return Bootloader_FlashErase(FLAG_ADDR, FLAG_SIZE);
}