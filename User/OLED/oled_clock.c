# include  "oled_clock.h"
# include  "oled_font.h"

// 显示年份（12x12字体），支持闪烁效果
// 参数: a - 年份（如2023）, flag - 1显示，0清除
void display_year(uint16_t a, uchar flag)
{
    if (flag)
    {
        OLED_ShowNum(6, 55, a, 4, OLED_6X8);  // 在指定位置显示4位年份，模式1（显示前导0）
				OLED_Update();
    }
    else
    {
//        // 清除年份显示区域（4个字符位置，每个占6列）
        OLED_ClearArea(6, 55,6,8);
        OLED_ClearArea(12, 55,6,8);
        OLED_ClearArea(18, 55,6,8);
        OLED_ClearArea(24, 55,6,8);
    }
}

// 显示月份（12x12字体），支持闪烁
// 参数: a - 月份（1-12）, flag - 1显示，0清除
void display_month(uchar a, uchar flag)
{
    if (flag)
    {
        OLED_ShowNum(40, 55, a / 10 ,1,OLED_6X8);  // 显示十位数
        OLED_ShowNum(46, 55, a % 10 ,1,OLED_6X8);  // 显示个位数
				OLED_Update();
    }
    else
    {
//        // 清除月份显示区域（2个字符）
        OLED_ClearArea(40, 55,6,8);
        OLED_ClearArea(46, 55,6,8);
    }
}

// 显示日期（12x12字体），支持闪烁
// 参数: a - 日期（1-31）, flag - 1显示，0清除
void display_day(uchar a, uchar flag)
{
    if (flag)
    {
				OLED_ShowNum(62, 55, a / 10 ,1,OLED_6X8);  // 显示十位数
				OLED_ShowNum(68, 55, a % 10 ,1,OLED_6X8);  // 显示个位数
				OLED_Update();
    }
    else
    {
        // 清除日期显示区域（2个字符）
      OLED_ClearArea(62, 55,6,8);
      OLED_ClearArea(68, 55,6,8);
    }
}

// 显示小时（24x24字体），支持闪烁
// 参数: a - 小时（0-23）, flag - 1显示，0清除
void display_hour(uchar a, uchar flag)
{
    if (flag)
    {
        OLED_ShowNum(16, 20, a / 10 ,1,OLED_12x24);  // 显示十位数
        OLED_ShowNum(28, 20, a % 10 ,1,OLED_12x24);  // 显示个位数
				OLED_Update();
    }
    else
    {
        // 清除小时显示区域（2个字符，占24x24区域）
        OLED_ClearArea(16, 20,12,24);
				OLED_ClearArea(28, 20,12,24);
    }
}

// 显示分钟（24x24字体），支持闪烁
// 参数: a - 分钟（0-59）, flag - 1显示，0清除
void display_min(uchar a, uchar flag)
{
    if (flag)
    {
				OLED_ShowNum(52, 20, a / 10 ,1,OLED_12x24);  // 显示十位数
        OLED_ShowNum(64, 20, a % 10,1,OLED_12x24);  // 显示个位数
				OLED_Update();
    }
    else
    {
        // 清除分钟显示区域
				OLED_ClearArea(52, 20,12,24);
				OLED_ClearArea(64, 20,12,24);
    }
}

// 显示秒钟（24x24字体），支持闪烁
// 参数: a - 秒钟（0-59）, flag - 1显示，0清除
void display_sec(uchar a, uchar flag)
{
    if (flag)
    {
        OLED_ShowNum(88, 20, a / 10 ,1,OLED_12x24);  // 显示十位数
        OLED_ShowNum(100, 20, a % 10 ,1,OLED_12x24); // 显示个位数
				OLED_Update();
    }
    else
    {
        // 清除秒钟显示区域
       OLED_ClearArea(88, 20,12,24);
			 OLED_ClearArea(100, 20,12,24);
    }
}

void display_week(uchar a,uchar flag)
{
	 if (flag)
    {
        
					OLED_ShowNum(85, 55, a ,1,OLED_6X8);
					OLED_Update();
    }
    else
    {
        // 清除周显示区域
         OLED_ClearArea(85, 55,6,8);
    }


}	


// 通用显示函数，根据shift选择显示不同时间组件
// 参数: a - 时间值, flag - 1显示/0清除, shift - 组件类型（0-秒,1-分,2-时,3-日,4-月,5-年）
void display(uint16_t a, uchar flag, uchar shift)
{
    switch (shift)
    {
        case 0:
            display_sec(a, flag);  // 显示秒
            break;
        case 1:
            display_min(a, flag);  // 显示分
            break;
        case 2:
            display_hour(a, flag); // 显示时
            break;
        case 3:
            display_day(a, flag);  // 显示日
            break;
        case 4:
            display_month(a, flag); // 显示月
            break;
        case 5:
            display_year(a+2000, flag); // 显示年（假设a为年份偏移，+2000）
            break;
				case 6:
						display_week(a,flag); 	//显示周
        default:
            break;
    }
}

//OLED_6X8放日期

// 显示完整时间（包括年月日时分秒和星期）
// 参数: ans - 指向DateTime结构体的指针，包含时间信息
void show_time(DateTime* ans)
{

	
		OLED_ShowNum(16, 20, ans->hour, 2, OLED_12x24); // 显示小时（24x24）
		OLED_ShowChar(40, 20, ':',  OLED_12x24);				// 显示冒号分隔符
		OLED_ShowNum(52, 20, ans->minute, 2, OLED_12x24);
		OLED_ShowChar(76, 20, ':',  OLED_12x24);  
		OLED_ShowNum(88, 20, ans->second, 2, OLED_12x24);

		
		OLED_ShowNum(6, 55, ans->year, 4,  OLED_6X8);     // 显示年份（12x12）
    OLED_ShowChar(32, 55, '-', OLED_6X8);               // 显示分隔符
    OLED_ShowNum(40, 55, ans->month, 2, OLED_6X8);   // 显示月份
    OLED_ShowChar(54, 55, '-',OLED_6X8);               // 显示分隔符
    OLED_ShowNum(62, 55, ans->dayofmonth, 2, OLED_6X8);  // 显示日期
    OLED_ShowNum(85, 55, ans->dayOfWeek, 1, OLED_6X8);   // 显示星期（1位数字）
	//	 OLED_UpdateArea(88, 20, 24, 24); 
		OLED_Update();
}


void show_bme280_time(bme280_show* bme)
{
	//温度
	OLED_ShowNum(66, 30,bme->Temp1, 2, OLED_6X8);
	OLED_ShowChar(78, 30, '.', OLED_6X8);
	OLED_ShowNum(83, 30,bme->Temp2, 2, OLED_6X8);
	
	OLED_ShowImage(95,28,12,12,temperature);// 12*12  温度符号
	OLED_ShowImage(20,12,38,48,sd_t); //温度计图像
	
	//湿度
	OLED_ShowNum(66, 15,bme->Hum1, 2,OLED_6X8);
	OLED_ShowChar(78, 15, '.', OLED_6X8);
	OLED_ShowNum(83, 15,bme->Hum2, 2,OLED_6X8);
	OLED_ShowChar(95, 15, '%', OLED_6X8);
	OLED_Update();

}	

void battery_show(uint16_t Bat_capacity)
{
 	OLED_ShowNum(84, 4, Bat_capacity, 3,  OLED_6X8);
	OLED_ShowChar(102, 4, '%',  OLED_6X8);
	
	if(Bat_capacity==100)OLED_ShowImage(110,0,16,16,battery);
	else if(Bat_capacity>=10&&Bat_capacity<100)
	{
		OLED_ShowImage(110,0,16,16,battery);
		OLED_ClearArea((112+Bat_capacity/10),5,(10-Bat_capacity/10),6);
		OLED_ClearArea(85,4,6,8);
		
	}
	else //清零
	{
		OLED_ShowImage(110,0,16,16,battery);
		OLED_ClearArea(112,5,10,6);
		OLED_ClearArea(85,4,12,8);
		
	}
	//OLED_Update();

//	OLED_ShowChar12(101, 0, '%');
//	Clear_Bitmap_RowMajor(118,5,8,6);

}

void show_step( uint16_t new_step_value)
{	
		OLED_ShowString(0,0,"step:",OLED_6X8);
		if(new_step_value >0 && new_step_value<9)								OLED_ShowNum(32, 0, new_step_value, 1,OLED_6X8);
		else if(new_step_value >10 && new_step_value<100)  			OLED_ShowNum(32, 0, new_step_value, 2,OLED_6X8);
		else if(new_step_value >100 && new_step_value<1000)			OLED_ShowNum(32, 0, new_step_value, 3,OLED_6X8);
		else if(new_step_value >1000 && new_step_value<10000)		OLED_ShowNum(32, 0, new_step_value, 4,OLED_6X8);
		else if(new_step_value >10000 && new_step_value<100000)	OLED_ShowNum(32, 0, new_step_value, 5,OLED_6X8);
	//	OLED_Update();
}

//
//	void HR_SpO2(void)
void HR_SpO2_showm(uint8_t hr, uint8_t spo2,uint8_t state)
{
	if(state){
	OLED_ShowString(20, 20, "心率:", OLED_8X16);
	OLED_ShowString(80,20,"BPM",OLED_8X16);
	OLED_ShowNum(60,20,hr,2,OLED_8X16);
	
	OLED_ShowString(20, 40, "SpO2:", OLED_8X16);
	OLED_ShowChar(85,40,'%', OLED_8X16);
	OLED_ShowNum(60,40,spo2,2,OLED_8X16);
	}
	else{
	
	OLED_ShowString(20, 20, "心率:", OLED_8X16);
	OLED_ShowString(80,20,"BPM",OLED_8X16);
	OLED_ShowString(60,20,"__",OLED_8X16);
	
	OLED_ShowString(20, 40, "SpO2:", OLED_8X16);
	OLED_ShowChar(85,40,'%', OLED_8X16);
	OLED_ShowString(60,40,"__",OLED_8X16);
	
	}
OLED_Update();
}

//// 打印水滴图标
//// 参数: x,y - 坐标, width,height - 图标尺寸
//void Draw_ShuiDi(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
//{
//    Draw_Icon(x, y, sd_t, width, height);  // sd_t为水滴图标数据
//}

////电池图标 16*16
//void  Draw_battery(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
//{
//	OLED_ShowImage(95,28,12,12,temperature);// 12*12  温度符号
//	Draw_Icon(x, y, battery, width, height);
//}

//// 打印温度计图
//void Draw_wdj(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
//{
//    Draw_Icon(x, y, wdj_Data, width, height);
//}

////打印闹钟
void Draw_alarm(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
		OLED_ShowImage(100,45,16,16,alalrm_data);

}


//// 打印嘉立创logo（88x64像素）
//void Draw_JLC_Logo(uint8_t x, uint8_t y)
//{
//    Draw_Icon(x, y, jlc_logo, 88, 64);
//}


