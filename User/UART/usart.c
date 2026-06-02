#include "usart.h"
#include "ota_update.h"
// #include "oled.h" // 如果你不需要在串口文件里调用OLED，可以注释掉
#include <stdlib.h>

static uint8_t g_usart2_rx_dma_buf[DEBUG_USART_RX_DMA_BUF_SIZE];

// 如果不需要调用 FreeRTOS API，可以将优先级设置为高于 11（例如 5-10），以避免与 FreeRTOS 优先级冲突。
void NVIC_Configuration(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 嵌套向量中断控制器组选择 (FreeRTOS 推荐 Group 4) */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    /* 配置 USART2 为中断源 */
    NVIC_InitStructure.NVIC_IRQChannel = DEBUG_USART_IRQ;
    /* 抢占优先级为6 (根据你的FreeRTOS configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY来定) */
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
    /* 子优先级为0 (Group 4 下必须为 0) */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    /* 使能中断 */
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    /* 初始化配置NVIC */
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = DEBUG_USART_RX_DMA_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

static void USART_RxDmaConfig(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    DEBUG_USART_RX_DMA_CLK_CMD(DEBUG_USART_RX_DMA_CLK, ENABLE);

    DMA_Cmd(DEBUG_USART_RX_DMA_STREAM, DISABLE);
    while (DMA_GetCmdStatus(DEBUG_USART_RX_DMA_STREAM) != DISABLE) {
    }

    DMA_DeInit(DEBUG_USART_RX_DMA_STREAM);
    DMA_StructInit(&DMA_InitStructure);
    DMA_InitStructure.DMA_Channel = DEBUG_USART_RX_DMA_CHANNEL;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&DEBUG_USARTx->DR;
    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)g_usart2_rx_dma_buf;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
    DMA_InitStructure.DMA_BufferSize = DEBUG_USART_RX_DMA_BUF_SIZE;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_1QuarterFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DEBUG_USART_RX_DMA_STREAM, &DMA_InitStructure);

    DMA_ClearFlag(DEBUG_USART_RX_DMA_STREAM,
                  DMA_FLAG_TCIF5 | DMA_FLAG_HTIF5 | DMA_FLAG_TEIF5 |
                  DMA_FLAG_DMEIF5 | DMA_FLAG_FEIF5);
    DMA_ITConfig(DEBUG_USART_RX_DMA_STREAM, DMA_IT_HT, ENABLE);
    DMA_ITConfig(DEBUG_USART_RX_DMA_STREAM, DMA_IT_TC, ENABLE);
    DMA_ITConfig(DEBUG_USART_RX_DMA_STREAM, DMA_IT_TE, ENABLE);
    DMA_ITConfig(DEBUG_USART_RX_DMA_STREAM, DMA_IT_DME, ENABLE);
    DMA_Cmd(DEBUG_USART_RX_DMA_STREAM, ENABLE);
}

void USART_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // 1. 打开 GPIOA 时钟 (AHB1) 和 USART2 时钟 (APB1)
    DEBUG_USART_GPIO_AHBxClkCmd(DEBUG_USART_GPIO_CLK, ENABLE);
    DEBUG_USART_APBxClkCmd(DEBUG_USART_CLK, ENABLE);

    // 2. 配置 PA2 为复用推挽输出 (TX)
    GPIO_InitStructure.GPIO_Pin = DEBUG_USART_TX_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;    // 推挽输出
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;      // 上拉
    GPIO_Init(DEBUG_USART_TX_GPIO_PORT, &GPIO_InitStructure);

    // 3. 配置 PA3 为复用输入 (RX)
    GPIO_InitStructure.GPIO_Pin = DEBUG_USART_RX_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(DEBUG_USART_RX_GPIO_PORT, &GPIO_InitStructure);

    // 4. 引脚复用映射到 USART2
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);  // PA2 -> USART2_TX
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);  // PA3 -> USART2_RX

    // 5. 配置串口参数
    USART_InitStructure.USART_BaudRate = DEBUG_USART_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No ;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(DEBUG_USARTx, &USART_InitStructure);

    USART_RxDmaConfig();

    // 6. 使能串口空闲和错误中断，接收数据由 DMA 环形缓冲承接
    USART_ITConfig(DEBUG_USARTx, USART_IT_IDLE, ENABLE);
    USART_ITConfig(DEBUG_USARTx, USART_IT_ERR, ENABLE);
    USART_DMACmd(DEBUG_USARTx, USART_DMAReq_Rx, ENABLE);

    // 7. 使能串口
    USART_Cmd(DEBUG_USARTx, ENABLE);

    // 8. 配置中断优先级
    NVIC_Configuration();
}

uint16_t Usart_RxDmaGetWriteIndex(void)
{
    uint16_t remaining = DMA_GetCurrDataCounter(DEBUG_USART_RX_DMA_STREAM);
    uint16_t write_index = (uint16_t)(DEBUG_USART_RX_DMA_BUF_SIZE - remaining);

    if (write_index >= DEBUG_USART_RX_DMA_BUF_SIZE) {
        write_index = 0U;
    }

    return write_index;
}

uint16_t Usart_RxDmaRead(uint16_t *read_index, uint8_t *data, uint16_t max_len)
{
    uint16_t write_index;
    uint16_t available;
    uint16_t count = 0U;
    uint16_t index;

    if ((read_index == NULL) || (data == NULL) || (max_len == 0U)) {
        return 0U;
    }

    write_index = Usart_RxDmaGetWriteIndex();
    index = *read_index;
    if (index >= DEBUG_USART_RX_DMA_BUF_SIZE) {
        index = 0U;
    }

    if (write_index >= index) {
        available = (uint16_t)(write_index - index);
    } else {
        available = (uint16_t)(DEBUG_USART_RX_DMA_BUF_SIZE - index + write_index);
    }

    if (available > max_len) {
        available = max_len;
    }

    while (count < available) {
        data[count++] = g_usart2_rx_dma_buf[index++];
        if (index >= DEBUG_USART_RX_DMA_BUF_SIZE) {
            index = 0U;
        }
    }

    *read_index = index;
    return count;
}

void Usart_RxDmaRestart(void)
{
    USART_DMACmd(DEBUG_USARTx, USART_DMAReq_Rx, DISABLE);
    DMA_Cmd(DEBUG_USART_RX_DMA_STREAM, DISABLE);
    while (DMA_GetCmdStatus(DEBUG_USART_RX_DMA_STREAM) != DISABLE) {
    }

    DMA_SetCurrDataCounter(DEBUG_USART_RX_DMA_STREAM, DEBUG_USART_RX_DMA_BUF_SIZE);
    DMA_ClearFlag(DEBUG_USART_RX_DMA_STREAM,
                  DMA_FLAG_TCIF5 | DMA_FLAG_HTIF5 | DMA_FLAG_TEIF5 |
                  DMA_FLAG_DMEIF5 | DMA_FLAG_FEIF5);
    (void)DEBUG_USARTx->SR;
    (void)DEBUG_USARTx->DR;
    DMA_Cmd(DEBUG_USART_RX_DMA_STREAM, ENABLE);
    USART_DMACmd(DEBUG_USARTx, USART_DMAReq_Rx, ENABLE);
}

/***************** 发送一个字符 **********************/
void Usart_SendByte(USART_TypeDef * pUSARTx, uint8_t ch)
{
    /* 发送一个字节数据到USART */
    USART_SendData(pUSARTx, ch);

    /* 等待发送数据寄存器为空 */
    while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);
}

/***************** 发送字符串 **********************/
void Usart_SendString(USART_TypeDef * pUSARTx, char *str)
{
    unsigned int k = 0;
    do {
        Usart_SendByte(pUSARTx, *(str + k));
        k++;
    } while (*(str + k) != '\0');

    /* 等待发送完成 */
    while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TC) == RESET) {
    }
}

// 重定向 C 库函数 printf 到串口
int fputc(int ch, FILE *f)
{
    if (Ota_UpdateInProgress() != 0U) {
        return ch;
    }

    /* 发送一个字节数据到串口 */
    USART_SendData(DEBUG_USARTx, (uint16_t)ch);

    /* 等待发送完毕 */
    while (USART_GetFlagStatus(DEBUG_USARTx, USART_FLAG_TXE) == RESET);

    return (ch);
}

// 重定向 C 库函数 scanf 到串口
int fgetc(FILE *f)
{
    (void)f;
    return EOF;
}
