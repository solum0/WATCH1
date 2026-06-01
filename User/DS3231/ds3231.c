#include "ds3231.h"
#include "i2c.h"    // 自定义 I2C 驱动
#include "usart.h"  // 串口驱动，用于调试输出

// DS3231 初始化函数（目前为空，仅保留初始化框架）
void DS3231_Init(void) {
    // MyI2C_Init(); // 如果需要初始化 I2C，可取消注释
}

/**
 * @brief 将 BCD 码转换为十进制数
 * @param byte：BCD 码值
 * @return 转换后的十进制数
 */
static uint8_t BCD_DEC(uint8_t byte) {
    uint8_t temp_H, temp_L;
    temp_L = byte & 0x0f;           // 取低4位
    temp_H = (byte & 0xf0) >> 4;    // 取高4位并右移
    return (temp_H * 10) + temp_L;  // 组合为十进制数
}

/**
 * @brief 将十进制数转换为 BCD 码
 * @param val：十进制数值
 * @return 转换后的 BCD 码
 */
uint8_t DEC_BCD(uint8_t val) {
    uint8_t i, j, k;
    i = val / 10;       // 十位数
    j = val % 10;       // 个位数
    k = j + (i << 4);   // 组合为 BCD 码
    return k;
}

/**
 * @brief 向 DS3231 指定寄存器写入一个字节
 * @param WriteAddr：寄存器地址
 * @param date：要写入的数据
 * @return 0：成功；非0：错误码（1-3表示不同阶段的错误）
 */
uint8_t IIC_DS3231_ByteWrite(uint8_t WriteReg, uint8_t data) {
   uint8_t bytesToSend[] = {WriteReg, data};
	 return h_I2C_SendBytes(DS3231_ADDRESS_Write, bytesToSend, 2);
   
}

/**
 * @brief 从 DS3231 指定寄存器读取一个字节
 * @param ReadAddr：寄存器地址
 * @param Receive：用于存储读取结果的指针
 * @return 0：成功；非0：错误码
 */
uint8_t IIC_DS3231_ByteRead(uint8_t ReadReg, uint8_t* Receive) {
   return	h_I2C_ReadReceives(DS3231_ADDRESS, ReadReg, Receive, 1);
    
}

/**
 * @brief 设置年月日
 * @param year：年（如23表示2023）
 * @param mon：月（1-12）
 * @param day：日（1-31）
 * @return 0：成功；非0：错误码
 */
uint8_t DS3231_setDate(uint8_t year, uint8_t mon, uint8_t day,uint8_t dayOfWeek) {
    uint8_t temp_H, temp_L;
		
		if(mon > 12) mon = 1 ;
		if(dayOfWeek > 7 || dayOfWeek < 1) dayOfWeek = 1;
		if(day < 1 ) day =1;
	
    // 转换年并写入
    temp_L = year % 10;
    temp_H = year / 10;
    year = (temp_H << 4) + temp_L;
    if (IIC_DS3231_ByteWrite(DS3231_YEAR_REG, year)) {
        printf("set year error\r\n");
        return 1;
    }
    // 转换月并写入
		 
    temp_L = mon % 10;
    temp_H = mon / 10;
    mon = (temp_H << 4) + temp_L;
    if (IIC_DS3231_ByteWrite(DS3231_MONTH_REG, mon)) {
        printf("set month error\r\n");
        return 2;
    }
    // 转换日并写入
		
    temp_L = day % 10;
    temp_H = day / 10;
    day = (temp_H << 4) + temp_L;
    if (IIC_DS3231_ByteWrite(DS3231_MDAY_REG, day)) {
        printf("set day error\r\n");
        return 3;
    }
		// 转换周并写入
		temp_L = dayOfWeek % 10;
    temp_H = dayOfWeek / 10;
    dayOfWeek = (temp_H << 4) + temp_L;
    if (IIC_DS3231_ByteWrite(DS3231_WDAY_REG, dayOfWeek)) {
        printf("set dayOfWeek error\r\n");
        return 4;
    }
		
    return 0;
}

/**
 * @brief 设置时分秒
 * @param hour：时（0-23）
 * @param min：分（0-59）
 * @param sec：秒（0-59）
 * @return 0：成功；非0：错误码
 */
uint8_t DS3231_setTime(uint8_t hour, uint8_t min, uint8_t sec) {
//    printf("1.hour=%d, min=%d, sec=%d\r\n", hour, min, sec);

    // 输入范围检查与修正
    if (hour > 23) hour = 0;
    if (min > 59) min = 1;
    if (sec > 59) sec = 1;

    uint8_t temp_H, temp_L;
    // 转换小时并写入
    temp_L = hour % 10;
    temp_H = hour / 10;
 //   printf("2.temp_L: %d, temp_H: %d\r\n", temp_L, temp_H);
    hour = (temp_H << 4) + temp_L;
    if (IIC_DS3231_ByteWrite(DS3231_HOUR_REG, hour)) return 1;

    // 转换分钟并写入
    temp_L = min % 10;
    temp_H = min / 10;
    min = (temp_H << 4) + temp_L;
    if (IIC_DS3231_ByteWrite(DS3231_MIN_REG, min)) return 2;

    // 转换秒并写入
    temp_L = sec % 10;
    temp_H = sec / 10;
    sec = (temp_H << 4) + temp_L;
    if (IIC_DS3231_ByteWrite(DS3231_SEC_REG, sec)) return 3;

    return 0;
}

/**
 * @brief 通过数组设置日期时间（数组顺序：年、月、日、时、分、秒）
 * @param ad：指向包含6个元素的数组的指针
 */
void DS3231_SetDateTime(uint8_t *ad) {
    uint8_t temp_H, temp_L;
    // 设置年
    temp_L = ad[0] % 10;
    temp_H = ad[0] / 10;
    ad[0] = (temp_H << 4) + temp_L;
    if (IIC_DS3231_ByteWrite(DS3231_YEAR_REG, ad[0])) {
        printf("set year error\r\n");
        return;
    }
    // 设置月
    temp_L = ad[1] % 10;
    temp_H = ad[1] / 10;
    ad[1] = (temp_H << 4) + temp_L;
    if (IIC_DS3231_ByteWrite(DS3231_MONTH_REG, ad[1])) {
        printf("set month error\r\n");
        return;
    }
    // 设置日
    temp_L = ad[2] % 10;
    temp_H = ad[2] / 10;
    ad[2] = (temp_H << 4) + temp_L;
    if (IIC_DS3231_ByteWrite(DS3231_MDAY_REG, ad[2])) {
        printf("set day error\r\n");
        return;
    }
    // 设置时
    temp_L = ad[3] % 10;
    temp_H = ad[3] / 10;
    ad[3] = (temp_H << 4) + temp_L;
    if (IIC_DS3231_ByteWrite(DS3231_HOUR_REG, ad[3])) return;
    // 设置分
    temp_L = ad[4] % 10;
    temp_H = ad[4] / 10;
    ad[4] = (temp_H << 4) + temp_L;
    if (IIC_DS3231_ByteWrite(DS3231_MIN_REG, ad[4])) return;
    // 设置秒
    temp_L = ad[5] % 10;
    temp_H = ad[5] / 10;
    ad[5] = (temp_H << 4) + temp_L;
    if (IIC_DS3231_ByteWrite(DS3231_SEC_REG, ad[5])) return;
}

/**
 * @brief 获取当前时间（时分秒）
 * @param ans：指向 DateTime 结构体的指针，用于存储结果
 * @return 0：成功；非0：错误码
 */
uint8_t DS3231_gettime(DateTime* ans) {
    uint8_t receive = 0;
    if (IIC_DS3231_ByteRead(DS3231_HOUR_REG, &receive)) return 1;
    ans->hour = BCD_DEC(receive);
    if (IIC_DS3231_ByteRead(DS3231_MIN_REG, &receive)) return 2;
    ans->minute = BCD_DEC(receive);
    if (IIC_DS3231_ByteRead(DS3231_SEC_REG, &receive)) return 3;
    ans->second = BCD_DEC(receive);
    return 0;
}

/**
 * @brief 获取当前日期（年月日星期）
 * @param ans：指向 DateTime 结构体的指针，用于存储结果
 * @return 0：成功；非0：错误码
 */
uint8_t DS3231_getdate(DateTime* ans) {
    uint8_t receive = 0;
    if (IIC_DS3231_ByteRead(DS3231_YEAR_REG, &receive)) return 1;
		ans->year = BCD_DEC(receive) + 2000; // 基准年份为2000
    if (IIC_DS3231_ByteRead(DS3231_MONTH_REG, &receive)) return 2;
    ans->month = BCD_DEC(receive);
    if (IIC_DS3231_ByteRead(DS3231_MDAY_REG, &receive)) return 3;
    ans->dayofmonth = BCD_DEC(receive);
    if (IIC_DS3231_ByteRead(DS3231_WDAY_REG, &receive)) return 4;
    ans->dayOfWeek = BCD_DEC(receive);
    return 0;
}

/**
 * @brief 设置 Alarm 1 每天报警
 * @param sec: 秒 (0-59)
 * @param min: 分 (0-59)
 * @param hour: 时 (0-23)
 * @return 0成功，否则错误码
 */
uint8_t DS3231_SetAlarm1Daily(uint8_t sec, uint8_t min, uint8_t hour, uint8_t day) {
    uint8_t temp;
    
    // 设置秒寄存器: 0x07
    temp = DEC_BCD(sec);
    temp &= 0x7F; // 清除A1M1位（不忽略秒）
    if (IIC_DS3231_ByteWrite(DS3231_AL1SEC_REG, temp)) return 1;
    
    // 设置分寄存器: 0x08
    temp = DEC_BCD(min);
    temp &= 0x7F; // 清除A1M2位（不忽略分）
    if (IIC_DS3231_ByteWrite(DS3231_AL1MIN_REG, temp)) return 2;
    
    // 设置小时寄存器: 0x09
    temp = DEC_BCD(hour);
    temp &= 0x7F; // 清除A1M3位（不忽略时）
    if (IIC_DS3231_ByteWrite(DS3231_AL1HOUR_REG, temp)) return 3;
    
    // 设置日/星期寄存器: 0x0A
    temp = DEC_BCD(day); 
	/*  */
    temp |= 0x80;     // 设置A1M4位（忽略日期/星期，即每日指定时分秒触发闹钟）
	//	temp &= 0x7F;				//到特定的日期alarm
    //  temp &= 0xBF;     // 设置DY/DT为0（使用日期）
    if (IIC_DS3231_ByteWrite(DS3231_AL1WDAY_REG, temp)) return 4;
    
    return 0;
}

/**
 * @brief 设置 Alarm 2 每天报警（无秒）
 * @param min: 分 (0-59)
 * @param hour: 时 (0-23)
 * @return 0成功，否则错误码
 */
uint8_t DS3231_SetAlarm2Daily(uint8_t min, uint8_t hour) {
    uint8_t temp;
    
    // 设置分寄存器: 0x0B
    temp = DEC_BCD(min);
    temp &= 0x7F; // 清除A2M2位（不忽略分）
    if (IIC_DS3231_ByteWrite(DS3231_AL1MIN_REG, temp)) return 1;
    
    // 设置小时寄存器: 0x0C
    temp = DEC_BCD(hour);
    temp &= 0x7F; // 清除A2M3位（不忽略时）
    if (IIC_DS3231_ByteWrite(DS3231_AL1HOUR_REG, temp)) return 2;
    
    // 设置日/星期寄存器: 0x0D
    temp = DEC_BCD(1); // 日期值任意
    temp |= 0x80;     // 设置A2M4位（忽略日期/星期）
    //temp &= 0xBF;     // 设置DY/DT为0（使用日期）
    if (IIC_DS3231_ByteWrite(DS3231_AL1WDAY_REG, temp)) return 3;
    
    return 0;
}

/**
 * @brief 启用或禁用闹钟中断
 * @param alarmNum: 1 for Alarm 1, 2 for Alarm 2
 * @param enable: 1 enable, 0 disable
 * @return 0成功，否则错误码
 */
uint8_t DS3231_EnableAlarmInterrupt(uint8_t alarmNum, uint8_t enable) {
    uint8_t controlReg;
    if (IIC_DS3231_ByteRead(DS3231_CONTROL_REG, &controlReg)) return 1;
    
    if (alarmNum == 1) {
        if (enable) {
            controlReg |= 0x01; // Set A1IE
        } else {
            controlReg &= ~0x01; // Clear A1IE
        }
    } else if (alarmNum == 2) {
        if (enable) {
            controlReg |= 0x02; // Set A2IE
        } else {
            controlReg &= ~0x02; // Clear A2IE
        }
    } else {
        return 2; // Invalid alarm number
    }
    
    // 设置INTCN位为1，以便中断输出
    controlReg |= 0x04;
    if (IIC_DS3231_ByteWrite(DS3231_CONTROL_REG, controlReg)) return 3;
    
    return 0;
}

/**
 * @brief 清除闹钟标志
 * @param alarmNum: 1 for Alarm 1, 2 for Alarm 2
 * @return 0成功，否则错误码
 */
uint8_t DS3231_ClearAlarmFlag(uint8_t alarmNum) {
    uint8_t statusReg;
    if (IIC_DS3231_ByteRead(DS3231_STATUS_REG, &statusReg)) return 1;
    
    if (alarmNum == 1) {
        statusReg &= ~0x01; // Clear A1F
    } else if (alarmNum == 2) {
        statusReg &= ~0x02; // Clear A2F
    } else {
        return 2;
    }
    
    if (IIC_DS3231_ByteWrite(DS3231_STATUS_REG, statusReg)) return 3;
    
    return 0;
}

/**
 * @brief 获取闹钟标志
 * @param alarmNum: 1 for Alarm 1, 2 for Alarm 2
 * @param flag: pointer to store flag value
 * @return 0成功，否则错误码
 */
uint8_t DS3231_GetAlarmFlag(uint8_t alarmNum, uint8_t* flag) {
    uint8_t statusReg;
    if (IIC_DS3231_ByteRead(DS3231_STATUS_REG, &statusReg)) return 1;
    
    if (alarmNum == 1) {
        *flag = statusReg & 0x01;
    } else if (alarmNum == 2) {
        *flag = statusReg & 0x02;
    } else {
        return 2;
    }
    
    return 0;
}

uint8_t is_digit_char(char c)
{
    return (c >= '0' && c <= '9');
}

uint8_t parse_app_time(const char *s, uint8_t ad[6])
{
    uint16_t year;
    uint8_t month, day, hour, minute, second;

    // 固定格式: MM/dd/yyyy HH:mm:ss，总长度 19
    if (!is_digit_char(s[0])  || !is_digit_char(s[1])  || s[2]  != '/' ||

		!is_digit_char(s[3])  || !is_digit_char(s[4])  || s[5]  != '/' ||
        !is_digit_char(s[6])  || !is_digit_char(s[7])  ||
        !is_digit_char(s[8])  || !is_digit_char(s[9])  || s[10] != ' ' ||
        !is_digit_char(s[11]) || !is_digit_char(s[12]) || s[13] != ':' ||
        !is_digit_char(s[14]) || !is_digit_char(s[15]) || s[16] != ':' ||
        !is_digit_char(s[17]) || !is_digit_char(s[18])) {
        return 0;
    }

    month  = (s[0]  - '0') * 10 + (s[1]  - '0');
    day    = (s[3]  - '0') * 10 + (s[4]  - '0');
    year   = (s[6]  - '0') * 1000 + (s[7] - '0') * 100 +
             (s[8]  - '0') * 10   + (s[9] - '0');
    hour   = (s[11] - '0') * 10 + (s[12] - '0');
    minute = (s[14] - '0') * 10 + (s[15] - '0');
    second = (s[17] - '0') * 10 + (s[18] - '0');

    if (month < 1 || month > 12) return 0;
    if (day < 1 || day > 31) return 0;
    if (hour > 23 || minute > 59 || second > 59) return 0;
    if (year < 2000 || year > 2099) return 0;

    ad[0] = year % 100;
    ad[1] = month;
    ad[2] = day;
    ad[3] = hour;
    ad[4] = minute;
    ad[5] = second;

    return 1;
}

uint8_t parse_app_alarm(const char *s, uint8_t len, uint8_t ad[2])
{
    uint8_t hour, minute;

    if (len == 3) {
        if (s[1] != ':') {
            return 0;
        }

        // Support both raw byte mode (0x06) and ASCII mode ('6').
        if (s[0] <= 23 && s[2] <= 59) {
            hour = (uint8_t)s[0];
            minute = (uint8_t)s[2];
        } else if (is_digit_char(s[0]) && is_digit_char(s[2])) {
            hour = (uint8_t)(s[0] - '0');
            minute = (uint8_t)(s[2] - '0');
        } else {
            return 0;
        }
    } else if (len == 5) {
        if (!is_digit_char(s[0]) || !is_digit_char(s[1]) ||
            s[2] != ':' ||
            !is_digit_char(s[3]) || !is_digit_char(s[4])) {
            return 0;
        }
        hour = (uint8_t)((s[0] - '0') * 10 + (s[1] - '0'));
        minute = (uint8_t)((s[3] - '0') * 10 + (s[4] - '0'));
    } else {
        return 0;
    }

    if (hour > 23 || minute > 59) {
        return 0;
    }

    ad[0] = hour;
    ad[1] = minute;
    return 1;
}
