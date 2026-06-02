#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <stdint.h>

typedef enum {
    OTA_UPDATE_UI_IDLE = 0,
    OTA_UPDATE_UI_RECEIVING,
    OTA_UPDATE_UI_VERIFYING,
    OTA_UPDATE_UI_REBOOTING,
    OTA_UPDATE_UI_FAILED
} ota_update_ui_state_t;

typedef struct {
    ota_update_ui_state_t ui_state;
    uint32_t image_size;
    uint32_t received_size;
    uint32_t image_version;
    uint8_t percent;
    uint8_t error_status;
} ota_update_status_t;

typedef void (*ota_update_status_callback_t)(const ota_update_status_t *status);

#define OTA_UPDATE_STATUS_OK               ((uint8_t)0U)
#define OTA_UPDATE_STATUS_BAD_MAGIC        ((uint8_t)1U)
#define OTA_UPDATE_STATUS_BAD_SIZE         ((uint8_t)2U)
#define OTA_UPDATE_STATUS_BAD_SEQUENCE     ((uint8_t)3U)
#define OTA_UPDATE_STATUS_BAD_OFFSET       ((uint8_t)4U)
#define OTA_UPDATE_STATUS_BAD_PACKET_CRC   ((uint8_t)5U)
#define OTA_UPDATE_STATUS_FLASH_FAILED     ((uint8_t)6U)
#define OTA_UPDATE_STATUS_IMAGE_CRC_FAILED ((uint8_t)7U)
#define OTA_UPDATE_STATUS_VERSION_REJECTED ((uint8_t)8U)
#define OTA_UPDATE_STATUS_RX_TIMEOUT       ((uint8_t)9U)

/**
 * @brief 向 OTA 状态机喂入 1 个串口接收字节。
 * @param byte 当前收到的字节。
 * @return 1 表示该字节已被 OTA 模块消费，0 表示不是 OTA 数据。
 */
uint8_t Ota_UpdateFeedByte(uint8_t byte);

/**
 * @brief 向 OTA 状态机喂入一段串口接收缓冲。
 * @param data 接收缓冲起始地址。
 * @param len 缓冲长度。
 * @return 已被 OTA 模块消费的字节数；遇到非 OTA 数据时停止。
 */
uint16_t Ota_UpdateFeedBuffer(const uint8_t *data, uint16_t len);

/**
 * @brief 轮询 OTA 接收超时状态。
 */
void Ota_UpdatePoll(void);

/**
 * @brief 查询 OTA 是否正在接收或处理升级数据。
 * @return 非 0 表示 OTA 正在进行，0 表示空闲。
 */
uint8_t Ota_UpdateInProgress(void);
uint8_t Ota_UpdateDisplayActive(void);
void Ota_UpdateGetStatus(ota_update_status_t *status);
void Ota_UpdateRegisterStatusCallback(ota_update_status_callback_t callback);

/**
 * @brief 获取 OTA 期间 UART 接收丢字节计数。
 * @return UART 接收丢字节次数。
 */
uint32_t Ota_UpdateGetUartDropCount(void);

/**
 * @brief 获取启动后捕获到的 RCC 复位原因寄存器值。
 * @return RCC->CSR 的历史快照。
 */
uint32_t Ota_UpdateGetResetCsr(void);

/**
 * @brief 通知 OTA 模块发生了一次 UART 接收丢字节。
 */
void Ota_UpdateOnUartRxDrop(void);

/**
 * @brief 捕获并清除 MCU 复位原因标志。
 */
void Ota_UpdateCaptureResetCause(void);

/**
 * @brief 复位 OTA 接收上下文和传输状态。
 */
void Ota_UpdateReset(void);

#endif
