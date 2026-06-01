#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <stdint.h>

/**
 * @brief 向 OTA 状态机喂入 1 个串口接收字节。
 * @param byte 当前收到的字节。
 * @return 1 表示该字节已被 OTA 模块消费，0 表示不是 OTA 数据。
 */
uint8_t Ota_UpdateFeedByte(uint8_t byte);

/**
 * @brief 轮询 OTA 接收超时状态。
 */
void Ota_UpdatePoll(void);

/**
 * @brief 查询 OTA 是否正在接收或处理升级数据。
 * @return 非 0 表示 OTA 正在进行，0 表示空闲。
 */
uint8_t Ota_UpdateInProgress(void);

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
