#ifndef __TIMER_H
#define __TIMER_H
#include "stm32f4xx.h" 
 
 
 
extern volatile uint8_t start_flag;  // 开始标志
extern volatile uint8_t zero_flag;   // 清零复位

extern volatile uint8_t start_flag;
extern volatile uint8_t zero_flag;
extern volatile uint8_t m;
extern volatile uint8_t s;
extern volatile uint8_t ms;

extern void show_timer(void);
extern void timer_loop(void);
extern void enableClock(void);
extern void disableClock(void);

typedef struct TimerStruct{
	uint8_t msec;  //毫秒
	uint8_t second;
	uint8_t minute;
	uint8_t hour;
} SecTimer;
 
 
//参考来自：https://gitee.com/joshua_xu/stopwatch/blob/master/HARDWARE/TIMER/timer.c#
void disableClock(void);	//定时器停止
void enableClock(void);  	//定时器开启

void get_timer(SecTimer* ans);  //获取计时器
void timer_loop(void);     			//执行时间戳计算
void main_timer_control(void);  //带有控制的计时器

void TIM3_Int_Init(void); 
void TIM2_Int_Init(void);   //用于计算步数

void zero_timer(void);    //秒表清零

#endif
