#include "key.h"
#include "stm32f4xx_gpio.h"
#include "motor.h"
#include "max30102.h"  

#include "FreeRTOS.h"
#include "task.h"

#include "semphr.h"
#include "queue.h"
#include "timers.h"

//WK-UP PA0, KEY1-PD10 		关机KEY3 - PA4		
//CTL - PB13(开机默认高电平，长按KEY3 关机), PB12 - BAT_ADC_EN( 默认低电平,key3长按使能高电平关机)

// 全局变量定义
uint8_t EXTI_IRQ_KEY = 0;             // 存储中断触发的按键编号
volatile uint8_t KeyPressed = 0;      // 按键按下标志位（volatile防止编译器优化）
static volatile uint8_t g_key_sleep_locked = 0U;
extern volatile uint8_t max30102_int_triggered;
extern uint8_t bell_onoff;
extern void MOTOR_GPIO_ON(void);
extern uint8_t DS3231_GetAlarmFlag(uint8_t alarmNum, uint8_t* flag);
extern uint8_t DS3231_ClearAlarmFlag(uint8_t alarmNum);
extern TimerHandle_t Sleep_switch = NULL;
void sleep_timer_callback(TimerHandle_t xTimer);
/**
 * @brief 配置外部中断
 * @param 无
 * @return 无
 */
void EXTI_Configuration(void)
{
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 使能AFIO时钟（用于GPIO与EXTI的映射）
   RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
	
	 // 配置GPIO引脚与外部中断线的映射关系
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA,EXTI_PinSource0);	// WK-UP -> PA0
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB,EXTI_PinSource10);	// KEY0 -> EXTI10
		
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB,EXTI_PinSource13);	//EXTI13（对应INT MAX30102）
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB,EXTI_PinSource1);	//SQW引脚  EXTI1
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA,EXTI_PinSource4);	//NEW_key3 PA4
	
    // 配置外部中断线10（对应KEY0）
    EXTI_InitStructure.EXTI_Line = EXTI_Line10;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;          // 中断模式
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;      // 下降沿触发
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;                    // 使能中断线
    EXTI_Init(&EXTI_InitStructure);

		//PA0 WK-UP 
    EXTI_InitStructure.EXTI_Line = EXTI_Line0;
		EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;						// 使用事件模式，不产生中断
		EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;				//上升沿
		EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

		
		 // 配置外部中断线1（对应SQW/INT DS3231）PB1
		EXTI_InitStructure.EXTI_Line = EXTI_Line1;
		EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;          // 中断模式
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;      // 下降沿触发
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;                    // 使能中断线
    EXTI_Init(&EXTI_InitStructure);
		
		EXTI_InitStructure.EXTI_Line = EXTI_Line4;
		EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;          // 中断模式
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;      // 下降沿触发
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;                    // 使能中断线
    EXTI_Init(&EXTI_InitStructure);
    
    // 配置外部中断线13（对应INT MAX30102）
    EXTI_InitStructure.EXTI_Line = EXTI_Line13;
		EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;          // 中断模式
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;      // 下降沿触发
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;                    // 使能中断线
    EXTI_Init(&EXTI_InitStructure);
	
    // 配置NVIC中断优先级 - EXTI10中断（KEY0） EXTI13 INT(max30102)
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);         // 优先级分组
	
    NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 12; // 抢占优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;         // 子优先级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;              // 使能中断通道
    NVIC_Init(&NVIC_InitStructure);

		// WK-UP -> PA0 
    NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
    NVIC_Init(&NVIC_InitStructure);
		
		//PD1 SQW
		NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn;
    NVIC_Init(&NVIC_InitStructure);
		
		//PA4 new_key3
		NVIC_InitStructure.NVIC_IRQChannel = EXTI4_IRQn;
    NVIC_Init(&NVIC_InitStructure);
}

/**
 * @brief 按键初始化函数
 * @param 无
 * @return 无
 */

void Key_Init(void)
{
	
	
    // 开启GPIOB时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	
    GPIO_InitTypeDef GPIO_InitStructure;
		//开关机key3 - PB4, 	ds3231中断引脚 - PB1, 	max30102中断引脚 - PB13
    GPIO_InitStructure.GPIO_Pin =  KEY1| GPIO_Pin_13| GPIO_Pin_1 ; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; 
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;    // 上拉输入模式
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);   // 初始化GPIOB 和 GPIOA
	
		 // 开启GPIOA时钟
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA ,ENABLE);
		GPIO_InitStructure.GPIO_Pin = KEY3_switch; 
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;  
	//	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;  
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;	//设置无上拉,	已有硬件上拉
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // 速度设置
    GPIO_Init(GPIOA, &GPIO_InitStructure);   
		
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0; 
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;    
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;	
	//	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;//设置无上拉,	已有硬件下拉
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // 速度设置
    GPIO_Init(GPIOA, &GPIO_InitStructure);   
    // 配置外部中断
    EXTI_Configuration();
	
		power_switch_init();  //CTL和BAT_ADC_EN 
    	//睡眠定时器
	Sleep_switch = xTimerCreate(
									"SLEEPTimer",
									pdMS_TO_TICKS(3000),	//一秒时间
									pdTRUE,
									(void *)0,
									sleep_timer_callback
									);
}

//PD10 - KEY0 PD9 - KEY1 PD8 -KEY2  PA0 - KEY 4 按下上升沿
/**
 * @brief EXTI1中断处理函数（KEY0）
 * @param 无
 * @return 无
 */
void EXTI15_10_IRQHandler(void) 
{		
    if (EXTI_GetITStatus(EXTI_Line10) != RESET)       // 检查EXTI10中断是否发生
    {
        if (!KeyPressed)                            
        {
					
            KeyPressed = 1;                          // 设置按键按下标志
            EXTI_IRQ_KEY = 1;                        // 记录按键编号
        }
        EXTI_ClearITPendingBit(EXTI_Line10);          // 清除中断标志位
    }
		
		if (EXTI_GetITStatus(EXTI_Line13) != RESET)  //max30102 INT
			{
				
				 max30102_int_triggered = 1; 
				EXTI_ClearITPendingBit(EXTI_Line13);
			}
}
/**
 * @brief EXTI9 和 EXTI8 中断处理函数
 * @param 无
 * @return 无
 */
//void EXTI9_5_IRQHandler(void)
//{
//    if (EXTI_GetITStatus(EXTI_Line9) != RESET)
//    {
//        if (!KeyPressed)
//        {
//            KeyPressed = 1;
//            EXTI_IRQ_KEY = 2;                        // KEY2
//        }
//        EXTI_ClearITPendingBit(EXTI_Line9);
//    }
//		if (EXTI_GetITStatus(EXTI_Line8) != RESET)
//        {
//            if (!KeyPressed)
//            {
//                KeyPressed = 1;
//                EXTI_IRQ_KEY = 3;                        // KEY3
//           
//            }
//						 EXTI_ClearITPendingBit(EXTI_Line8);
//        }	
//}

void EXTI0_IRQHandler(void) 
{
  		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
   if (EXTI_GetITStatus(EXTI_Line0) != RESET)       // 检查EXTI1中断是否发生
   {
				xTimerResetFromISR(Sleep_switch,&xHigherPriorityTaskWoken);
				portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
			
				//唤醒
				 if(PWR_GetFlagStatus(PWR_FLAG_SB) != RESET)
					{
							PWR_ClearFlag(PWR_FLAG_SB);
					}

         if (!KeyPressed)                             // 防止重复处理
        {
            KeyPressed = 1;                          // 设置按键按下标志
            EXTI_IRQ_KEY = 9;                        // 记录按键编号
        }

       EXTI_ClearITPendingBit(EXTI_Line0);          // 清除中断标志位


   }

}

// /**
//  * @brief EXTI0中断处理函数
//  * @param 无
//  * @return 无
//  */
// void EXTI0_IRQHandler(void) 
// {
//   		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//    if (EXTI_GetITStatus(EXTI_Line0) != RESET)       // 检查EXTI1中断是否发生
//    {
// 				xTimerResetFromISR(Sleep_switch,&xHigherPriorityTaskWoken);
// 				portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
			
// 				//唤醒
// 				 if(PWR_GetFlagStatus(PWR_FLAG_SB) != RESET)
// 					{
// 							PWR_ClearFlag(PWR_FLAG_SB);
// 					}
//        EXTI_ClearITPendingBit(EXTI_Line0);          // 清除中断标志位
//    }
//     if (EXTI_GetITStatus(EXTI_Line0) != RESET)       // 检查EXTI1中断是否发生
//     {
//         if (!KeyPressed)                             // 防止重复处理
//         {
//             KeyPressed = 1;                          // 设置按键按下标志
//             EXTI_IRQ_KEY = 9;                        // 记录按键编号
//         }
//         EXTI_ClearITPendingBit(EXTI_Line0);          // 清除中断标志位
//     }
// }

void Key_SetSleepLock(uint8_t locked)
{
    g_key_sleep_locked = (locked != 0U) ? 1U : 0U;
}

//wk-up PA0		
void sleep_timer_callback(TimerHandle_t xTimer)
{
		uint8_t current_state = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
		if (g_key_sleep_locked != 0U) {
			return;
		}

		if(current_state == SET)
		{	 
      OLED_WriteCommand(0xAE);	//设置显示开启/关闭，0xAE关闭，0xAF开启
			Enter_Standby_Mode(); 	//进入待机模式
		} 
}

/**
 * @brief 中断方式获取按键值
 * @param 无
 * @return 按键编号（1-4），无按键按下返回0
 */
uint8_t GetKeyNum(void)
{
    if (KeyPressed)
    {
        uint8_t keyNum = 0;
    //   vTaskDelay(pdMS_TO_TICKS(20)); // 延时消抖
        
        // 根据中断记录的按键编号检查实际按键状态
        switch (EXTI_IRQ_KEY)
        {
            case 1: // KEY0	
       //         if (GPIO_ReadInputDataBit(KEY_GPIO_PORT, KEY0) == 0)
                    keyNum = 4;
                break;
//            case 2: // KEY1  
//             //   if (GPIO_ReadInputDataBit(KEY_GPIO_PORT, KEY1) == 0)
//                    keyNum = 2;
//                break;
//            case 3: // KEY2  	
//          //      if (GPIO_ReadInputDataBit(KEY_GPIO_PORT, KEY2) == 0)
//                    keyNum = 3;
//                break;
            case 9: // KEY3  PA0
           //     if (GPIO_ReadInputDataBit(GPIOA, KEY3) == 1)
                    keyNum = 9;
                break;
            default:
							keyNum = 0;
                break;
        }
        
        KeyPressed = 0; // 复位按键状态
        return keyNum;
    }
    return 0;
}

///**
// * @brief 轮询方式获取按键值（阻塞式） 未使用！！！
// * @return 按键编号（1-4），无按键按下返回0
// * @note 此函数是阻塞式操作，当按键按住不放时，函数会卡住，直到按键松手
// */
//uint8_t Key_GetNum(void)
//{
//    uint8_t KeyNum = 0; // 默认无按键按下
//    
//    // 检查KEY0
//    if (GPIO_ReadInputDataBit(GPIOD, KEY0) == 0)
//    {
//        vTaskDelay(pdMS_TO_TICKS(20)); // 延时消抖
//        while (GPIO_ReadInputDataBit(GPIOD, KEY0) == 0); // 等待按键释放

//        KeyNum = 1;
//    }
//    
//    // 检查KEY1
//    if (GPIO_ReadInputDataBit(GPIOD, KEY1) == 0)
//    {
//        vTaskDelay(pdMS_TO_TICKS(20));
//        while (GPIO_ReadInputDataBit(GPIOD, KEY1) == 0);

//        KeyNum = 2;
//    }
//    
//    // 检查KEY2
//    if (GPIO_ReadInputDataBit(GPIOD, KEY2) == 0)
//    {
//        vTaskDelay(pdMS_TO_TICKS(20));
//        while (GPIO_ReadInputDataBit(GPIOD, KEY2) == 0);

//        KeyNum = 3;
//    }
//    
//    // 检查KEY3
//    if (GPIO_ReadInputDataBit(GPIOA, KEY3) == 1)
//    {
//        vTaskDelay(pdMS_TO_TICKS(20));
//        while (GPIO_ReadInputDataBit(GPIOD, KEY3) == 1);

//        KeyNum = 4;
//    }
//    
//    return KeyNum; // 返回按键编号
//}

void power_switch_init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	
		//CTL默认高电平
		GPIO_InitStructure.GPIO_Pin = CTL; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // 速度设置
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;    // 上拉输出模式
    GPIO_Init(GPIOB, &GPIO_InitStructure); 
		GPIO_WriteBit(GPIOB, CTL, Bit_SET);
	
		//BAT_ADC_EN PB12 默认低电平
		GPIO_InitStructure.GPIO_Pin = BAT_ADC_EN; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; 
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;    
    GPIO_Init(GPIOB, &GPIO_InitStructure); 
		GPIO_WriteBit(GPIOB, BAT_ADC_EN, Bit_RESET);
}	


void Enter_Standby_Mode(void)
{
    // 清除WKUP事件标志
    EXTI_ClearFlag(EXTI_Line0);
    
    // 使能PWR时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    
    // 使能WKUP引脚唤醒功能
    PWR_WakeUpPinCmd(ENABLE);
    
    // 清除待机标志
    PWR_ClearFlag(PWR_FLAG_WU);
    
    // 进入待机模式
    PWR_EnterSTANDBYMode();
}





