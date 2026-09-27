#include "bootloader.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * 外部依赖: UART 句柄
 * ==========================================================================*/
extern UART_HandleTypeDef huart1;

/* ============================================================================
 * 状态上报 (用于上位机监控 Bootloader 运行)
 * ==========================================================================*/
static void Bootloader_Notify(const char *tag, const char *detail)
{
    char buf[96];
    int n;

    if (detail != NULL)
        n = snprintf(buf, sizeof(buf), "BOOT:%s,%s\r\n", tag, detail);
    else
        n = snprintf(buf, sizeof(buf), "BOOT:%s\r\n", tag);

    if (n > 0)
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)n, 100);
}

static void Bootloader_NotifyU32(const char *tag, const char *key, uint32_t val)
{
    char buf[96];
    int n = snprintf(buf, sizeof(buf), "BOOT:%s,%s=%lu\r\n",
                     tag, key, (unsigned long)val);
    if (n > 0)
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)n, 100);
}

/* ============================================================================
 * CRC16/MODBUS
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
 * 根据地址返回扇区编号
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
 * 擦除 Flash
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
        erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

        if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return false;
        }

        if      (sector <= FLASH_SECTOR_3) start_addr += 0x4000;
        else if (sector == FLASH_SECTOR_4) start_addr  = 0x08020000;
        else if (sector == FLASH_SECTOR_5) start_addr  = 0x08040000;
        else { HAL_FLASH_Lock(); return false; }
    }

    HAL_FLASH_Lock();
    return true;
}

/* ============================================================================
 * 写 Flash
 * ==========================================================================*/
bool Bootloader_FlashWrite(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t i = 0;

    if (data == NULL || len == 0) return true;

    HAL_FLASH_Unlock();

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
 * 校验固件
 * ==========================================================================*/
bool Bootloader_VerifyFirmware(uint32_t app_addr, const FirmwareHeader_t *hdr)
{
    if (hdr == NULL) return false;
    if (hdr->magic != FW_HEADER_MAGIC) return false;
    if (hdr->length == 0 || hdr->length > APP_MAX_SIZE) return false;
    if (app_addr < APP_A_ADDR || app_addr >= 0x08040000) return false;

    const uint8_t *fw_data = (const uint8_t *)app_addr;
    uint16_t crc_calc = Bootloader_CRC16_Calc(fw_data, hdr->length);

    return (crc_calc == hdr->crc16);
}

/* ============================================================================
 * 解析固件头
 * ==========================================================================*/
bool Bootloader_ParseHeader(const uint8_t *buf, FirmwareHeader_t *hdr)
{
    if (buf == NULL || hdr == NULL) return false;

    memcpy(hdr, buf, sizeof(FirmwareHeader_t));
    return (hdr->magic == FW_HEADER_MAGIC);
}

/* ============================================================================
 * App 可跳转性检查
 * ==========================================================================*/
static bool Bootloader_IsAppValid(uint32_t app_addr)
{
    uint32_t stack_top = *(__IO uint32_t *)app_addr;
    uint32_t reset_vec = *(__IO uint32_t *)(app_addr + 4);

    if (stack_top < 0x20000000 || stack_top > 0x20010000)
        return false;

    if (reset_vec < 0x08000000 || reset_vec > 0x08040000)
        return false;
    if ((reset_vec & 1) == 0)
        return false;

    return true;
}

/* ============================================================================
 * 跳转到 App
 * ==========================================================================*/
void Bootloader_JumpToApp(uint32_t app_addr)
{
    typedef void (*pFunction)(void);
    pFunction JumpToApplication;
    uint32_t stack_top;
    uint32_t jump_addr;

    if (!Bootloader_IsAppValid(app_addr))
    {
        Bootloader_Notify("JUMP_FAIL", "app_invalid");
        return;
    }

    stack_top = *(__IO uint32_t *)app_addr;
    jump_addr = *(__IO uint32_t *)(app_addr + 4);

    Bootloader_NotifyU32("JUMP", "addr", app_addr);

    /* 给上位机留点时间把消息收完 */
    HAL_Delay(20);

    __disable_irq();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    for (uint8_t i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    __set_MSP(stack_top);
    SCB->VTOR = app_addr;

    JumpToApplication = (pFunction)jump_addr;
    JumpToApplication();

    /* 不会返回 */
}

/* ============================================================================
 * Bootloader 主流程
 * ==========================================================================*/
void Bootloader_Run(void)
{
    BootFlag_t flag;
    FirmwareHeader_t hdr;
    uint32_t target_addr   = APP_A_ADDR;
    uint32_t fallback_addr = APP_A_ADDR;

    Bootloader_Notify("POWERON", NULL);

    if (Bootloader_GetFlag(&flag))
    {
        Bootloader_NotifyU32("FLAG_VALID", "state", flag.state);
        Bootloader_NotifyU32("FLAG_INFO", "target", flag.target);
        Bootloader_NotifyU32("FLAG_INFO", "length", flag.length);
        Bootloader_NotifyU32("FLAG_INFO", "crc", flag.crc16);

        switch (flag.state)
        {
        /* --------------------------------------------------
         * 待搬运: CACHE 区已有完整固件
         * -------------------------------------------------- */
        case UPDATE_STATE_PENDING:
        {
            Bootloader_Notify("STATE", "PENDING");

            /* 决定目标区 */
            if (flag.target == TARGET_APP_A)
            {
                target_addr   = APP_A_ADDR;
                fallback_addr = APP_A_ADDR;
            }
            else if (flag.target == TARGET_APP_B)
            {
                target_addr   = APP_B_ADDR;
                fallback_addr = APP_A_ADDR;
            }
            else
            {
                Bootloader_Notify("TARGET_INVALID", NULL);
                Bootloader_ClearFlag();
                Bootloader_JumpToApp(APP_A_ADDR);
                break;
            }

            Bootloader_NotifyU32("TARGET", "addr", target_addr);

            /* 构造固件头用于校验 */
            hdr.magic    = FW_HEADER_MAGIC;
            hdr.version  = flag.version;
            hdr.length   = flag.length;
            hdr.crc16    = flag.crc16;
            hdr.reserved = 0;

            /* ---- 1. 校验 CACHE 区 ---- */
            Bootloader_Notify("VERIFY", "start");
            if (!Bootloader_VerifyFirmware(CACHE_ADDR, &hdr))
            {
                Bootloader_Notify("VERIFY", "fail");
                flag.state = UPDATE_STATE_ABORT;
                Bootloader_SetFlag(&flag);
                Bootloader_JumpToApp(fallback_addr);
                break;
            }
            Bootloader_Notify("VERIFY", "ok");

            /* ---- 2. 擦除目标区 ---- */
            Bootloader_Notify("ERASE", "start");
            if (!Bootloader_FlashErase(target_addr, flag.length))
            {
                Bootloader_Notify("ERASE", "fail");
                Bootloader_JumpToApp(fallback_addr);
                break;
            }
            Bootloader_Notify("ERASE", "ok");

            /* ---- 3. 搬运 CACHE -> 目标区 ---- */
            Bootloader_Notify("COPY", "start");
            {
                const uint8_t *src = (const uint8_t *)CACHE_ADDR;
                if (!Bootloader_FlashWrite(target_addr, src, flag.length))
                {
                    Bootloader_Notify("COPY", "fail");
                    Bootloader_JumpToApp(fallback_addr);
                    break;
                }
            }
            Bootloader_Notify("COPY", "ok");

            /* ---- 4. 清 FLAG ---- */
            Bootloader_Notify("FLAG_CLEAR", "start");
            Bootloader_ClearFlag();
            Bootloader_Notify("FLAG_CLEAR", "ok");

            /* ---- 5. 跳新固件 ---- */
            Bootloader_Notify("JUMP_NEW", NULL);
            Bootloader_JumpToApp(target_addr);
            break;
        }

        /* --------------------------------------------------
         * 上次正在接收, 中途断电: 回退
         * -------------------------------------------------- */
        case UPDATE_STATE_RECEIVING:
            Bootloader_Notify("STATE", "RECEIVING");
            Bootloader_Notify("ROLLBACK", "receiving_incomplete");
            Bootloader_ClearFlag();
            Bootloader_JumpToApp(APP_A_ADDR);
            break;

        /* --------------------------------------------------
         * 上次升级失败: 回退
         * -------------------------------------------------- */
        case UPDATE_STATE_ABORT:
            Bootloader_Notify("STATE", "ABORT");
            Bootloader_Notify("ROLLBACK", "abort");
            Bootloader_ClearFlag();
            Bootloader_JumpToApp(APP_A_ADDR);
            break;

        /* --------------------------------------------------
         * 无任务
         * -------------------------------------------------- */
        case UPDATE_STATE_IDLE:
        default:
            Bootloader_Notify("STATE", "IDLE");
            Bootloader_ClearFlag();
            Bootloader_JumpToApp(APP_A_ADDR);
            break;
        }
    }
    else
    {
        Bootloader_Notify("STATE", "NO_FLAG");
        Bootloader_JumpToApp(APP_A_ADDR);
    }

    Bootloader_Notify("HALT", "jump_failed");
    while (1)
    {
        /* 跳转失败, 停留 */
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