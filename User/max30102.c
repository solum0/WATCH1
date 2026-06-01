#include "max30102.h"
#include "i2c.h"

volatile uint8_t max30102_int_triggered = 0;


uint8_t max30102_FIFO_ReadBytes(uint8_t Register_Address,uint8_t* Data)
{	
	return MAX30102_IIC_ReadBytes(Register_Address,Data,6);
	
}

void MAX30102_Init(void)
{

	MAX30102_IIC_WriteByte(REG_INTR_ENABLE_1,0xc0);	// INTR setting
	MAX30102_IIC_WriteByte(REG_INTR_ENABLE_2,0x00);
	MAX30102_IIC_WriteByte(REG_FIFO_WR_PTR,0x00);  	//FIFO_WR_PTR[4:0]
	MAX30102_IIC_WriteByte(REG_OVF_COUNTER,0x00);  	//OVF_COUNTER[4:0]
	MAX30102_IIC_WriteByte(REG_FIFO_RD_PTR,0x00);  	//FIFO_RD_PTR[4:0]
	MAX30102_IIC_WriteByte(REG_PILOT_PA,0x7f);   	// Choose value for ~ 25mA for Pilot LED
	
//	MAX30102_IIC_WriteByte(REG_MODE_CONFIG,0x03);  	//0x02 for Red only, 0x03 for SpO2 mode 0x07 multimode LED
	
	MAX30102_IIC_WriteByte(REG_LED1_PA,0x24);   	//Choose value for ~ 7mA for LED1
	MAX30102_IIC_WriteByte(REG_LED2_PA,0x24);   	// Choose value for ~ 7mA for LED2
	MAX30102_IIC_WriteByte(REG_SPO2_CONFIG,0x27);  	// SPO2_ADC range = 4096nA, SPO2 sample rate (100 Hz), LED pulseWidth (400uS)  
	MAX30102_IIC_WriteByte(REG_FIFO_CONFIG,0x1F);  	//sample avg = 4, fifo rollover=true, fifo almost full = 15
	
	MAX30102_IIC_WriteByte(REG_MODE_CONFIG,0x83);
//		MAX30102_IIC_WriteByte(REG_MODE_CONFIG,0x80);  //关机
//		MAX30102_IIC_WriteByte(REG_MODE_CONFIG,0x40);	 //复机
	
}
void MAX30102_Reset(void)
{

	MAX30102_IIC_WriteByte(REG_MODE_CONFIG,0x03);
	
}

void MAX30102_off(void)
{

	MAX30102_IIC_WriteByte(REG_MODE_CONFIG,0x83);  //关机

}

void maxim_max30102_read_fifo(uint32_t *pun_red_led, uint32_t *pun_ir_led)
{
	uint32_t un_temp;
	unsigned char uch_temp;
	char ach_i2c_data[6];
	*pun_red_led=0;
	*pun_ir_led=0;

  
  //read and clear status register
  MAX30102_IIC_ReadByte(REG_INTR_STATUS_1, &uch_temp);
  MAX30102_IIC_ReadByte(REG_INTR_STATUS_2, &uch_temp);
  
  MAX30102_IIC_ReadBytes(REG_FIFO_DATA,(uint8_t *)ach_i2c_data,6);	//每个样本6个字节
  
  un_temp=(unsigned char) ach_i2c_data[0];
  un_temp<<=16;
  *pun_red_led+=un_temp;
  un_temp=(unsigned char) ach_i2c_data[1];
  un_temp<<=8;
  *pun_red_led+=un_temp;
  un_temp=(unsigned char) ach_i2c_data[2];
  *pun_red_led+=un_temp;
  
  un_temp=(unsigned char) ach_i2c_data[3];
  un_temp<<=16;
  *pun_ir_led+=un_temp;
  un_temp=(unsigned char) ach_i2c_data[4];
  un_temp<<=8;
  *pun_ir_led+=un_temp;
  un_temp=(unsigned char) ach_i2c_data[5];
  *pun_ir_led+=un_temp;
  *pun_red_led&=0x03FFFF;  //Mask MSB [23:18]
  *pun_ir_led&=0x03FFFF;  //Mask MSB [23:18]
}

uint8_t MAX30102_IIC_ReadBytes( uint8_t ReadReg,uint8_t* Receive,uint8_t dataLength)
{		
	return	h_I2C_ReadReceives(I2C_MAX30102_ADDRESS, ReadReg, Receive, dataLength);
}

uint8_t MAX30102_IIC_ReadByte(uint8_t ReadReg,uint8_t* Receive)
{				  	  	    																 
  return	h_I2C_ReadReceives(I2C_MAX30102_ADDRESS, ReadReg, Receive, 1);
}

uint8_t MAX30102_IIC_WriteByte(uint8_t WriteReg,uint8_t data)
{				   	  	    																 
  uint8_t bytesToSend[] = {WriteReg, data};
	return  h_I2C_SendBytes(I2C_MAX30102_WRITE, bytesToSend, 2);
	 
}

const uint16_t auw_hamm[31]={ 41,    276,    512,    276,     41 }; //Hamm=  long16(512* hamming(5)');
//uch_spo2_table is computed as  -45.060*ratioAverage* ratioAverage + 30.354 *ratioAverage + 94.845 ;
const uint8_t uch_spo2_table[184]={ 95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99, 
                            99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 
                            100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97, 
                            97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91, 
                            90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81, 
                            80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67, 
                            66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50, 
                            49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29, 
                            28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5, 
                            3, 2, 1 } ;
static  int32_t an_dx[ BUFFER_SIZE-MA4_SIZE]; // delta
static  int32_t an_x[ BUFFER_SIZE]; //ir
static  int32_t an_y[ BUFFER_SIZE]; //red
														
/**
* \brief        Calculate the heart rate and SpO2 level
* \par          Details
*               By detecting  peaks of PPG cycle and corresponding AC/DC of red/infra-red signal, the ratio for the SPO2 is computed.
*               Since this algorithm is aiming for Arm M0/M3. formaula for SPO2 did not achieve the accuracy due to register overflow.
*               Thus, accurate SPO2 is precalculated and save longo uch_spo2_table[] per each ratio.
*
* \param[in]    *pun_ir_buffer           - IR sensor data buffer
* \param[in]    n_ir_buffer_length      - IR sensor data buffer length
* \param[in]    *pun_red_buffer          - Red sensor data buffer
* \param[out]    *pn_spo2                - Calculated SpO2 value
* \param[out]    *pch_spo2_valid         - 1 if the calculated SpO2 value is valid
* \param[out]    *pn_heart_rate          - Calculated heart rate value
* \param[out]    *pch_hr_valid           - 1 if the calculated heart rate value is valid
*
* \retval       None
*/
void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer,  int32_t n_ir_buffer_length, uint32_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, 
                              int32_t *pn_heart_rate, int8_t  *pch_hr_valid)
{
    uint32_t un_ir_mean;
    int32_t k, n_i_ratio_count;
    int32_t i, s, m, n_exact_ir_valley_locs_count, n_middle_idx;
    int32_t n_th1, n_npks;      
    int32_t an_ir_valley_locs[15];
    int32_t an_exact_ir_valley_locs[15];
    int32_t an_dx_peak_locs[15];
    int32_t n_peak_interval_sum;
    
    int32_t n_y_ac, n_x_ac;
    int32_t n_spo2_calc; 
    int32_t n_y_dc_max, n_x_dc_max; 
    int32_t n_y_dc_max_idx, n_x_dc_max_idx; 
    int32_t an_ratio[5], n_ratio_average; 
    int32_t n_nume, n_denom;

    // --- 1. 移除直流分量 (DC Removal) ---
    un_ir_mean = 0; 
    for (k=0; k<n_ir_buffer_length; k++) un_ir_mean += pun_ir_buffer[k];
    un_ir_mean = un_ir_mean/n_ir_buffer_length;
    for (k=0; k<n_ir_buffer_length; k++) an_x[k] = pun_ir_buffer[k] - un_ir_mean; 
    
    // --- 2. 4点滑动平均 (平滑处理) ---
    for(k=0; k< BUFFER_SIZE-MA4_SIZE; k++){
        an_x[k] = (an_x[k] + an_x[k+1] + an_x[k+2] + an_x[k+3]) / 4; 
    }

    // --- 3. 计算一阶导数 (寻找变化率) ---
    for(k=0; k<BUFFER_SIZE-MA4_SIZE-1; k++)
        an_dx[k] = (an_x[k+1] - an_x[k]);

    // --- 4. 再次滑动平均 ---
    for(k=0; k< BUFFER_SIZE-MA4_SIZE-2; k++){
        an_dx[k] = (an_dx[k] + an_dx[k+1]) / 2;
    }
    
    // --- 5. 汉明窗滤波 (增强主峰，抑制旁瓣) ---
    for (i=0; i<BUFFER_SIZE-HAMMING_SIZE-MA4_SIZE-2; i++){
        s = 0;
        for(k=i; k<i+HAMMING_SIZE; k++){
            s -= an_dx[k] * auw_hamm[k-i]; 
        }
        an_dx[i] = s / 1146; 
    }

    // --- 6. [优化] 动态阈值计算 ---
    // 原代码仅使用平均值，这里我们加入最大值判断，使得阈值更稳健
    int32_t max_amp = 0;
    n_th1 = 0;
    for (k=0; k<BUFFER_SIZE-HAMMING_SIZE; k++){
        int32_t val = (an_dx[k]>0) ? an_dx[k] : -an_dx[k];
        n_th1 += val;
        if(val > max_amp) max_amp = val;
    }
    n_th1 = n_th1 / (BUFFER_SIZE-HAMMING_SIZE);

    // 如果平均噪声很小，强制提高阈值为最大振幅的 30% ~ 40%
    // 这能有效去除重搏波（重搏波通常比主波小）
    if (n_th1 < max_amp / 3) {
        n_th1 = max_amp / 3; 
    }
    // 限制最小阈值，防止全是噪声时乱计算
    if (n_th1 < 30) n_th1 = 30; 

    // n_min_distance (倒数第二个参数): 
    // 原值为 8 (对应 0.08秒, 750bpm)。
    // 修改为 25 (对应 0.25秒, 240bpm)。这意味着两心跳间距必须大于 0.25秒。
    maxim_find_peaks(an_dx_peak_locs, &n_npks, an_dx, BUFFER_SIZE-HAMMING_SIZE, n_th1, 25, 5);

    // --- 8. 心率计算 ---
    n_peak_interval_sum = 0;
    if (n_npks >= 2){
        for (k=1; k<n_npks; k++)
            n_peak_interval_sum += (an_dx_peak_locs[k] - an_dx_peak_locs[k-1]);
        
        n_peak_interval_sum = n_peak_interval_sum / (n_npks - 1);
        int32_t calculated_hr = (int32_t)(6000 / n_peak_interval_sum); // 100Hz * 60s = 6000

        // 简单的范围过滤
        if (calculated_hr > 40 && calculated_hr < 200) {
            *pn_heart_rate = calculated_hr;
            *pch_hr_valid = 1;
        } else {
            *pn_heart_rate = -999;
            *pch_hr_valid = 0;
        }
    }
    else {
        *pn_heart_rate = -999;
        *pch_hr_valid = 0;
    }
            
    // --- 9. 寻找 SpO2 计算用的波谷 (IR 和 RED 的原始数据) ---
    for (k=0; k<n_npks; k++)
        an_ir_valley_locs[k] = an_dx_peak_locs[k] + HAMMING_SIZE/2; 

    // 重新加载原始数据
    for (k=0; k<n_ir_buffer_length; k++) {
        an_x[k] = pun_ir_buffer[k]; 
        an_y[k] = pun_red_buffer[k]; 
    }

    // 精确寻找波谷位置
    n_exact_ir_valley_locs_count = 0; 
    for(k=0; k<n_npks; k++){
        int32_t un_only_once = 1;
        m = an_ir_valley_locs[k];
        int32_t n_c_min = 16777216; // 2^24
        
        // 搜索范围稍微扩大一点 +/- 5
        if (m+5 < BUFFER_SIZE-HAMMING_SIZE && m-5 > 0){
            for(i=m-5; i<m+5; i++)
                if (an_x[i] < n_c_min){
                    n_c_min = an_x[i];
                    an_exact_ir_valley_locs[k] = i;
                    un_only_once = 0;
                }
            if (un_only_once == 0)
                n_exact_ir_valley_locs_count++;
        }
    }

    if (n_exact_ir_valley_locs_count < 2){
       *pn_spo2 = -999; 
       *pch_spo2_valid = 0; 
       return;
    }

    // SpO2 数据的 4点平滑
    for(k=0; k<BUFFER_SIZE-MA4_SIZE; k++){
        an_x[k] = (an_x[k] + an_x[k+1] + an_x[k+2] + an_x[k+3]) / 4;
        an_y[k] = (an_y[k] + an_y[k+1] + an_y[k+2] + an_y[k+3]) / 4;
    }

    // --- 10. 计算 R 值和 SpO2 ---
    n_ratio_average = 0; 
    n_i_ratio_count = 0; 
    
    for(k=0; k<5; k++) an_ratio[k] = 0;

    for (k=0; k<n_exact_ir_valley_locs_count-1; k++){
        n_y_dc_max = -16777216; 
        n_x_dc_max = -16777216; 
        
        // 检查两个波谷之间的距离，太近则不计算 SpO2
        if (an_exact_ir_valley_locs[k+1] - an_exact_ir_valley_locs[k] > 15){ // 改大一点，增强稳定性
            for (i=an_exact_ir_valley_locs[k]; i<an_exact_ir_valley_locs[k+1]; i++){
                if (an_x[i] > n_x_dc_max) {n_x_dc_max = an_x[i]; n_x_dc_max_idx = i;}
                if (an_y[i] > n_y_dc_max) {n_y_dc_max = an_y[i]; n_y_dc_max_idx = i;}
            }
            
            // AC 分量计算 (基于波谷和波峰差值)
            n_y_ac = (an_y[an_exact_ir_valley_locs[k+1]] - an_y[an_exact_ir_valley_locs[k]]) * (n_y_dc_max_idx - an_exact_ir_valley_locs[k]); 
            n_y_ac = an_y[an_exact_ir_valley_locs[k]] + n_y_ac / (an_exact_ir_valley_locs[k+1] - an_exact_ir_valley_locs[k]); 
            n_y_ac = an_y[n_y_dc_max_idx] - n_y_ac; 

            n_x_ac = (an_x[an_exact_ir_valley_locs[k+1]] - an_x[an_exact_ir_valley_locs[k]]) * (n_x_dc_max_idx - an_exact_ir_valley_locs[k]); 
            n_x_ac = an_x[an_exact_ir_valley_locs[k]] + n_x_ac / (an_exact_ir_valley_locs[k+1] - an_exact_ir_valley_locs[k]); 
            n_x_ac = an_x[n_y_dc_max_idx] - n_x_ac; 
            
            n_nume = (n_y_ac * n_x_dc_max) >> 7; 
            n_denom = (n_x_ac * n_y_dc_max) >> 7;

            // 增加对非法比值的过滤
            if (n_denom > 0 && n_i_ratio_count < 5 && n_nume != 0){   
                an_ratio[n_i_ratio_count] = (n_nume * 20) / n_denom; // 修改乘数系数来适配你的硬件，标准是*100，这里*20对应特定的查找表
                n_i_ratio_count++;
            }
        }
    }

    // 中值滤波取平均
    maxim_sort_ascend(an_ratio, n_i_ratio_count);
    n_middle_idx = n_i_ratio_count / 2;

    if (n_middle_idx > 1){
        n_ratio_average = (an_ratio[n_middle_idx-1] + an_ratio[n_middle_idx]) / 2; 
		}
    else {
        n_ratio_average = an_ratio[n_middle_idx];
		}

    if(n_ratio_average > 2 && n_ratio_average < 184){
        n_spo2_calc = uch_spo2_table[n_ratio_average];
        *pn_spo2 = n_spo2_calc;
        *pch_spo2_valid = 1;
    }
    else{
        *pn_spo2 = -999; 
        *pch_spo2_valid = 0; 
    }
}

/**
* \brief        Find peaks
* \par          Details
*               Find at most MAX_NUM peaks above MIN_HEIGHT separated by at least MIN_DISTANCE
*
* \retval       None
*/
void maxim_find_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height, int32_t n_min_distance, int32_t n_max_num)
{
    maxim_peaks_above_min_height( pn_locs, pn_npks, pn_x, n_size, n_min_height );
    maxim_remove_close_peaks( pn_locs, pn_npks, pn_x, n_min_distance );
    *pn_npks = min( *pn_npks, n_max_num );
}

/**
* \brief        Find peaks above n_min_height
* \par          Details
*               Find all peaks above MIN_HEIGHT
*
* \retval       None
*/
void maxim_peaks_above_min_height(int32_t *pn_locs, int32_t *pn_npks, int32_t  *pn_x, int32_t n_size, int32_t n_min_height)
{
    int32_t i = 1, n_width;
    *pn_npks = 0;
    
    while (i < n_size-1){
        if (pn_x[i] > n_min_height && pn_x[i] > pn_x[i-1]){            // find left edge of potential peaks
            n_width = 1;
            while (i+n_width < n_size && pn_x[i] == pn_x[i+n_width])    // find flat peaks
                n_width++;
            if (pn_x[i] > pn_x[i+n_width] && (*pn_npks) < 15 ){                            // find right edge of peaks
                pn_locs[(*pn_npks)++] = i;        
                // for flat peaks, peak location is left edge
                i += n_width+1;
            }
            else
                i += n_width;
        }
        else
            i++;
    }
}

/**
* \brief        Remove peaks
* \par          Details
*               Remove peaks separated by less than MIN_DISTANCE
*
* \retval       None
*/
void maxim_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_min_distance)

{
    
    int32_t i, j, n_old_npks, n_dist;
    
    /* Order peaks from large to small */
    maxim_sort_indices_descend( pn_x, pn_locs, *pn_npks );

    for ( i = -1; i < *pn_npks; i++ ){
        n_old_npks = *pn_npks;
        *pn_npks = i+1;
        for ( j = i+1; j < n_old_npks; j++ ){
            n_dist =  pn_locs[j] - ( i == -1 ? -1 : pn_locs[i] ); // lag-zero peak of autocorr is at index -1
            if ( n_dist > n_min_distance || n_dist < -n_min_distance )
                pn_locs[(*pn_npks)++] = pn_locs[j];
        }
    }

    // Resort indices longo ascending order
    maxim_sort_ascend( pn_locs, *pn_npks );
}

/**
* \brief        Sort array
* \par          Details
*               Sort array in ascending order (insertion sort algorithm)
*
* \retval       None
*/
void maxim_sort_ascend(int32_t *pn_x,int32_t n_size) 

{
    int32_t i, j, n_temp;
    for (i = 1; i < n_size; i++) {
        n_temp = pn_x[i];
        for (j = i; j > 0 && n_temp < pn_x[j-1]; j--)
            pn_x[j] = pn_x[j-1];
        pn_x[j] = n_temp;
    }
}

/**
* \brief        Sort indices
* \par          Details
*               Sort indices according to descending order (insertion sort algorithm)
*
* \retval       None
*/ 
void maxim_sort_indices_descend(int32_t *pn_x, int32_t *pn_indx, int32_t n_size)

{
    int32_t i, j, n_temp;
    for (i = 1; i < n_size; i++) {
        n_temp = pn_indx[i];
        for (j = i; j > 0 && pn_x[n_temp] > pn_x[pn_indx[j-1]]; j--)
            pn_indx[j] = pn_indx[j-1];
        pn_indx[j] = n_temp;
    }
}

int check_signal_quality(uint32_t *buffer, int length, uint32_t min, uint32_t max) {
    if (max - min < 1000) return 0; // 信号幅值太小，可能噪声
    // 可选：计算方差或动态范围
    return 1;
}

void moving_average_filter(uint32_t *input, uint32_t *output, int length, int window_size) {
    for (int i = 0; i < length; i++) {
        uint32_t sum = 0;
        int count = 0;
        for (int j = -window_size/2; j <= window_size/2; j++) {
            int index = (i + j + length) % length; // 处理环形缓冲区
            sum += input[index];
            count++;
        }
        output[i] = sum / count;
    }
}

uint8_t INT_max30102(uint8_t auto_clear)  // 参数auto_clear：是否自动清除标志
{
  uint8_t status = max30102_int_triggered;
    if (auto_clear && status) {
      //  max30102_int_triggered = 0;  // 只有在有中断且auto_clear为1时才清除
    }
    return status;
}



