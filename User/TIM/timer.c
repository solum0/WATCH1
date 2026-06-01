# include "timer.h"
# include "oled.h"
#include "usart.h"
#include "mpu6050.h"

//TIM2 步数，TIM3 秒表 TIM4 电池

volatile uint8_t start_flag = 0;  // 开始标志
volatile uint8_t zero_flag = 0;   // 清零标志
volatile uint8_t update_flag = 0; // 更新标志

volatile uint8_t m = 0; 		// 分钟
volatile uint8_t s = 0; 		// 秒
volatile uint8_t ms = 0; 	// 毫秒

volatile uint16_t timestamp = 0;  //时间戳

uint8_t tim_PeriodNum = 0;


/*
*秒表
*通用定时器中断初始化
*调用方法: TIM3_Int_Init(100-1,7200-1)   //计数到计数到100，为10ms间隔一次计数  
*/
void TIM3_Int_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); // 时钟使能

    tim_PeriodNum = 99 + 1;   //分配数，应当根据pcs 和 arr 进行计算 

    TIM_TimeBaseStructure.TIM_Period = 99; // 设置在下一个更新事件装入活动的自动重装载寄存器周期的值
	TIM_TimeBaseStructure.TIM_Prescaler = 8399; // 设置用来作为TIMx时钟频率除数的预分频值 ，APB1为42Mhz，定时器时钟为84Mhz 
    TIM_TimeBaseStructure.TIM_ClockDivision = 0; // 设置时钟分割:TDTS = Tck_tim
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  // TIM向上计数模式
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure); // 根据TIM_TimeBaseInitStruct中指定的参数初始化TIMx的时间基数单位

    
    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;  // TIM3中断
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 13;  // 先占优先级12级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;  // 从优先级0级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; // IRQ通道被使能
    NVIC_Init(&NVIC_InitStructure);  // 根据NVIC_InitStruct中指定的参数初始化外设NVIC寄存器

    //TIM_Cmd(TIM3, ENABLE);  // 使能TIMx外设
}

//为步数提供计时
void TIM2_Int_Init(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE); // 时钟使能


    TIM_TimeBaseStructure.TIM_Period = 499; // 设置在下一个更新事件装入活动的自动重装载寄存器周期的值
    TIM_TimeBaseStructure.TIM_Prescaler = 8399; // 设置用来作为TIMx时钟频率除数的预分频值 
    TIM_TimeBaseStructure.TIM_ClockDivision = 0; // 设置时钟分割:TDTS = Tck_tim
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  // TIM向上计数模式
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure); // 根据TIM_TimeBaseInitStruct中指定的参数初始化TIMx的时间基数单位

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE); // 使能TIM2中断
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;  // TIM2中断
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 7;  // 先占优先级13级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;  // 从优先级0级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; // IRQ通道被使能
    NVIC_Init(&NVIC_InitStructure);  // 根据NVIC_InitStruct中指定的参数初始化外设NVIC寄存器

    TIM_Cmd(TIM2, ENABLE);  // 使能TIMx外设


}	

// 启用定时器
void enableClock(void)
{		
		TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE); // 使能TIM3中断
    TIM_Cmd(TIM3, ENABLE);  // 启用TIMx外设
}

// 禁用定时器
void disableClock(void)
{
    TIM_Cmd(TIM3, DISABLE); // 禁用TIMx外设
}



void TIM3_IRQHandler(void)  //提供了一个精确的 10ms 时间基准
{																										  //72MHz / (7199 + 1) = 10,000 Hz，频率与周期互为倒数
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) //1/10,000 秒 = 0.1ms，再*100(arr + 1) = 10ms
    { // 检查指定的TIM中断发生与否
        update_flag = 1;        //设置更新时间标志
        timestamp++;			//记录中断次数
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);  // 清除TIM3的中断待处理位
    }
}

//获取计时器
void get_timer(SecTimer* ans)
{
    ans->minute = m;
    ans->msec = ms;
    ans->second = s;
}


//计时器计数控制
void timer_loop(void) //更新秒表的时间值（毫秒、秒、分钟）
{
    if (update_flag)
    {
        update_flag = 0; // 清除更新标志
//		printf("ms = %d",ms);
        uint16_t temp = timestamp + ms;         //ms代表10ms 的倍数
        if (temp >= tim_PeriodNum) 			 //tim_PeriodNum = arr + 1 = 100
        {
            s += temp / tim_PeriodNum;
            ms = temp % tim_PeriodNum;
            timestamp = 0;
        }
        else														
        {
            ms += timestamp;
            timestamp = 0;
        }

        if (s >= 60)  //检查秒数 s 是否超过 60
        {
            m += s / 60; //若超过，增加m
            s = s % 60;  
        }
    }

}

//秒表计时器
void show_timer(void)
{
    
    if (m > 100) {
        OLED_ShowNum(8, 20, m,2,OLED_12x24); // 时
    } else {
        OLED_ShowNum(16, 20, m,2,OLED_12x24); // 时
    }

			OLED_ShowChar(40, 20, ':',OLED_12x24);
			OLED_ShowNum(52, 20, s, 2, OLED_12x24) ; // 分
			OLED_ShowChar(76, 20, ':',OLED_12x24);
			OLED_ShowNum(88, 20, ms, 2, OLED_12x24); // 秒
		
			OLED_Update();
			
}

//秒表计时控制
void main_timer_control(void)
{
   
	if (zero_flag) {
        m = 0;
        s = 0;
        ms = 0;
        zero_flag = 0;
        printf("Timer reset to zero\n");
    }

    if (start_flag) {
        timer_loop();  // 计数器
    }
    show_timer(); 
}

void zero_timer(void)
{

    m = 0;
    s = 0;
    ms = 0;

}

