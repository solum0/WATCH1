#ifndef __EM7028_H__
#define __EM7028_H__

#include <stdint.h>

#define EM7028_ID    0x36U
#define EM7028_ADDR  0x24U

/* Register map */
#define ID_REG            0x00U
#define HRS_CFG           0x01U
#define HRS_INT_CTRL      0x02U
#define HRS_LT_L          0x03U
#define HRS_LT_H          0x04U
#define HRS_HT_L          0x05U
#define HRS_HT_H          0x06U
#define LED_CRT           0x07U
#define HRS2_DATA_OFFSET  0x08U
#define HRS2_CTRL         0x09U
#define HRS2_GAIN_CTRL    0x0AU
#define HRS1_CTRL         0x0DU
#define INT_CTRL          0x0EU
#define SOFT_RESET        0x0FU

#define HRS2_DATA0_L      0x20U
#define HRS2_DATA0_H      0x21U
#define HRS2_DATA1_L      0x22U
#define HRS2_DATA1_H      0x23U
#define HRS2_DATA2_L      0x24U
#define HRS2_DATA2_H      0x25U
#define HRS2_DATA3_L      0x26U
#define HRS2_DATA3_H      0x27U

#define HRS1_DATA0_L      0x28U
#define HRS1_DATA0_H      0x29U
#define HRS1_DATA1_L      0x2AU
#define HRS1_DATA1_H      0x2BU
#define HRS1_DATA2_L      0x2CU
#define HRS1_DATA2_H      0x2DU
#define HRS1_DATA3_L      0x2EU
#define HRS1_DATA3_H      0x2FU

#define EM7028_HR_SAMPLE_INTERVAL_MS 25U
#define EM7028_HR_VALID_MIN          50U
#define EM7028_HR_VALID_MAX          120U
#define EM7028_HR_LOST_LIMIT         120U
#define EM7028_HR_AVG_WINDOW_MS      3000U
#define EM7028_HR_AVG_MIN_SAMPLES    5U
#define EM7028_HR_MEDIAN_WINDOW_SIZE 6U
#define EM7028_HR_STEP_LIMIT         5U

uint8_t EM7028_ReadOneReg(unsigned char reg_addr);
void EM7028_WriteOneReg(unsigned char reg_addr, unsigned char dat);

uint8_t EM7028_Get_ID(void);
uint8_t EM7028_hrs_init(void);
uint8_t EM7028_hrs_Enable(void);
uint8_t EM7028_hrs_DisEnable(void);
uint16_t EM7028_Get_HRS1(void);

void EM7028_HR_SetEnabled(uint8_t enabled);
void HR_SpO2_Hander(void);
#endif
