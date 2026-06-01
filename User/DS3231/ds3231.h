#ifndef DS3231_H
#define DS3231_H

#include "stm32f4xx.h" 
#include "FreeRTOS.h"
#include "task.h"

#include "hi2c.h" 

// 闹钟模式定义
#define ALARM_MODE_DAILY 0x00 // 每天触发（忽略日期/星期）
// 其他模式可根据需要定义

// 函数声明
uint8_t DS3231_SetAlarm1Daily(uint8_t sec, uint8_t min, uint8_t hour, uint8_t day);
uint8_t DS3231_SetAlarm2Daily(uint8_t min, uint8_t hour);
uint8_t DS3231_EnableAlarmInterrupt(uint8_t alarmNum, uint8_t enable);
uint8_t DS3231_ClearAlarmFlag(uint8_t alarmNum);
uint8_t DS3231_GetAlarmFlag(uint8_t alarmNum, uint8_t* flag);


// DS3231 I2C 设备地址定义
#define DS3231_ADDRESS          0x68    // I2C 从设备地址（7位地址）
#define DS3231_ADDRESS_Write    0xD0    // 写模式下的从设备地址（8位地址，最低位为0）
#define DS3231_ADDRESS_Read     0xD1    // 读模式下的从设备地址（8位地址，最低位为1）

/* DS3231 寄存器地址定义（参考数据手册第8.2节） */
#define DS3231_SEC_REG          0x00    // 秒寄存器
#define DS3231_MIN_REG          0x01    // 分寄存器
#define DS3231_HOUR_REG         0x02    // 小时寄存器
#define DS3231_WDAY_REG         0x03    // 星期寄存器
#define DS3231_MDAY_REG         0x04    // 日寄存器
#define DS3231_MONTH_REG        0x05    // 月寄存器
#define DS3231_YEAR_REG         0x06    // 年寄存器（2位，通常表示00-99年）

// 报警寄存器（两个报警设置）
#define DS3231_AL1SEC_REG       0x07    // 报警1秒寄存器
#define DS3231_AL1MIN_REG       0x08    // 报警1分寄存器
#define DS3231_AL1HOUR_REG      0x09    // 报警1小时寄存器
#define DS3231_AL1WDAY_REG      0x0A    // 报警1星期寄存器

#define DS3231_AL2MIN_REG       0x0B    // 报警2分寄存器
#define DS3231_AL2HOUR_REG      0x0C    // 报警2小时寄存器
#define DS3231_AL2WDAY_REG      0x0D    // 报警2星期寄存器

// 控制与状态寄存器
#define DS3231_CONTROL_REG      0x0E    // 控制寄存器
#define DS3231_STATUS_REG       0x0F    // 状态寄存器
#define DS3231_AGING_OFFSET_REG 0x10    // 老化偏移寄存器
#define DS3231_TMP_UP_REG       0x11    // 温度高字节寄存器
#define DS3231_TMP_LOW_REG      0x12    // 温度低字节寄存器

// 报警模式定义（用于设置报警触发方式）
#define EverySecond     0x01    // 每秒报警
#define EveryMinute     0x02    // 每分钟报警
#define EveryHour       0x03    // 每小时报警

// 日期时间结构体定义
typedef struct DateTImeStruct {
    uint8_t second;         // 秒（0-59）
    uint8_t minute;         // 分（0-59）
    uint8_t hour;           // 时（0-23）
    uint8_t dayofmonth;     // 日（1-31）
    uint8_t month;          // 月（1-12）
    uint16_t year;          // 年（如2023）
    uint8_t dayOfWeek;      // 星期（0=周日，1=周一，...，6=周六）
} DateTime;

// 函数声明
uint8_t DS3231_setDate(uint8_t year, uint8_t mon, uint8_t day,uint8_t dayOfWeek);         // 设置年月日
uint8_t DS3231_setTime(uint8_t hour, uint8_t min, uint8_t sec);         // 设置时分秒
uint8_t DS3231_getdate(DateTime* ans);                                  // 获取日期
uint8_t DS3231_gettime(DateTime* ans);                                  // 获取时间
void DS3231_SetDateTime(uint8_t *ad);                                   // 通过数组设置日期时间
void DS3231_Init(void);                                                 // 初始化函数（目前为空）
uint8_t BCD_DEC(uint8_t val);                                                // BCD码转十进制
uint8_t DEC_BCD(uint8_t val);                                                // 十进制转BCD码

//I2C操作
uint8_t IIC_DS3231_ByteRead(uint8_t ReadReg, uint8_t* Receive);
uint8_t IIC_DS3231_ByteWrite(uint8_t WriteReg, uint8_t data);

uint8_t parse_app_time(const char *s, uint8_t ad[6]);
uint8_t parse_app_alarm(const char *s, uint8_t len, uint8_t ad[2]);
uint8_t is_digit_char(char c);
#endif 
