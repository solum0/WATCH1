#include "boot_shared.h"
#include "app_version.h"
#include "stm32f4xx_flash.h"
#include <stdint.h>

/* 擦除 Bootloader 和 APP 共享的元数据扇区。
 * BOOT_META_ADDR 位于 Flash Sector 4，STM32F4 Flash 写入前必须先按扇区擦除。
 */
static int AppBoot_EraseMeta(void)
{
    return FLASH_EraseSector(FLASH_Sector_4, VoltageRange_3) == FLASH_COMPLETE;
}

/* 将更新后的启动元数据写回 Flash。
 * 元数据记录 APP 更新状态、镜像大小、CRC、版本号和试运行启动次数。
 */
static int AppBoot_WriteMeta(const boot_meta_t *meta)
{
    const uint32_t *src = (const uint32_t *)meta;
    uint32_t i;

    /* Flash 编程前必须解锁，并清除上一次操作遗留的错误/完成标志。 */
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    if (!AppBoot_EraseMeta()) {
        FLASH_Lock();
        return 0;
    }

    /* boot_meta_t 按 32bit 字对齐写入，和 Bootloader 侧读取格式保持一致。 */
    for (i = 0U; i < (sizeof(*meta) / sizeof(uint32_t)); ++i) {
        if (FLASH_ProgramWord(BOOT_META_ADDR + (i * 4U), src[i]) != FLASH_COMPLETE) {
            FLASH_Lock();
            return 0;
        }
    }

    FLASH_Lock();
    return 1;
}

/* APP 启动成功后调用本函数确认升级结果。
 * Bootloader 安装新固件后会把 state 置为 BOOT_FLAG_TRIAL；如果 APP 能运行到这里，
 * 就把状态改成 BOOT_FLAG_CONFIRMED，避免下次启动被 Bootloader 判定试运行失败并回滚。
 *
 * 返回值：
 * 1：当前确实是试运行固件，并且确认写回成功。
 * 0：元数据无效、当前不是试运行状态，或 Flash 写回失败。
 */
uint8_t AppBoot_ConfirmIfTrial(void)
{
    boot_meta_t meta;
    const volatile uint32_t *src = (const volatile uint32_t *)BOOT_META_ADDR;
    uint32_t *dst = (uint32_t *)&meta;
    uint32_t i;

    /* 从 Flash 元数据区复制到 RAM，避免直接频繁访问 Flash 结构体字段。 */
    for (i = 0U; i < (sizeof(meta) / sizeof(uint32_t)); ++i) {
        dst[i] = src[i];
    }

    /* magic/version 不匹配说明元数据不是当前协议格式，不能修改。 */
    if ((meta.magic != BOOT_META_MAGIC) || (meta.version != BOOT_META_VERSION)) {
        return 0U;
    }

    /* 只有试运行状态需要 APP 主动确认；已确认或无升级状态直接返回。 */
    if (meta.state != BOOT_FLAG_TRIAL) {
        return 0U;
    }

    if (meta.image_version != APP_FW_VERSION) {
        return 0U;
    }

    /* 确认成功后清零 boot_count，下次启动 Bootloader 会直接跳转 APP，不再回滚。 */
    meta.state = BOOT_FLAG_CONFIRMED;
    meta.boot_count = 0U;
    meta.reserved[BOOT_META_CONFIRMED_VERSION_INDEX] = APP_FW_VERSION;
    return (uint8_t)AppBoot_WriteMeta(&meta);
}
