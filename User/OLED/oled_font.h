#ifndef __OLED_FONT__H
#define __OLED_FONT__H

#include "stm32f4xx.h"                   


extern const  uint8_t  OLED_F6x12[][12];


extern const  uint8_t  OLED_F12x12[][24];
/****************************************8*16的点阵************************************/
//extern  const uint8_t OLED_F8x16[][16];
  
extern const uint8_t oled_16xChar[][36];

extern const uint8_t sd_t_[];  //水滴图像

extern const uint8_t wdj_Data[];

extern const uint8_t alalrm_data_[]; //闹钟图像
extern const uint8_t battery_[];			//电池图像
/****************************************8*16的点阵************************************/


extern unsigned char zf24[][36];
 
extern unsigned char hz[];		/*年月日*/
 
extern char zf_index[];

extern const unsigned  char  jlc_logo[];  //嘉立创log，88*64

//中文字体
extern const unsigned char Hzk1[][32];




#endif 
