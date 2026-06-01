#ifndef __BME280_H__
#define __BME280_H__

#include "stm32f4xx.h" 
#include "i2c.h"    
#include "FreeRTOS.h"
#include "task.h"
#include "hi2c.h" // 假设这是你的I2C底层

// BME280 I2C 地址
#define BME280_ADDR         0x76 
#define BME280_ADDR_Read 		0xED
#define BME280_ADDR_Write 	0xEC

// 寄存器地址
#define BME280_REG_ID       0xD0
#define BME280_REG_RESET    0xE0
#define BME280_REG_CTRL_HUM 0xF2
#define BME280_REG_STATUS   0xF3
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG   0xF5
#define BME280_REG_PRESS_MSB 0xF7

// 常用配置宏 (直接修改这里即可改变传感器行为)
// 1. 湿度设置 (0xF2): x1过采样 = 0x01
#define HEX_CTRL_HUM        0x01  
// 2. 配置设置 (0xF5): 待机500ms(100), x16滤波(100), 无SPI(00) = 100 100 00 = 0x90
#define HEX_CONFIG          0x90  
// 3. 测量设置 (0xF4): 温度x4(011), 气压x4(011), 正常模式(11) = 011 011 11 = 0x6f
#define HEX_CTRL_MEAS       0x6F  

// 校准数据结构体 (必须保留用于计算)
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;
    int32_t  t_fine; // 全局中间变量
} BME280_CalibData;

// 函数声明
int BME280_Init(void);
int BME280_Read_All(float *temp, float *hum, float *press);
uint8_t BME_POWER_OFF(void);

typedef struct sensor_Struct{
uint32_t Temp1;		//整数
uint32_t Temp2;  //小数
uint32_t Hum1;
uint32_t Hum2;	
uint32_t Press;
}bme280_show;

#endif
