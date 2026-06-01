#ifndef __MOTOR_H
#define __MOTOR_H
 
#include "stm32f4xx.h" 
#include "FreeRTOS.h"
#include "task.h"



#define Motor_GPIO_PORT	 GPIOA			            /* GPIO端口 */
#define Motor_RCC 	     RCC_AHB1Periph_GPIOA		/* GPIO端口时钟 */
#define Motor_PIN		 GPIO_Pin_8	

void Motor_GPIO_Init(void);
void Motor_GPIO_ON(void);
void Motor_GPIO_off(void);



#endif

