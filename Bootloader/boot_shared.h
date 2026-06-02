#ifndef BOOT_SHARED_H
#define BOOT_SHARED_H

#include <stdint.h>

/* STM32F411CEU6 Flash 总容量 512KB，地址范围 0x08000000 - 0x0807FFFF。 */
#define BOOT_FLASH_BASE_ADDR      ((uint32_t)0x08000000U)
#define BOOT_FLASH_END_ADDR       ((uint32_t)0x08080000U)

/* Bootloader 固定占用前 64KB，对应 Sector 0-3。 */
#define BOOTLOADER_ADDR           ((uint32_t)0x08000000U)
#define BOOTLOADER_SIZE           ((uint32_t)0x00010000U)

/* 元数据区保存升级状态、CRC、版本号等信息，对应 Sector 4。 */
#define BOOT_META_ADDR            ((uint32_t)0x08010000U)
#define BOOT_META_SIZE            ((uint32_t)0x00010000U)

/* 当前运行的 APP 区，对应 Sector 5。 */
#define BOOT_APP_ADDR             ((uint32_t)0x08020000U)
#define BOOT_APP_SIZE             ((uint32_t)0x00020000U)

/* 下载区保存待升级的新固件，对应 Sector 6。 */
#define BOOT_DOWNLOAD_ADDR        ((uint32_t)0x08040000U)
#define BOOT_DOWNLOAD_SIZE        ((uint32_t)0x00020000U)

/* 备份区保存旧 APP，用于新固件试运行失败后的回滚，对应 Sector 7。 */
#define BOOT_BACKUP_ADDR          ((uint32_t)0x08060000U)
#define BOOT_BACKUP_SIZE          ((uint32_t)0x00020000U)

/* APP 向量表首字必须落在 SRAM 范围内，Bootloader 用它判断镜像基本合法性。 */
#define BOOT_SRAM_START           ((uint32_t)0x20000000U)
#define BOOT_SRAM_END             ((uint32_t)0x20020000U)

/* 元数据协议标识，magic 为 ASCII "BTW1"。 */
#define BOOT_META_MAGIC           ((uint32_t)0x42545731U) /* BTW1 */
#define BOOT_META_VERSION         ((uint32_t)0x00000001U)

/* state 字段状态机：
 * NONE：无升级任务。
 * UPDATE_PENDING：下载区已有新固件，Bootloader 下次启动时安装。
 * TRIAL：新固件已安装，等待 APP 启动后确认。
 * CONFIRMED：APP 已确认可用。
 * ROLLBACK：已从备份区回滚旧固件。
 */
#define BOOT_FLAG_NONE            ((uint32_t)0xFFFFFFFFU)
#define BOOT_FLAG_UPDATE_PENDING  ((uint32_t)0xA55A0001U)
#define BOOT_FLAG_TRIAL           ((uint32_t)0xA55A0002U)
#define BOOT_FLAG_CONFIRMED       ((uint32_t)0xA55A0003U)
#define BOOT_FLAG_ROLLBACK        ((uint32_t)0xA55A0004U)

#define BOOT_META_CONFIRMED_VERSION_INDEX  0U
#define BOOT_META_PREVIOUS_VERSION_INDEX   1U

/* Bootloader、APP、打包脚本共享的元数据格式，必须保持字段顺序和大小一致。 */
typedef struct {
    uint32_t magic;         /* 固定为 BOOT_META_MAGIC，用于判断元数据是否有效。 */
    uint32_t version;       /* 元数据结构版本，用于后续兼容升级。 */
    uint32_t state;         /* 当前升级状态，取 BOOT_FLAG_xxx。 */
    uint32_t image_size;    /* 下载区有效固件长度，单位字节。 */
    uint32_t image_crc;     /* 下载固件 CRC32，用于安装前后校验。 */
    uint32_t image_version; /* 候选 APP 版本号，本次 OTA 准备安装的固件版本。 */
    uint32_t boot_count;    /* TRIAL 状态下累计启动次数，超限则回滚。 */
    uint32_t reserved[9];   /* [0] 已确认版本，[1] 上一个版本，其余预留，结构体固定 64 字节。 */
} boot_meta_t;

#endif
