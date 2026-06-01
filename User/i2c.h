#ifndef __MYI2C_H
#define __MYI2C_H
#include "stm32f4xx.h"                   // Device header
#include "FreeRTOS.h"
#include "task.h"

#define IIC_SDA_Pin   GPIO_Pin_9      //棕线
#define IIC_SCL_Pin   GPIO_Pin_8      //红线  

void MyI2C_Init(void);
void MyI2C_Start(void);
void MyI2C_Stop(void);
void MyI2C_SendByte(uint8_t Byte);
uint8_t MyI2C_ReceiveByte(uint8_t Ack);
void MyI2C_SendAck(uint8_t AckBit);
uint8_t MyI2C_ReceiveAck(void);
int My_II2C_ReceiveBytes(uint8_t Addr, uint8_t *pBuffer, uint16_t Size);

#endif
