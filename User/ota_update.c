#include "ota_update.h"
#include "app_version.h"
#include "boot_shared.h"
#include "stm32f4xx.h"
#include "stm32f4xx_flash.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

#define OTA_FRAME_START             ((uint8_t)0x01U)
#define OTA_FRAME_DATA              ((uint8_t)0x02U)
#define OTA_FRAME_END               ((uint8_t)0x03U)
#define OTA_FRAME_ACK               ((uint8_t)0x79U)
#define OTA_FRAME_TAIL_PAD          ((uint8_t)0x55U)

#define OTA_STATUS_OK               ((uint8_t)0U)
#define OTA_STATUS_BAD_MAGIC        ((uint8_t)1U)
#define OTA_STATUS_BAD_SIZE         ((uint8_t)2U)
#define OTA_STATUS_BAD_SEQUENCE     ((uint8_t)3U)
#define OTA_STATUS_BAD_OFFSET       ((uint8_t)4U)
#define OTA_STATUS_BAD_PACKET_CRC   ((uint8_t)5U)
#define OTA_STATUS_FLASH_FAILED     ((uint8_t)6U)
#define OTA_STATUS_IMAGE_CRC_FAILED ((uint8_t)7U)
#define OTA_STATUS_VERSION_REJECTED ((uint8_t)8U)
#define OTA_STATUS_RX_TIMEOUT       ((uint8_t)9U)

#define OTA_START_LEN               ((uint16_t)19U)
#define OTA_DATA_HEADER_LEN         ((uint16_t)9U)
#define OTA_END_LEN                 ((uint16_t)9U)
#define OTA_MAX_PAYLOAD_SIZE        ((uint16_t)480U)
#define OTA_MAX_DATA_FRAME_SIZE     ((uint16_t)(OTA_DATA_HEADER_LEN + OTA_MAX_PAYLOAD_SIZE + 4U))
#define OTA_RX_FRAME_TIMEOUT_MS     ((uint32_t)1000U)
#define OTA_UI_FAILED_HOLD_MS       ((uint32_t)3000U)

typedef enum {
    OTA_RX_IDLE = 0,
    OTA_RX_START,
    OTA_RX_WAIT_FRAME,
    OTA_RX_DATA,
    OTA_RX_END
} ota_rx_state_t;

typedef struct {
    ota_rx_state_t rx_state;
    uint8_t transfer_active;
    uint8_t reset_after_ack;
    uint16_t rx_index;
    uint16_t rx_expected;
    uint16_t payload_size;
    uint16_t expected_seq;
    uint32_t image_size;
    uint32_t image_crc;
    uint32_t image_version;
    uint32_t received_size;
    TickType_t last_rx_tick;
    uint8_t rx_buf[OTA_MAX_DATA_FRAME_SIZE];
} ota_context_t;

static ota_context_t g_ota;
static volatile uint32_t g_ota_uart_rx_drop_count;
static volatile uint32_t g_ota_reset_csr;
static ota_update_status_t g_ota_status;
static uint8_t g_ota_display_active;
static TickType_t g_ota_ui_state_tick;
static ota_update_status_callback_t g_ota_status_callback;

static uint8_t Ota_CalcPercent(uint32_t received_size, uint32_t image_size)
{
    uint32_t percent;

    if (image_size == 0U) {
        return 0U;
    }

    if (received_size >= image_size) {
        return 100U;
    }

    percent = (received_size * 100U) / image_size;
    if (percent > 100U) {
        percent = 100U;
    }

    return (uint8_t)percent;
}

static void Ota_SetUiState(ota_update_ui_state_t ui_state, uint8_t error_status)
{
    g_ota_status.ui_state = ui_state;
    g_ota_status.image_size = g_ota.image_size;
    g_ota_status.received_size = g_ota.received_size;
    g_ota_status.image_version = g_ota.image_version;
    g_ota_status.percent = Ota_CalcPercent(g_ota.received_size, g_ota.image_size);
    g_ota_status.error_status = error_status;
    g_ota_ui_state_tick = xTaskGetTickCount();

    if (ui_state == OTA_UPDATE_UI_REBOOTING) {
        g_ota_status.received_size = g_ota.image_size;
        g_ota_status.percent = 100U;
    }

    g_ota_display_active = (ui_state != OTA_UPDATE_UI_IDLE) ? 1U : 0U;

    if (g_ota_status_callback != 0) {
        g_ota_status_callback(&g_ota_status);
    }
}

static void Ota_ResetContext(uint8_t clear_display)
{
    g_ota.rx_state = OTA_RX_IDLE;
    g_ota.transfer_active = 0U;
    g_ota.reset_after_ack = 0U;
    g_ota.rx_index = 0U;
    g_ota.rx_expected = 0U;
    g_ota.payload_size = 0U;
    g_ota.expected_seq = 0U;
    g_ota.image_size = 0U;
    g_ota.image_crc = 0U;
    g_ota.image_version = 0U;
    g_ota.received_size = 0U;
    g_ota.last_rx_tick = 0U;

    if (clear_display != 0U) {
        Ota_SetUiState(OTA_UPDATE_UI_IDLE, OTA_STATUS_OK);
    }
}

static void Ota_FailAndReset(uint8_t error_status)
{
    Ota_SetUiState(OTA_UPDATE_UI_FAILED, error_status);
    Ota_ResetContext(0U);
}

/**
 * @brief 从 Boot metadata 读取当前已确认版本。
 * @return 已确认 APP 版本；若 metadata 无效或尚未记录，则返回 APP_FW_VERSION。
 */
static uint32_t Ota_GetConfirmedVersion(void)
{
    boot_meta_t meta;
    const volatile uint32_t *src = (const volatile uint32_t *)BOOT_META_ADDR;
    uint32_t *dst = (uint32_t *)&meta;
    uint32_t i;
    uint32_t confirmed_version = APP_FW_VERSION;

    for (i = 0U; i < (sizeof(meta) / sizeof(uint32_t)); ++i) {
        dst[i] = src[i];
    }

    if ((meta.magic == BOOT_META_MAGIC) && (meta.version == BOOT_META_VERSION)) {
        uint32_t stored_version = meta.reserved[BOOT_META_CONFIRMED_VERSION_INDEX];
        if ((stored_version != 0xFFFFFFFFU) && (stored_version > confirmed_version)) {
            confirmed_version = stored_version;
        }
    }

    return confirmed_version;
}

/**
 * @brief 从小端格式字节流读取 16 位无符号数。
 * @param p 指向至少 2 字节数据的缓冲区。
 * @return 读取到的 16 位数值。
 */
static uint16_t Ota_ReadLe16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief 从小端格式字节流读取 32 位无符号数。
 * @param p 指向至少 4 字节数据的缓冲区。
 * @return 读取到的 32 位数值。
 */
static uint32_t Ota_ReadLe32(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/**
 * @brief 将 16 位无符号数按小端格式写入缓冲区。
 * @param p 指向至少 2 字节空间的缓冲区。
 * @param value 待写入的 16 位数值。
 */
static void Ota_WriteLe16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value & 0xFFU);
    p[1] = (uint8_t)((value >> 8) & 0xFFU);
}

/**
 * @brief 将 32 位无符号数按小端格式写入缓冲区。
 * @param p 指向至少 4 字节空间的缓冲区。
 * @param value 待写入的 32 位数值。
 */
static void Ota_WriteLe32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value & 0xFFU);
    p[1] = (uint8_t)((value >> 8) & 0xFFU);
    p[2] = (uint8_t)((value >> 16) & 0xFFU);
    p[3] = (uint8_t)((value >> 24) & 0xFFU);
}

/**
 * @brief 计算 RAM 缓冲区的 CRC32 校验值。
 * @param data 指向待校验数据的缓冲区。
 * @param size 待校验数据长度，单位为字节。
 * @return CRC32 校验值。
 */
static uint32_t Ota_Crc32Bytes(const uint8_t *data, uint32_t size)
{
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t i;
    uint8_t bit;

    for (i = 0U; i < size; ++i) {
        crc ^= (uint32_t)data[i];
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

/**
 * @brief 计算 Flash 指定地址范围内数据的 CRC32 校验值。
 * @param addr Flash 起始地址。
 * @param size 待校验数据长度，单位为字节。
 * @return CRC32 校验值。
 */
static uint32_t Ota_Crc32Flash(uint32_t addr, uint32_t size)
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

/**
 * @brief 通过调试串口发送 OTA 应答帧。
 * @param ack_type 应答对应的帧类型。
 * @param status OTA_STATUS_xxx 状态码。
 * @param seq 数据帧序号或期望序号。
 * @param offset 当前已接收偏移或附加调试信息。
 */
static void Ota_SendAck(uint8_t ack_type, uint8_t status, uint16_t seq, uint32_t offset)
{
    uint8_t ack[20];
    uint8_t i;

    ack[0] = OTA_FRAME_ACK;
    ack[1] = ack_type;
    ack[2] = status;
    Ota_WriteLe16(&ack[3], seq);
    Ota_WriteLe32(&ack[5], offset);
    ack[9] = (uint8_t)'D';
    ack[10] = (uint8_t)g_ota.rx_state;
    Ota_WriteLe16(&ack[11], g_ota.rx_index);
    Ota_WriteLe16(&ack[13], g_ota.rx_expected);
    Ota_WriteLe16(&ack[15], g_ota.expected_seq);
    Ota_WriteLe16(&ack[17], (uint16_t)g_ota_uart_rx_drop_count);
    ack[19] = 0xA5U;

    for (i = 0U; i < sizeof(ack); ++i) {
        Usart_SendByte(DEBUG_USARTx, ack[i]);
    }

    while (USART_GetFlagStatus(DEBUG_USARTx, USART_FLAG_TC) == RESET) {
    }
}

/**
 * @brief 开始收集一个新的 OTA 帧。
 * @param frame_type 帧类型，取 OTA_FRAME_START/DATA/END。
 * @param expected_len 当前已知的帧长度。
 */
static void Ota_BeginCollect(uint8_t frame_type, uint16_t expected_len)
{
    g_ota.rx_buf[0] = frame_type;
    g_ota.rx_index = 1U;
    g_ota.rx_expected = expected_len;
    g_ota.last_rx_tick = xTaskGetTickCount();

    if (frame_type == OTA_FRAME_START) {
        g_ota.rx_state = OTA_RX_START;
    } else if (frame_type == OTA_FRAME_DATA) {
        g_ota.rx_state = OTA_RX_DATA;
    } else {
        g_ota.rx_state = OTA_RX_END;
    }
}

/**
 * @brief 复位 OTA 接收上下文和传输状态。
 */
void Ota_UpdateReset(void)
{
    Ota_ResetContext(1U);
}

/**
 * @brief 获取 OTA 期间 UART 接收丢字节计数。
 * @return UART 接收丢字节次数。
 */
uint32_t Ota_UpdateGetUartDropCount(void)
{
    return g_ota_uart_rx_drop_count;
}

/**
 * @brief 获取启动后捕获到的 RCC 复位原因寄存器值。
 * @return RCC->CSR 的历史快照。
 */
uint32_t Ota_UpdateGetResetCsr(void)
{
    return g_ota_reset_csr;
}

/**
 * @brief 通知 OTA 模块发生了一次 UART 接收丢字节。
 */
void Ota_UpdateOnUartRxDrop(void)
{
    ++g_ota_uart_rx_drop_count;
}

/**
 * @brief 捕获并清除 MCU 复位原因标志。
 */
void Ota_UpdateCaptureResetCause(void)
{
    g_ota_reset_csr = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF;
}

/**
 * @brief 查询 OTA 是否正在接收或处理升级数据。
 * @return 非 0 表示 OTA 正在进行，0 表示空闲。
 */
uint8_t Ota_UpdateInProgress(void)
{
    return (uint8_t)((g_ota.rx_state != OTA_RX_IDLE) || (g_ota.transfer_active != 0U));
}

uint8_t Ota_UpdateDisplayActive(void)
{
    return g_ota_display_active;
}

void Ota_UpdateGetStatus(ota_update_status_t *status)
{
    if (status == 0) {
        return;
    }

    *status = g_ota_status;
}

void Ota_UpdateRegisterStatusCallback(ota_update_status_callback_t callback)
{
    g_ota_status_callback = callback;
}

/**
 * @brief 根据当前接收状态获取应答帧类型。
 * @return OTA_FRAME_START、OTA_FRAME_DATA 或 OTA_FRAME_END。
 */
static uint8_t Ota_CurrentAckType(void)
{
    if (g_ota.rx_state == OTA_RX_START) {
        return OTA_FRAME_START;
    }

    if (g_ota.rx_state == OTA_RX_END) {
        return OTA_FRAME_END;
    }

    return OTA_FRAME_DATA;
}

/**
 * @brief 清理当前帧接收状态并准备接收下一帧。
 */
static void Ota_PrepareForNextFrame(void)
{
    g_ota.rx_state = g_ota.transfer_active ? OTA_RX_WAIT_FRAME : OTA_RX_IDLE;
    g_ota.rx_index = 0U;
    g_ota.rx_expected = 0U;
    g_ota.last_rx_tick = 0U;
}

/**
 * @brief 轮询 OTA 接收超时状态。
 */
void Ota_UpdatePoll(void)
{
    TickType_t now = xTaskGetTickCount();
    uint8_t ack_type;
    uint16_t ack_seq;

    if ((g_ota_status.ui_state == OTA_UPDATE_UI_FAILED) &&
        ((uint32_t)((now - g_ota_ui_state_tick) * portTICK_PERIOD_MS) > OTA_UI_FAILED_HOLD_MS)) {
        Ota_SetUiState(OTA_UPDATE_UI_IDLE, OTA_STATUS_OK);
    }

    if ((g_ota.rx_state != OTA_RX_IDLE) && (g_ota.rx_state != OTA_RX_WAIT_FRAME) &&
        (g_ota.rx_index > 0U) &&
        ((uint32_t)((now - g_ota.last_rx_tick) * portTICK_PERIOD_MS) > OTA_RX_FRAME_TIMEOUT_MS)) {
        if (g_ota.transfer_active != 0U) {
            ack_type = Ota_CurrentAckType();
            ack_seq = (ack_type == OTA_FRAME_DATA) ? g_ota.expected_seq : 0U;
            Ota_SendAck(ack_type, OTA_STATUS_RX_TIMEOUT, ack_seq, g_ota.received_size);
        }
        Ota_FailAndReset(OTA_STATUS_RX_TIMEOUT);
    }
}

/**
 * @brief 复位 Flash 指令缓存和数据缓存，确保后续读取到最新内容。
 */
static void Ota_FlushFlashCache(void)
{
    FLASH_InstructionCacheCmd(DISABLE);
    FLASH_DataCacheCmd(DISABLE);
    FLASH_InstructionCacheReset();
    FLASH_DataCacheReset();
    FLASH_InstructionCacheCmd(ENABLE);
    FLASH_DataCacheCmd(ENABLE);
}

/**
 * @brief 擦除 OTA 下载区所在 Flash 扇区。
 * @return 1 表示擦除成功，0 表示擦除失败。
 */
static int Ota_EraseDownloadArea(void)
{
    FLASH_Status status;

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    status = FLASH_EraseSector(FLASH_Sector_6, VoltageRange_3);
    FLASH_Lock();

    Ota_FlushFlashCache();
    return status == FLASH_COMPLETE;
}

/**
 * @brief 将升级数据写入 OTA 下载区。
 * @param offset 相对于 BOOT_DOWNLOAD_ADDR 的写入偏移。
 * @param data 指向待写入数据的缓冲区。
 * @param len 待写入数据长度，单位为字节。
 * @return 1 表示写入成功，0 表示写入失败。
 */
static int Ota_WriteDownloadBytes(uint32_t offset, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    FLASH_Status status = FLASH_COMPLETE;

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    for (i = 0U; i < len; ++i) {
        status = FLASH_ProgramByte(BOOT_DOWNLOAD_ADDR + offset + i, data[i]);
        if (status != FLASH_COMPLETE) {
            break;
        }
    }

    FLASH_Lock();
    return status == FLASH_COMPLETE;
}

/**
 * @brief 校验下载区固件向量表是否合法。
 * @return 1 表示向量表有效，0 表示无效。
 */
static int Ota_DownloadVectorIsValid(void)
{
    uint32_t stack = *(volatile uint32_t *)BOOT_DOWNLOAD_ADDR;
    uint32_t reset = *(volatile uint32_t *)(BOOT_DOWNLOAD_ADDR + 4U);
    uint32_t reset_code = reset & ~1U;

    if ((stack < BOOT_SRAM_START) || (stack > BOOT_SRAM_END) || ((stack & 3U) != 0U)) {
        return 0;
    }

    if (((reset & 1U) == 0U) ||
        (reset_code < BOOT_APP_ADDR) ||
        (reset_code >= (BOOT_APP_ADDR + BOOT_APP_SIZE))) {
        return 0;
    }

    return 1;
}

/**
 * @brief 写入 Bootloader 元信息，标记新固件等待搬运和启动验证。
 * @return 1 表示写入成功，0 表示写入失败。
 */
static int Ota_WriteBootMeta(void)
{
    boot_meta_t meta;
    const uint32_t *src = (const uint32_t *)&meta;
    uint32_t i;
    uint32_t confirmed_version = Ota_GetConfirmedVersion();
    FLASH_Status status = FLASH_COMPLETE;

    meta.magic = BOOT_META_MAGIC;
    meta.version = BOOT_META_VERSION;
    meta.state = BOOT_FLAG_UPDATE_PENDING;
    meta.image_size = g_ota.image_size;
    meta.image_crc = g_ota.image_crc;
    meta.image_version = g_ota.image_version;
    meta.boot_count = 0U;
    for (i = 0U; i < 9U; ++i) {
        meta.reserved[i] = 0xFFFFFFFFU;
    }
    meta.reserved[BOOT_META_CONFIRMED_VERSION_INDEX] = confirmed_version;

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    status = FLASH_EraseSector(FLASH_Sector_4, VoltageRange_3);
    if (status == FLASH_COMPLETE) {
        for (i = 0U; i < (sizeof(meta) / sizeof(uint32_t)); ++i) {
            status = FLASH_ProgramWord(BOOT_META_ADDR + (i * 4U), src[i]);
            if (status != FLASH_COMPLETE) {
                break;
            }
        }
    }

    FLASH_Lock();
    return status == FLASH_COMPLETE;
}

/**
 * @brief 处理 OTA 起始帧并初始化一次升级传输。
 * 检查 magic 是否为 "OTA1"、固件大小是否合法、payload 大小是否合法，然后擦除下载区
 * BOOT_DOWNLOAD_ADDR = 0x08040000，也就是 Sector 6。成功后设置 transfer_active = 1，回复 ACK。
 */
static void Ota_HandleStartFrame(void)
{
    uint32_t image_size = Ota_ReadLe32(&g_ota.rx_buf[5]);
    uint32_t image_crc = Ota_ReadLe32(&g_ota.rx_buf[9]);
    uint32_t image_version = Ota_ReadLe32(&g_ota.rx_buf[13]);
    uint16_t payload_size = Ota_ReadLe16(&g_ota.rx_buf[17]);

    if ((g_ota.rx_buf[1] != (uint8_t)'O') ||
        (g_ota.rx_buf[2] != (uint8_t)'T') ||
        (g_ota.rx_buf[3] != (uint8_t)'A') ||
        (g_ota.rx_buf[4] != (uint8_t)'1')) {
        Ota_SendAck(OTA_FRAME_START, OTA_STATUS_BAD_MAGIC, 0U, 0U);
        Ota_FailAndReset(OTA_STATUS_BAD_MAGIC);
        return;
    }

    g_ota.image_size = image_size;
    g_ota.image_crc = image_crc;
    g_ota.image_version = image_version;
    g_ota.received_size = 0U;

    if ((image_size == 0U) || (image_size > BOOT_DOWNLOAD_SIZE) || (image_size > BOOT_APP_SIZE) ||
        (payload_size == 0U) || (payload_size > OTA_MAX_PAYLOAD_SIZE)) {
        Ota_SendAck(OTA_FRAME_START, OTA_STATUS_BAD_SIZE, 0U, 0U);
        Ota_FailAndReset(OTA_STATUS_BAD_SIZE);
        return;
    }

    if (image_version < Ota_GetConfirmedVersion()) {
        Ota_SendAck(OTA_FRAME_START, OTA_STATUS_VERSION_REJECTED, 0U, image_version);
        Ota_FailAndReset(OTA_STATUS_VERSION_REJECTED);
        return;
    }

    Ota_SetUiState(OTA_UPDATE_UI_RECEIVING, OTA_STATUS_OK);

    if (!Ota_EraseDownloadArea()) {
        Ota_SendAck(OTA_FRAME_START, OTA_STATUS_FLASH_FAILED, 0U, 0U);
        Ota_FailAndReset(OTA_STATUS_FLASH_FAILED);
        return;
    }

    g_ota.transfer_active = 1U;
    g_ota.payload_size = payload_size;
    g_ota.expected_seq = 0U;
    g_ota.rx_state = OTA_RX_WAIT_FRAME;
    Ota_SetUiState(OTA_UPDATE_UI_RECEIVING, OTA_STATUS_OK);

    Ota_SendAck(OTA_FRAME_START, OTA_STATUS_OK, 0U, Ota_UpdateGetResetCsr());
}

/**
 * @brief 判断当前数据帧是否为上一帧的重复发送。
 * @param seq 当前数据帧序号。
 * @param offset 当前数据帧写入偏移。
 * @param len 当前数据帧负载长度。
 * @return 非 0 表示重复帧，0 表示不是重复帧。
 */
static uint8_t Ota_DataFrameIsDuplicate(uint16_t seq, uint32_t offset, uint16_t len)
{
    uint16_t previous_seq = (uint16_t)(g_ota.expected_seq - 1U);

    return (uint8_t)((seq == previous_seq) && ((offset + len) == g_ota.received_size));
}

/**
 * @brief 处理 OTA 数据帧，完成校验、去重、顺序检查和 Flash 写入。
 */
static void Ota_HandleDataFrame(void)
{
    /* 数据帧格式：type(1) + seq(2) + offset(4) + len(2) + payload(len) + crc32(4)。 */
    uint16_t seq = Ota_ReadLe16(&g_ota.rx_buf[1]);
    uint32_t offset = Ota_ReadLe32(&g_ota.rx_buf[3]);
    uint16_t len = Ota_ReadLe16(&g_ota.rx_buf[7]);
    const uint8_t *payload = &g_ota.rx_buf[9];
    uint32_t expected_crc = Ota_ReadLe32(&g_ota.rx_buf[9U + len]);
    uint32_t next_offset = offset + len;

    /* 先校验当前包自身的 CRC，错误时不写 Flash，等待上位机按 ACK 状态重发。 */
    if (Ota_Crc32Bytes(payload, len) != expected_crc) {
        Ota_SendAck(OTA_FRAME_DATA, OTA_STATUS_BAD_PACKET_CRC, seq, g_ota.received_size);
        Ota_PrepareForNextFrame();
        return;
    }

    /* 上一次 ACK 可能丢失，上位机会重发同一帧；重复帧直接返回 OK，不重复写 Flash。 */
    if (Ota_DataFrameIsDuplicate(seq, offset, len) != 0U) {
        Ota_SendAck(OTA_FRAME_DATA, OTA_STATUS_OK, seq, g_ota.received_size);
        Ota_PrepareForNextFrame();
        return;
    }

    /* 数据帧必须按序到达，乱序时回传当前期望序号，方便上位机从该序号继续发送。 */
    if (seq != g_ota.expected_seq) {
        Ota_SendAck(OTA_FRAME_DATA, OTA_STATUS_BAD_SEQUENCE, g_ota.expected_seq, g_ota.received_size);
        Ota_PrepareForNextFrame();
        return;
    }

    /* 写入偏移必须紧跟已接收长度，并且不能超过起始帧声明的固件总大小。 */
    if ((offset != g_ota.received_size) || (next_offset > g_ota.image_size)) {
        Ota_SendAck(OTA_FRAME_DATA, OTA_STATUS_BAD_OFFSET, g_ota.expected_seq, g_ota.received_size);
        Ota_PrepareForNextFrame();
        return;
    }

    /* 校验全部通过后，将 payload 写入下载区；Flash 写失败需要终止本次 OTA。 */
    if (!Ota_WriteDownloadBytes(offset, payload, len)) {
        Ota_SendAck(OTA_FRAME_DATA, OTA_STATUS_FLASH_FAILED, seq, g_ota.received_size);
        Ota_FailAndReset(OTA_STATUS_FLASH_FAILED);
        return;
    }

    /* 更新接收进度和下一帧期望序号，然后释放当前帧缓冲状态。 */
    g_ota.received_size = next_offset;
    g_ota.expected_seq = (uint16_t)(g_ota.expected_seq + 1U);
    Ota_SetUiState(OTA_UPDATE_UI_RECEIVING, OTA_STATUS_OK);
    Ota_PrepareForNextFrame();

    /* 最后回 ACK，offset 字段带上当前累计接收字节数。 */
    Ota_SendAck(OTA_FRAME_DATA, OTA_STATUS_OK, seq, g_ota.received_size);
}

/**
 * @brief 处理 OTA 结束帧，校验完整固件并触发系统复位。
 */
static void Ota_HandleEndFrame(void)
{
    uint32_t image_size = Ota_ReadLe32(&g_ota.rx_buf[1]);
    uint32_t image_crc = Ota_ReadLe32(&g_ota.rx_buf[5]);

    if ((image_size != g_ota.image_size) || (image_crc != g_ota.image_crc) ||
        (g_ota.received_size != g_ota.image_size)) {
        Ota_SendAck(OTA_FRAME_END, OTA_STATUS_BAD_SIZE, 0U, g_ota.received_size);
        Ota_FailAndReset(OTA_STATUS_BAD_SIZE);
        return;
    }

    Ota_SetUiState(OTA_UPDATE_UI_VERIFYING, OTA_STATUS_OK);
    Ota_FlushFlashCache();

    if (Ota_Crc32Flash(BOOT_DOWNLOAD_ADDR, g_ota.image_size) != g_ota.image_crc) {
        Ota_SendAck(OTA_FRAME_END, OTA_STATUS_IMAGE_CRC_FAILED, 0U, g_ota.received_size);
        Ota_FailAndReset(OTA_STATUS_IMAGE_CRC_FAILED);
        return;
    }

    if (!Ota_DownloadVectorIsValid()) {
        Ota_SendAck(OTA_FRAME_END, OTA_STATUS_BAD_SIZE, 0U, g_ota.received_size);
        Ota_FailAndReset(OTA_STATUS_BAD_SIZE);
        return;
    }

    if (!Ota_WriteBootMeta()) {
        Ota_SendAck(OTA_FRAME_END, OTA_STATUS_FLASH_FAILED, 0U, g_ota.received_size);
        Ota_FailAndReset(OTA_STATUS_FLASH_FAILED);
        return;
    }

    Ota_SetUiState(OTA_UPDATE_UI_REBOOTING, OTA_STATUS_OK);
    Ota_SendAck(OTA_FRAME_END, OTA_STATUS_OK, 0U, g_ota.image_size);
    vTaskDelay(pdMS_TO_TICKS(500));
    NVIC_SystemReset();
}

/**
 * @brief 向 OTA 状态机喂入 1 个串口接收字节。
 * @param byte 当前收到的字节。
 * @return 1 表示该字节已被 OTA 模块消费，0 表示不是 OTA 数据。
 */
uint8_t Ota_UpdateFeedByte(uint8_t byte)
{
    uint16_t len;
    TickType_t now;

    /* 每收到一个字节都顺带检查接收超时，避免半包长期占住状态机。 */
    Ota_UpdatePoll();

    /* 空闲状态只识别起始帧头；尾部填充字节直接吞掉，其它字节交还给普通串口逻辑。 */
    if (g_ota.rx_state == OTA_RX_IDLE) {
        if (byte == OTA_FRAME_START) {
            Ota_BeginCollect(byte, OTA_START_LEN);      /* 开始收集固定 19 字节起始帧。 */
            return 1U;
        }
        if (byte == OTA_FRAME_TAIL_PAD) {
            return 1U;
        }
        return 0U;
    }

    /* 已经进入 OTA 传输后，等待下一帧帧头：数据帧、结束帧，也允许新的起始帧重新同步。 */
    if (g_ota.rx_state == OTA_RX_WAIT_FRAME) {
        if (byte == OTA_FRAME_DATA) {
            Ota_BeginCollect(byte, OTA_DATA_HEADER_LEN);
            return 1U;
        }

        if (byte == OTA_FRAME_END) {
            Ota_BeginCollect(byte, OTA_END_LEN);
            return 1U;
        }

        if (byte == OTA_FRAME_START) {
            Ota_BeginCollect(byte, OTA_START_LEN);
            return 1U;
        }

        if (byte == OTA_FRAME_TAIL_PAD) {
            return 1U;
        }

        /* OTA 过程中收到未知帧头，认为序列异常并把当前期望进度回给上位机。 */
        Ota_SendAck(byte, OTA_STATUS_BAD_SEQUENCE, g_ota.expected_seq, g_ota.received_size);
        return 1U;
    }

    now = xTaskGetTickCount();

    /* 防御性检查：如果索引已经越过接收缓冲区，立即复位 OTA 状态机。 */
    if (g_ota.rx_index >= sizeof(g_ota.rx_buf)) {
        Ota_UpdateReset();
        return 1U;
    }

    /* 普通收包路径：将字节追加到当前帧缓冲，并刷新最后接收时间。 */
    g_ota.rx_buf[g_ota.rx_index++] = byte;
    g_ota.last_rx_tick = now;

    /* 起始帧使用固定 magic "OTA1"，边收边检查可尽早排除普通串口数据误触发。 */
    if (g_ota.rx_state == OTA_RX_START) {
        if ((g_ota.rx_index == 2U) && (g_ota.rx_buf[1] != (uint8_t)'O')) {
            Ota_UpdateReset();
            return 0U;
        }
        if ((g_ota.rx_index == 3U) && (g_ota.rx_buf[2] != (uint8_t)'T')) {
            Ota_UpdateReset();
            return 1U;
        }
        if ((g_ota.rx_index == 4U) && (g_ota.rx_buf[3] != (uint8_t)'A')) {
            Ota_UpdateReset();
            return 1U;
        }
        if ((g_ota.rx_index == 5U) && (g_ota.rx_buf[4] != (uint8_t)'1')) {
            Ota_UpdateReset();
            return 1U;
        }
    }

    /* 数据帧头收满后才能知道 payload 长度，再据此计算整帧应接收的总长度。 */
    if ((g_ota.rx_state == OTA_RX_DATA) && (g_ota.rx_index == OTA_DATA_HEADER_LEN)) {
        len = Ota_ReadLe16(&g_ota.rx_buf[7]);
        if ((len == 0U) || (len > g_ota.payload_size) || (len > OTA_MAX_PAYLOAD_SIZE)) {
            Ota_SendAck(OTA_FRAME_DATA, OTA_STATUS_BAD_SIZE, Ota_ReadLe16(&g_ota.rx_buf[1]), g_ota.received_size);
            Ota_PrepareForNextFrame();
            return 1U;
        }

        g_ota.rx_expected = (uint16_t)(OTA_DATA_HEADER_LEN + len + 4U);
    }

    /* 当前帧还没收完整，继续消费后续字节。 */
    if (g_ota.rx_index < g_ota.rx_expected) {
        return 1U;
    }

    /* 当前帧已收完整，按状态分发到对应处理函数。 */
    if (g_ota.rx_state == OTA_RX_START) {
        Ota_HandleStartFrame();
    } else if (g_ota.rx_state == OTA_RX_DATA) {
        Ota_HandleDataFrame();
    } else if (g_ota.rx_state == OTA_RX_END) {
        Ota_HandleEndFrame();
    } else {
        Ota_UpdateReset();
    }

    return 1U;
}

uint16_t Ota_UpdateFeedBuffer(const uint8_t *data, uint16_t len)
{
    uint16_t consumed = 0U;

    if (data == 0) {
        return 0U;
    }

    while (consumed < len) {
        if (Ota_UpdateFeedByte(data[consumed]) == 0U) {
            break;
        }
        ++consumed;
    }

    return consumed;
}
