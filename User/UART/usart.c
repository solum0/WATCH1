#include "usart.h"
#include "ota_update.h"
// #include "oled.h" // 如果你不需要在串口文件里调用OLED，可以注释掉
#include <stdlib.h>

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
		
    // 6. 使能串口接收中断
    USART_ITConfig(DEBUG_USARTx, USART_IT_RXNE, ENABLE);

    // 7. 使能串口
    USART_Cmd(DEBUG_USARTx, ENABLE);
		
    // 8. 配置中断优先级
    NVIC_Configuration();
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

//// USART2 中断服务函数
//void DEBUG_USART_IRQHandler(void)
//{
//    uint8_t ucTemp;
//    if (USART_GetITStatus(DEBUG_USARTx, USART_IT_RXNE) != RESET) {
//        // 接收数据
//        ucTemp = USART_ReceiveData(DEBUG_USARTx);
//        // 原样回响发送回去
//        USART_SendData(DEBUG_USARTx, ucTemp);
//    }
//}

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
    /* 等待串口输入数据 */
    while (USART_GetFlagStatus(DEBUG_USARTx, USART_FLAG_RXNE) == RESET);

    return (int)USART_ReceiveData(DEBUG_USARTx);
}
