#include "ADC_battery.h"
#include "stm32f4xx.h" 
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_adc.h"
#include "stm32f4xx_rcc.h"
#include "FreeRTOS.h"
#include "task.h"

volatile uint16_t adc_buffer[ADC_BUFFER_SIZE];    // DMA buffer M0
volatile uint16_t adc_buffer_b[ADC_BUFFER_SIZE];  // DMA buffer M1
volatile uint16_t *adc_ready_buffer = adc_buffer; // Last completed segment
volatile uint16_t adc_ready_count = ADC_BUFFER_SIZE;

/**
 * @brief  ADC1 初始化 (TIM4触发 + DMA + 循环模式)
 */
void ADC_Battery_Init(void)
{
    // 1. 开启时钟 (ADC1 在 APB2，GPIOA 在 AHB1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE); 
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE); 

    // 2. 配置 GPIOA_Pin_1 为模拟输入
    GPIO_InitTypeDef adc_gpio_cmd;
    adc_gpio_cmd.GPIO_Mode = GPIO_Mode_AN; 
    adc_gpio_cmd.GPIO_Pin = GPIO_Pin_1;
    adc_gpio_cmd.GPIO_PuPd = GPIO_PuPd_NOPULL; 
    GPIO_Init(GPIOA, &adc_gpio_cmd);

    // 3. 初始化子模块
    adc_tim_init();           // 定时器初始化 (现为TIM4)
    ADC_dma_int();            // DMA 初始化
    NVIC_adc_Configuration(); // 中断配置

    // 4. ADC 通用配置
    ADC_CommonInitTypeDef adc_config_cmd;
    adc_config_cmd.ADC_Mode = ADC_Mode_Independent;
    adc_config_cmd.ADC_DMAAccessMode = ADC_DMAAccessMode_1;
    adc_config_cmd.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_20Cycles;
    // F411 APB2=100MHz, Div4 = 25MHz (ADC最高允许36MHz)
    adc_config_cmd.ADC_Prescaler = ADC_Prescaler_Div4; 
    ADC_CommonInit(&adc_config_cmd);

    // 5. ADC1 具体参数配置
    ADC_InitTypeDef adc_cmd_config;
    adc_cmd_config.ADC_ContinuousConvMode = DISABLE;             // 由定时器触发，不开启连续转换
    adc_cmd_config.ADC_DataAlign = ADC_DataAlign_Right;          // 右对齐
    
    // 【修改点1】：改为 TIM4 的 CC4 触发
    adc_cmd_config.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T4_CC4; 
    adc_cmd_config.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_Rising; 
    adc_cmd_config.ADC_NbrOfConversion = 1; 
    adc_cmd_config.ADC_Resolution = ADC_Resolution_12b; 
    adc_cmd_config.ADC_ScanConvMode = DISABLE; 
    ADC_Init(ADC1, &adc_cmd_config);
	
    // 6. 配置规则通道与启动
    ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);	
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_15Cycles);
	
    ADC_DMACmd(ADC1, ENABLE); 
    ADC_Cmd(ADC1, ENABLE);

    // 【修改点2】：开启 TIM4 开始触发采样
    TIM_Cmd(TIM4, ENABLE);
}

/**
 * @brief  TIM4 初始化：用于产生 1ms 一次的触发信号
 * 定时周期 = (PSC + 1) * (ARR + 1) / TIMxCLK
         = (199 + 1) * (49999 + 1) / 100,000,000
         = 0.1 s
         = 100 ms
 */
void adc_tim_init(void)
{
   RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

   TIM_TimeBaseInitTypeDef TIM4_adc_cmd;
   TIM4_adc_cmd.TIM_CounterMode = TIM_CounterMode_Up;
   TIM4_adc_cmd.TIM_RepetitionCounter = 0;
   TIM4_adc_cmd.TIM_Period = 49999;                   // ARR = 49999 
   TIM4_adc_cmd.TIM_Prescaler = 199;                 // PSC = 199，计数频率 = 100MHz / (199 + 1) = 500 kHz    
   TIM4_adc_cmd.TIM_ClockDivision = TIM_CKD_DIV1;
   TIM_TimeBaseInit(TIM4, &TIM4_adc_cmd);

   // 【修改点4】：配置 OC4 模式 (因为 ADC_ExternalTrigConv_T4_CC4 需要通道4)
   TIM_OCInitTypeDef TIM_OCInitStructure;
   TIM_OCStructInit(&TIM_OCInitStructure);
   TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
   TIM_OCInitStructure.TIM_Pulse = 2500; // 前 1250 µs 输出高电平，后 1250 µs 输出低电平。
   TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
   TIM_OC4Init(TIM4, &TIM_OCInitStructure);
	
}

/**
 * @brief  DMA2 初始化：ADC1 对应 Stream 0, Channel 0
 */
void ADC_dma_int(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

    DMA_InitTypeDef dma_config_init;
    DMA_StructInit(&dma_config_init); // 【关键修复】：将结构体内容清零并赋予默认值

    dma_config_init.DMA_Channel = DMA_Channel_0;
    dma_config_init.DMA_DIR = DMA_DIR_PeripheralToMemory; 
    dma_config_init.DMA_Memory0BaseAddr = (uint32_t)adc_buffer; 
    dma_config_init.DMA_MemoryInc = DMA_MemoryInc_Enable; 
    dma_config_init.DMA_BufferSize = ADC_BUFFER_SIZE; 
    dma_config_init.DMA_Mode = DMA_Mode_Circular;               // 循环模式
    dma_config_init.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR; 
    dma_config_init.DMA_PeripheralInc = DMA_PeripheralInc_Disable; 
    dma_config_init.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; 
    dma_config_init.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord; 
    dma_config_init.DMA_Priority = DMA_Priority_High; 
    dma_config_init.DMA_FIFOMode = DMA_FIFOMode_Disable; 
	
    DMA_Init(DMA2_Stream0, &dma_config_init);
    DMA_DoubleBufferModeConfig(DMA2_Stream0, (uint32_t)adc_buffer_b, DMA_Memory_0);
    DMA_DoubleBufferModeCmd(DMA2_Stream0, ENABLE);

    DMA_ClearFlag(DMA2_Stream0, DMA_FLAG_TCIF0 | DMA_FLAG_HTIF0 | DMA_FLAG_TEIF0 | DMA_FLAG_DMEIF0 | DMA_FLAG_FEIF0);
    DMA_ITConfig(DMA2_Stream0, DMA_IT_TC, ENABLE);
    DMA_ITConfig(DMA2_Stream0, DMA_IT_HT, ENABLE);
    DMA_Cmd(DMA2_Stream0, ENABLE);
}

/**
 * @brief  配置 DMA 中断优先级
 */
void NVIC_adc_Configuration(void)
{
    NVIC_InitTypeDef NVIC_ADC_CONFIG;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4); 

    NVIC_ADC_CONFIG.NVIC_IRQChannel = DMA2_Stream0_IRQn;
    NVIC_ADC_CONFIG.NVIC_IRQChannelPreemptionPriority = 11; 
    NVIC_ADC_CONFIG.NVIC_IRQChannelSubPriority = 0; 
    NVIC_ADC_CONFIG.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_ADC_CONFIG);
}

/**
 * @brief 开启ADC采集
 * 
 */
void ADC_Start(void)
{
   		// 1. 开启 GPIOB 时钟
RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

// 2. 配置 PB12 为推挽输出
GPIO_InitTypeDef GPIO_InitStructure;
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;       // 普通输出模式
GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;      // 推挽输出 (Push-Pull)
GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;   // 输出速度 (2MHz/25MHz/50MHz/100MHz均可)
GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;    // 无上下拉 (推挽模式下通常不需要)
GPIO_Init(GPIOB, &GPIO_InitStructure);

// PB12 置位（输出高电平）
GPIO_SetBits(GPIOB, GPIO_Pin_12);

}

