// motor.c
#include "motor.h"
#include "math.h"
#include "delay.h"

void Motor_GPIO_Init(void)
{
//    GPIO_InitTypeDef GPIO_InitStructure;

//    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);  // 使能 GPIOA 时钟

//    // 配置 PA8 为 TIM1_CH1
//    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;  // 使用 PA8
//    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;  // 复用功能
//    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; // 推挽输出
//    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;   // 上拉
//    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; // 速度 50MHz
//    GPIO_Init(GPIOA, &GPIO_InitStructure);

//    // 将 PA8 映射到 TIM1
//    GPIO_PinAFConfig(GPIOA, GPIO_PinSource8, GPIO_AF_TIM1);

//    // 初始化 TIM1
//    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

//    TIM_TimeBaseInitTypeDef Tim_init_base;
//    Tim_init_base.TIM_CounterMode = TIM_CounterMode_Up;
//    Tim_init_base.TIM_Period = 1618;       // ARR = 9
//    Tim_init_base.TIM_Prescaler = 37;    // PSC = 167
//    Tim_init_base.TIM_RepetitionCounter = 0;
//    TIM_TimeBaseInit(TIM1, &Tim_init_base);

//		TIM_ARRPreloadConfig(TIM1, ENABLE);
//    TIM_Cmd(TIM1, ENABLE);
//		
//    TIM_OCInitTypeDef tim_compare_pwm;
//    tim_compare_pwm.TIM_OCMode = TIM_OCMode_PWM1;
//    tim_compare_pwm.TIM_OutputState = TIM_OutputState_Enable;
//    tim_compare_pwm.TIM_OutputNState = TIM_OutputNState_Disable; // 互补输出禁用 (对于TIM1高级定时器）)
//    tim_compare_pwm.TIM_OCPolarity = TIM_OCPolarity_High;
//    tim_compare_pwm.TIM_OCNPolarity = TIM_OCNPolarity_High;
//    tim_compare_pwm.TIM_Pulse = 0; // 初始占空比为 0
//    TIM_OC1Init(TIM1, &tim_compare_pwm);
    GPIO_InitTypeDef GPIO_InitStruct;
    
    // 使能 GPIOA 时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    
    // 配置 PA8 为推挽输出
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStruct);



}

void Motor_GPIO_ON(void)
{
//    TIM_CtrlPWMOutputs(TIM1, ENABLE); // 使能 PWM 输出
//    TIM_SetCompare1(TIM1, 810);     // 设置CCR占空比 50%
	  GPIO_SetBits(GPIOA, GPIO_Pin_8);
}

void Motor_GPIO_off(void)
{
//    TIM_SetCompare1(TIM1, 0);
//		TIM_CtrlPWMOutputs(TIM1, DISABLE);
	GPIO_ResetBits(GPIOA, GPIO_Pin_8);
}
