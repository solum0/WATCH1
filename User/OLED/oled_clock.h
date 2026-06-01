#ifndef __OLED_CLOCK_H
#define __OLED_CLOCK_H

#include "stm32f4xx.h"                   // STM32F10x设备头文件
#include  "ds3231.h"                    // DS3231 RTC驱动头文件
#include  "oled.h"                      // OLED驱动头文件
#include  "oled_font.h"                 // 字体数据头文件
#include "bme280.h"

#include "FreeRTOS.h"
#include "task.h"

#define uchar unsigned char             // 定义uchar为unsigned char简写

// 函数声明：显示时间组件（支持闪烁）
void display_year(uint16_t a,uchar flag);
void display_month(uchar a,uchar flag);
void display_day(uchar a,uchar flag);
void display_hour(uchar a,uchar flag);
void display_min(uchar a,uchar flag);
void display_sec(uchar a,uchar flag);
void display_week(uchar a,uchar flag);
void display(uint16_t a,uchar flag,uchar shift);

void show_time(DateTime* ans);       // 时间打印函数

// 图标绘制函数
void Draw_JLC_Logo(uint8_t x, uint8_t y);  // 打印嘉立创logo

void Draw_wdj(uint8_t x, uint8_t y, uint8_t width, uint8_t height); // 打印温度计图标
void Draw_ShuiDi(uint8_t x, uint8_t y, uint8_t width, uint8_t height); // 打印水滴图标
void Draw_alarm(uint8_t x, uint8_t y, uint8_t width, uint8_t height); //打印闹钟
void Draw_battery(uint8_t x, uint8_t y, uint8_t width, uint8_t height); //打印电池

//电池
void battery_show(uint16_t Bat_capacity);

//bme280
void show_bme280_time(bme280_show* bme);

//step
void show_step( uint16_t new_step_value);

//心率模式
void HR_SpO2_showm(uint8_t hr, uint8_t spo2,uint8_t state);
//	void HR_SpO2(void);
#endif
