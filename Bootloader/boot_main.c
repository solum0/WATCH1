/*
启动
  |
  v
读取 metadata
  |
  +-- 如果有待升级固件 -> 校验下载区 -> 备份旧 APP -> 安装新 APP -> 标记 TRIAL
  |
  +-- 如果是 TRIAL 状态 -> 检查 APP 是否已确认
          |
          +-- 没确认且超过次数 -> 从备份区回滚旧 APP
  |
  v
检查当前 APP 是否有效
  |
  +-- 有效 -> 跳转到 APP
  |
  +-- 无效 -> 停在 Bootloader 死循环

  如果移植其他STM32系列：
  1. F4 Pack、F4 启动文件、F4 CMSIS、F4 Flash 驱动。
  2. Flash 固定分区
  3. Page擦除还是sector擦除？
  4. APP 侧也要改：ota_update.c (line 269) 和 boot_confirm.c (line 10)
*/

/*
    Boot_ImageVectorIsValid()：检查 APP 镜像的向量表是否合法。
    Boot_CalcCrc32()：计算固件 CRC32。
    Boot_EraseRange()：擦除指定 Flash 区域。
    Boot_CopyRange()：把下载区/备份区的数据复制到 APP 区。
    Boot_ReadMeta() / Boot_WriteMeta()：读取和写入升级状态信息。
    Boot_InstallDownloadedImage()：把下载区的新固件安装到 APP 区。
    Boot_RestoreBackup()：升级失败或试运行失败时恢复旧 APP。
    Boot_ProcessUpdateState()：根据 metadata 状态决定升级、计数或回滚。
    Boot_JumpToApp()：设置向量表、MSP，然后跳转到 APP。
    main()：整个 Bootloader 的入口。
*/
#include "boot_shared.h"
#include "stm32f4xx.h"
#include "stm32f4xx_flash.h"
#include <stdint.h>

#define BOOT_MAX_TRIAL_BOOT_COUNT  ((uint32_t)1U)
#define BOOT_FLASH_VOLTAGE_RANGE   VoltageRange_3

typedef void (*boot_app_entry_t)(void);

static int Boot_RestoreBackup(boot_meta_t *meta);

/* 检查镜像向量表是否基本可信。
 * 向量表第 0 个字是初始 MSP，必须指向 SRAM；
 * 第 1 个字是 Reset_Handler，Thumb 位必须为 1，代码地址必须落在运行区间。
 */
static int Boot_ImageVectorIsValid(uint32_t image_addr, uint32_t runtime_addr)
{
    uint32_t stack = *(volatile uint32_t *)image_addr;
    uint32_t reset = *(volatile uint32_t *)(image_addr + 4U);
    uint32_t reset_code = reset & ~1U;

    if ((stack < BOOT_SRAM_START) || (stack > BOOT_SRAM_END) || ((stack & 3U) != 0U)) {
        return 0;
    }

    if (((reset & 1U) == 0U) ||
        (reset_code < runtime_addr) ||
        (reset_code >= (runtime_addr + BOOT_APP_SIZE))) {
        return 0;
    }

    return 1;
}

static int Boot_AppImageIsValid(uint32_t image_addr)
{
    return Boot_ImageVectorIsValid(image_addr, BOOT_APP_ADDR);
}

static int Boot_CurrentAppIsValid(void)
{
    return Boot_AppImageIsValid(BOOT_APP_ADDR);
}

/* 软件 CRC32，算法参数需要和 Bootloader/make_update_package.py 打包时保持一致。 */
static uint32_t Boot_CalcCrc32(uint32_t addr, uint32_t size)
{
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t i;
    uint8_t bit;

    for (i = 0U; i < size; ++i) {
        crc ^= (uint32_t)(*(volatile uint8_t *)(addr + i));
        for (bit = 0U; bit < 8U; ++bit) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1U) ^ 0xEDB88320U;
            } else {
                crc >>= 1U;
            }
        }
    }

    return ~crc;
}

/* 根据地址换算 STM32F411 的 Flash Sector 编号。 */
static uint32_t Boot_SectorFromAddress(uint32_t addr)
{
    if (addr < 0x08004000U) { return FLASH_Sector_0; }
    if (addr < 0x08008000U) { return FLASH_Sector_1; }
    if (addr < 0x0800C000U) { return FLASH_Sector_2; }
    if (addr < 0x08010000U) { return FLASH_Sector_3; }
    if (addr < 0x08020000U) { return FLASH_Sector_4; }
    if (addr < 0x08040000U) { return FLASH_Sector_5; }
    if (addr < 0x08060000U) { return FLASH_Sector_6; }
    return FLASH_Sector_7;
}

/* 擦除指定 Flash 范围覆盖到的所有扇区。
 * F411 前 4 个扇区为 16KB，Sector 4 为 64KB，Sector 5-7 为 128KB。
 */
static int Boot_EraseRange(uint32_t addr, uint32_t size)
{
    uint32_t end = addr + size;
    uint32_t cur = addr;

    while (cur < end) {
        if (FLASH_EraseSector(Boot_SectorFromAddress(cur), BOOT_FLASH_VOLTAGE_RANGE) != FLASH_COMPLETE) {
            return 0;
        }

        if (cur < 0x08010000U) {
            cur += 0x00004000U;
        } else if (cur < 0x08020000U) {
            cur += 0x00010000U;
        } else {
            cur += 0x00020000U;
        }
    }

    return 1;
}

static int Boot_ProgramWord(uint32_t addr, uint32_t data)
{
    return FLASH_ProgramWord(addr, data) == FLASH_COMPLETE;
}

/* 擦写 APP 区后刷新 Flash I/D Cache，避免跳转后读到旧指令或旧数据。 */
static void Boot_FlushFlashCache(void)
{
    FLASH_InstructionCacheCmd(DISABLE);
    FLASH_DataCacheCmd(DISABLE);
    FLASH_InstructionCacheReset();
    FLASH_DataCacheReset();
    FLASH_InstructionCacheCmd(ENABLE);
    FLASH_DataCacheCmd(ENABLE);
}

/* 按 32bit 字复制 Flash 区域，目标区域必须已提前擦除。 */
static int Boot_CopyRange(uint32_t dst, uint32_t src, uint32_t size)
{
    uint32_t offset;

    for (offset = 0U; offset < size; offset += 4U) {
        uint32_t word = *(volatile uint32_t *)(src + offset);
        if (!Boot_ProgramWord(dst + offset, word)) {
            return 0;
        }
    }

    return 1;
}

/* 从元数据区读取 boot_meta_t 到 RAM。 */
static void Boot_ReadMeta(boot_meta_t *meta)
{
    const volatile uint32_t *src = (const volatile uint32_t *)BOOT_META_ADDR;
    uint32_t *dst = (uint32_t *)meta;
    uint32_t i;

    for (i = 0U; i < (sizeof(*meta) / sizeof(uint32_t)); ++i) {
        dst[i] = src[i];
    }
}

static int Boot_MetaIsValid(const boot_meta_t *meta)
{
    return (meta->magic == BOOT_META_MAGIC) && (meta->version == BOOT_META_VERSION);
}

/* 写回元数据。Flash 只能从 1 写成 0，更新元数据前必须擦除整个元数据扇区。 */
static int Boot_WriteMeta(const boot_meta_t *meta)
{
    const uint32_t *src = (const uint32_t *)meta;
    uint32_t i;

    if (!Boot_EraseRange(BOOT_META_ADDR, BOOT_META_SIZE)) {
        return 0;
    }

    for (i = 0U; i < (sizeof(*meta) / sizeof(uint32_t)); ++i) {
        if (!Boot_ProgramWord(BOOT_META_ADDR + (i * 4U), src[i])) {
            return 0;
        }
    }

    return 1;
}

/* 同时校验镜像大小、向量表和 CRC，防止把无效下载内容安装到 APP 区。 */
static int Boot_ImageMatches(uint32_t addr, uint32_t size, uint32_t expected_crc)
{
    if ((size == 0U) || (size > BOOT_APP_SIZE)) {
        return 0;
    }

    if (!Boot_AppImageIsValid(addr)) {
        return 0;
    }

    return Boot_CalcCrc32(addr, size) == expected_crc;
}

static int Boot_ImageVersionIsAllowed(const boot_meta_t *meta)
{
    uint32_t confirmed_version = meta->reserved[BOOT_META_CONFIRMED_VERSION_INDEX];

    if (confirmed_version == 0xFFFFFFFFU) {
        confirmed_version = 0U;
    }

    return meta->image_version >= confirmed_version;
}

/* 安装新固件前先备份当前 APP。
 * 如果当前 APP 本身无效，则只清空备份区并继续安装新固件。
 */
static int Boot_BackupCurrentApp(void)
{
    if (!Boot_EraseRange(BOOT_BACKUP_ADDR, BOOT_BACKUP_SIZE)) {
        return 0;
    }

    if (!Boot_CurrentAppIsValid()) {
        return 1;
    }

    return Boot_CopyRange(BOOT_BACKUP_ADDR, BOOT_APP_ADDR, BOOT_APP_SIZE);
}

/* 从下载区安装新 APP。
 * 任何擦写或校验失败都会尝试恢复备份，避免设备停留在不可启动状态。
 */
static int Boot_InstallDownloadedImage(boot_meta_t *meta)
{
    uint32_t previous_confirmed_version;

    if (!Boot_ImageVersionIsAllowed(meta)) {
        return 0;
    }

    if (!Boot_ImageMatches(BOOT_DOWNLOAD_ADDR, meta->image_size, meta->image_crc)) {
        return 0;
    }

    previous_confirmed_version = meta->reserved[BOOT_META_CONFIRMED_VERSION_INDEX];

    if (!Boot_BackupCurrentApp()) {
        return 0;
    }

    if (!Boot_EraseRange(BOOT_APP_ADDR, BOOT_APP_SIZE)) {
        (void)Boot_RestoreBackup(meta);
        return 0;
    }

    if (!Boot_CopyRange(BOOT_APP_ADDR, BOOT_DOWNLOAD_ADDR, meta->image_size)) {
        (void)Boot_RestoreBackup(meta);
        return 0;
    }

    Boot_FlushFlashCache();

    if (!Boot_ImageMatches(BOOT_APP_ADDR, meta->image_size, meta->image_crc)) {
        (void)Boot_RestoreBackup(meta);
        return 0;
    }

    /* 新 APP 首次启动必须由 APP 侧调用 AppBoot_ConfirmIfTrial() 确认。 */
    meta->state = BOOT_FLAG_TRIAL;
    meta->boot_count = 0U;
    meta->reserved[BOOT_META_PREVIOUS_VERSION_INDEX] = previous_confirmed_version;
    return Boot_WriteMeta(meta);
}

/* 将备份区旧 APP 恢复到 APP 区，并把状态标记为 ROLLBACK。 */
static int Boot_RestoreBackup(boot_meta_t *meta)
{
    if (!Boot_AppImageIsValid(BOOT_BACKUP_ADDR)) {
        return 0;
    }

    if (!Boot_EraseRange(BOOT_APP_ADDR, BOOT_APP_SIZE)) {
        return 0;
    }

    if (!Boot_CopyRange(BOOT_APP_ADDR, BOOT_BACKUP_ADDR, BOOT_APP_SIZE)) {
        return 0;
    }

    Boot_FlushFlashCache();

    meta->state = BOOT_FLAG_ROLLBACK;
    meta->boot_count = 0U;
    if (meta->reserved[BOOT_META_PREVIOUS_VERSION_INDEX] != 0xFFFFFFFFU) {
        meta->reserved[BOOT_META_CONFIRMED_VERSION_INDEX] =
            meta->reserved[BOOT_META_PREVIOUS_VERSION_INDEX];
    }
    return Boot_WriteMeta(meta);
}

/* Bootloader 启动时处理升级状态机。
 * UPDATE_PENDING：安装下载区固件。
 * TRIAL：如果 APP 上次没有确认成功，超过允许次数后回滚。
 */
static void Boot_ProcessUpdateState(void)
{
    boot_meta_t meta;

    Boot_ReadMeta(&meta);
    if (!Boot_MetaIsValid(&meta)) {
        return;
    }

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    if (meta.state == BOOT_FLAG_UPDATE_PENDING) {
        /* 安装失败时记录 ROLLBACK，便于后续诊断。 */
        if (!Boot_InstallDownloadedImage(&meta)) {
            meta.state = BOOT_FLAG_ROLLBACK;
            meta.boot_count = 0U;
            (void)Boot_WriteMeta(&meta);
        }
    } else if (meta.state == BOOT_FLAG_TRIAL) {
        /* APP 若未及时确认，下一次 Bootloader 启动会进入这里并触发回滚。 */
        if (meta.boot_count >= BOOT_MAX_TRIAL_BOOT_COUNT) {
            (void)Boot_RestoreBackup(&meta);
        } else {
            meta.boot_count++;
            (void)Boot_WriteMeta(&meta);
        }
    }

    FLASH_Lock();
}

/* 跳转 APP 前关闭 Bootloader 使用过的中断和 SysTick，避免影响 APP 初始化。 */
static void Boot_DeinitBeforeJump(void)
{
    uint32_t i;

    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    for (i = 0U; i < 8U; ++i) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
}

/* 设置 APP 向量表、主栈 MSP，然后跳到 APP Reset_Handler。 */
static void Boot_JumpToApp(uint32_t app_addr)
{
    uint32_t app_stack = *(volatile uint32_t *)app_addr;
    uint32_t app_reset = *(volatile uint32_t *)(app_addr + 4U);
    boot_app_entry_t app_entry = (boot_app_entry_t)app_reset;

    Boot_DeinitBeforeJump();

    SCB->VTOR = app_addr;
    __set_MSP(app_stack);
    __DSB();
    __ISB();

    app_entry();
}

int main(void)
{
    /* 先处理升级/回滚，再判断当前 APP 是否可启动。 */
    Boot_ProcessUpdateState();

    if (Boot_CurrentAppIsValid()) { /* 判定基本合法、可以启动的 APP 固件 */
        Boot_JumpToApp(BOOT_APP_ADDR);
    }

    /* 没有可用 APP 时停留在 Bootloader，方便后续调试或重新烧录。 */
    while (1) {
    }
}
