#ifndef __ADC_BATTERY_H__
#define __ADC_BATTERY_H__

#include "stm32f4xx.h"

#define ADC_BUFFER_SIZE   30

extern volatile uint16_t adc_buffer[ADC_BUFFER_SIZE];
extern volatile uint16_t adc_buffer_b[ADC_BUFFER_SIZE];
extern volatile uint16_t *adc_ready_buffer;
extern volatile uint16_t adc_ready_count;

void ADC_Battery_Init(void);
void adc_tim_init(void);
void ADC_dma_int(void);
void NVIC_adc_Configuration(void);
void ADC_Start(void);

#endif
