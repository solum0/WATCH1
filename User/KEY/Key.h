#ifndef __KEY_H
#define __KEY_H

#include "stm32f4xx.h"                   // 引入STM32F10x设备头文件
#include "delay.h"     // 添加这行                      // 引入延时函数头文件
#include "FreeRTOS.h"
#include "task.h"

//WK-UP PA0, KEY1-PD10 		关机KEY3 - PA4		
//CTL - PB13(开机默认高电平，长按KEY3 关机), PB12 - BAT_ADC_EN( 默认低电平,key3长按使能高电平关机)


// 按键硬件配置定义


#define KEY1          GPIO_Pin_10     //PD10 

#define KEY3_switch   GPIO_Pin_4      //PA4

#define CTL						GPIO_Pin_13			//PB13

#define BAT_ADC_EN		GPIO_Pin_12			//PB12

// 函数声明
void Key_Init(void);                   // 按键初始化函数
uint8_t Key_GetNum(void);              // 轮询方式获取按键值函数
uint8_t GetKeyNum(void);               // 中断方式获取按键值函数

void power_switch_init(void);
void Enter_Standby_Mode(void);
#endif
