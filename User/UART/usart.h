#ifndef USART_H_
#define USART_H_

#include "stdio.h"
#include "stm32f4xx.h" 

// 串口2 - USART2 (PA2, PA3)
#define  DEBUG_USARTx                   USART2
#define  DEBUG_USART_CLK                RCC_APB1Periph_USART2   // 注意：USART2挂载在APB1
#define  DEBUG_USART_APBxClkCmd         RCC_APB1PeriphClockCmd
#define  DEBUG_USART_BAUDRATE           115200

// USART GPIO 引脚宏定义
#define  DEBUG_USART_GPIO_CLK           RCC_AHB1Periph_GPIOA
#define  DEBUG_USART_GPIO_AHBxClkCmd    RCC_AHB1PeriphClockCmd  // 修改了宏名以匹配AHB1

#define  DEBUG_USART_TX_GPIO_PORT       GPIOA
#define  DEBUG_USART_TX_GPIO_PIN        GPIO_Pin_2		// PA2 -> TXD
#define  DEBUG_USART_RX_GPIO_PORT       GPIOA
#define  DEBUG_USART_RX_GPIO_PIN        GPIO_Pin_3		// PA3 -> RXD

#define  DEBUG_USART_IRQ                USART2_IRQn
#define  DEBUG_USART_IRQHandler         USART2_IRQHandler       // 中断服务函数重命名为 USART2

// 函数声明
void USART_Config(void);
void Usart_SendString(USART_TypeDef * pUSARTx, char *str);
void Usart_SendByte(USART_TypeDef * pUSARTx, uint8_t ch);

#endif /* USART_H_ */
