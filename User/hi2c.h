#ifndef __HI2C_H
#define __HI2C_H


#include "stm32f4xx.h" 


void hi2c_init(void);


uint8_t h_I2C_SendBytes(uint8_t Addr, uint8_t *pData, uint8_t Size);
uint8_t h_I2C_ReceiveBytes(uint8_t Addr, uint8_t *pBuffer, uint8_t Size);
uint8_t h_I2C_ReadReceives(uint8_t DevAddr, uint8_t ReadAddr, uint8_t *pBuffer, uint8_t Size);

uint8_t OLED_WriteData(uint8_t *Data, uint8_t Count);

void OLED_WriteCommand(uint8_t Command);





#endif










