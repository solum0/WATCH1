#include "delay.h"
#include "usart.h"
void Delay_TIM_Init(void)
{
    // 使用TIM2作为微秒延迟定时器
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;           // 最大计数值
    TIM_TimeBaseStructure.TIM_Prescaler = (SystemCoreClock / 1000000) - 1;  // 1MHz
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    
    TIM_Cmd(TIM2, ENABLE);
}

void Delay_us(uint32_t us)
{
		taskENTER_CRITICAL();
    static uint8_t initialized = 0;
    if (!initialized) {
        Delay_TIM_Init();
        initialized = 1;
    }
    
    uint16_t start = TIM2->CNT;
    
    // 处理计数器溢出
    while ((TIM2->CNT - start) < us) {
        if (us > 50) {  // 长时间延迟允许任务切换
            taskYIELD();
        }
    }
		taskEXIT_CRITICAL();
}



