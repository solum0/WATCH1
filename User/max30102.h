#ifndef __MAX30102_H
#define __MAX30102_H
#include "stm32f4xx.h" 
#include "stdbool.h"

#include "string.h" 	
#include "delay.h"	
#include "FreeRTOS.h"
#include "task.h"
#include "key.h"

#include "hi2c.h"

#define I2C_WR	0		/* 写控制bit */
#define I2C_RD	1		/* 读控制bit */

#define true 1
#define false 0
#define FS 100
#define BUFFER_SIZE  (FS* 5) 
#define HR_FIFO_SIZE 7
#define MA4_SIZE  4 // DO NOT CHANGE
#define HAMMING_SIZE  5// DO NOT CHANGE
#define min(x,y) ((x) < (y) ? (x) : (y))



//#define max30102_WR_address 0xAE
#define I2C_MAX30102_ADDRESS 0x57
#define I2C_MAX30102_WRITE 0xAE		//写
#define I2C_MAX30102_READ 0xAF		//读

//register addresses
#define REG_INTR_STATUS_1 0x00
#define REG_INTR_STATUS_2 0x01
#define REG_INTR_ENABLE_1 0x02
#define REG_INTR_ENABLE_2 0x03
#define REG_FIFO_WR_PTR 0x04
#define REG_OVF_COUNTER 0x05
#define REG_FIFO_RD_PTR 0x06
#define REG_FIFO_DATA 0x07				//存储数据寄存器
#define REG_FIFO_CONFIG 0x08
#define REG_MODE_CONFIG 0x09
#define REG_SPO2_CONFIG 0x0A
#define REG_LED1_PA 0x0C
#define REG_LED2_PA 0x0D
#define REG_PILOT_PA 0x10
#define REG_MULTI_LED_CTRL1 0x11
#define REG_MULTI_LED_CTRL2 0x12
#define REG_TEMP_INTR 0x1F
#define REG_TEMP_FRAC 0x20
#define REG_TEMP_CONFIG 0x21
#define REG_PROX_INT_THRESH 0x30
#define REG_REV_ID 0xFE
#define REG_PART_ID 0xFF

#define MAX_BRIGHTNESS 255	//最大亮度

#define MAX30102_POWER_OFF    0X87  //关机
#define MAX30102_POWER_RESET 	0x47	//复位

extern volatile uint8_t max30102_int_triggered; 

//IIC所有操作函数		 

uint8_t MAX30102_IIC_WriteByte(uint8_t WriteReg,uint8_t data);
uint8_t MAX30102_IIC_ReadByte(uint8_t ReadReg,uint8_t* Receive);

uint8_t MAX30102_IIC_ReadBytes( uint8_t ReadReg,uint8_t* Receive,uint8_t dataLength);

//MAX30102所有操作函数
void MAX30102_Init(void);  
void MAX30102_Reset(void);

uint8_t max30102_FIFO_ReadBytes(uint8_t Register_Address,uint8_t* Data);

void maxim_max30102_read_fifo(uint32_t *pun_red_led, uint32_t *pun_ir_led);

int check_signal_quality(uint32_t *buffer, int length, uint32_t min, uint32_t max);
void moving_average_filter(uint32_t *input, uint32_t *output, int length, int window_size);

void MAX30102_off(void); //关机

//心率血氧算法所有函数
void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer ,int32_t n_ir_buffer_length, uint32_t *pun_red_buffer ,   int32_t *pn_spo2, int8_t *pch_spo2_valid ,int32_t *pn_heart_rate , int8_t  *pch_hr_valid);
void maxim_find_peaks( int32_t *pn_locs, int32_t *pn_npks,  int32_t *pn_x, int32_t n_size, int32_t n_min_height, int32_t n_min_distance, int32_t n_max_num );
void maxim_peaks_above_min_height( int32_t *pn_locs, int32_t *pn_npks,  int32_t *pn_x, int32_t n_size, int32_t n_min_height );
void maxim_remove_close_peaks( int32_t *pn_locs, int32_t *pn_npks,   int32_t  *pn_x, int32_t n_min_distance );
void maxim_sort_ascend( int32_t *pn_x, int32_t n_size );
void maxim_sort_indices_descend(  int32_t  *pn_x, int32_t *pn_indx, int32_t n_size);

uint8_t INT_max30102(uint8_t auto_clear);
#endif
