#include "bme280.h"

BME280_CalibData calib; // 全局校准数据

// --- 内部底层读写封装 ---
static uint8_t BME_Write(uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    return h_I2C_SendBytes(BME280_ADDR_Write, buf, 2);
}

static uint8_t BME_ReadLen(uint8_t reg, uint8_t *buf, uint8_t len) {
    return h_I2C_ReadReceives(BME280_ADDR, reg, buf, len);
}

// --- 读取校准数据 (芯片出厂固化数据) ---
static void BME280_Read_Calibration(void) {
    uint8_t buf[26];
    
    // 读取 Temp & Press 校准 (0x88 - 0x9F)
    BME_ReadLen(0x88, buf, 24);
    calib.dig_T1 = (buf[1] << 8) | buf[0];							//合成16进制
    calib.dig_T2 = (int16_t)((buf[3] << 8) | buf[2]);
    calib.dig_T3 = (int16_t)((buf[5] << 8) | buf[4]);
    calib.dig_P1 = (buf[7] << 8) | buf[6];
    calib.dig_P2 = (int16_t)((buf[9] << 8) | buf[8]);
    calib.dig_P3 = (int16_t)((buf[11] << 8) | buf[10]);
    calib.dig_P4 = (int16_t)((buf[13] << 8) | buf[12]);
    calib.dig_P5 = (int16_t)((buf[15] << 8) | buf[14]);
    calib.dig_P6 = (int16_t)((buf[17] << 8) | buf[16]);
    calib.dig_P7 = (int16_t)((buf[19] << 8) | buf[18]);
    calib.dig_P8 = (int16_t)((buf[21] << 8) | buf[20]);
    calib.dig_P9 = (int16_t)((buf[23] << 8) | buf[22]);

    // 读取 Hum 校准 (0xA1, 0xE1 - 0xE7)
    uint8_t h1;
    BME_ReadLen(0xA1, &h1, 1);
    calib.dig_H1 = h1;
    
    BME_ReadLen(0xE1, buf, 7);
    calib.dig_H2 = (int16_t)((buf[1] << 8) | buf[0]);
    calib.dig_H3 = buf[2];
    calib.dig_H4 = (int16_t)((buf[3] << 4) | (buf[4] & 0x0F)); //buf[4] 低四位有效
    calib.dig_H5 = (int16_t)((buf[5] << 4) | (buf[4] >> 4));  //buf[4] 取高四位
    calib.dig_H6 = (int8_t)buf[6];
}

// --- 初始化函数 ---
int BME280_Init(void) {
    // 1. 软复位
    if(BME_Write(BME280_REG_RESET, 0xB6) != 0) return 1;
    vTaskDelay(pdMS_TO_TICKS(10)); // 等待复位完成

    // 2. 读取校准参数 (必须步骤)
    BME280_Read_Calibration();

    // 3. 直接写入配置寄存器 顺序必须是: Ctrl_Hum -> Config -> Ctrl_Meas
    BME_Write(BME280_REG_CTRL_HUM, HEX_CTRL_HUM); 
    BME_Write(BME280_REG_CONFIG,   HEX_CONFIG);
    BME_Write(BME280_REG_CTRL_MEAS, 0x6c); 

    return 0; // 成功
}

// --- 读取并计算数据 ---
// 传入指针获取结果，返回0表示成功
int BME280_Read_All(float *temp, float *hum, float *press) {
		BME_Write(BME280_REG_CTRL_MEAS, HEX_CTRL_MEAS); 					//从睡眠模式恢复
	
    uint8_t data[8];
    // 一次性读取从 0xF7 开始的8个字节 (Press_MSB 到 Hum_LSB)
    if(BME_ReadLen(BME280_REG_PRESS_MSB, data, 8) != 0) return 1;

    // 1. 组合原始 ADC 值
    int32_t adc_p = (int32_t)((data[0] << 12) | (data[1] << 4) | (data[2] >> 4));
    int32_t adc_t = (int32_t)((data[3] << 12) | (data[4] << 4) | (data[5] >> 4));
    int32_t adc_h = (int32_t)((data[6] << 8) | data[7]);

    // 2. 温度补偿 (必须最先计算，因为t_fine用于后续补偿)
    double var1, var2, T;
    var1 = (((double)adc_t) / 16384.0 - ((double)calib.dig_T1) / 1024.0) * ((double)calib.dig_T2);
    var2 = ((((double)adc_t) / 131072.0 - ((double)calib.dig_T1) / 8192.0) *
            (((double)adc_t) / 131072.0 - ((double)calib.dig_T1) / 8192.0)) * ((double)calib.dig_T3);
    calib.t_fine = (int32_t)(var1 + var2);
    T = (var1 + var2) / 5120.0;
    *temp = (float)T;

    // 3. 气压补偿
    double p;
    var1 = ((double)calib.t_fine / 2.0) - 64000.0;
    var2 = var1 * var1 * ((double)calib.dig_P6) / 32768.0;
    var2 = var2 + var1 * ((double)calib.dig_P5) * 2.0;
    var2 = (var2 / 4.0) + (((double)calib.dig_P4) * 65536.0);
    var1 = (((double)calib.dig_P3) * var1 * var1 / 524288.0 + ((double)calib.dig_P2) * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * ((double)calib.dig_P1);
    
    if (var1 == 0.0) {
        *press = 0; // 避免除以0
    } else {
        p = 1048576.0 - (double)adc_p;
        p = (p - (var2 / 4096.0)) * 6250.0 / var1;
        var1 = ((double)calib.dig_P9) * p * p / 2147483648.0;
        var2 = p * ((double)calib.dig_P8) / 32768.0;
        p = p + (var1 + var2 + ((double)calib.dig_P7)) / 16.0;
        *press = (float)p; // 单位 Pa
    }

    // 4. 湿度补偿
    double h;
    h = (((double)calib.t_fine) - 76800.0);
    h = (adc_h - (((double)calib.dig_H4) * 64.0 + ((double)calib.dig_H5) / 16384.0 * h)) *
        (((double)calib.dig_H2) / 65536.0 * (1.0 + ((double)calib.dig_H6) / 67108864.0 * h *
        (1.0 + ((double)calib.dig_H3) / 67108864.0 * h)));
    h = h * (1.0 - ((double)calib.dig_H1) * h / 524288.0);
    if (h > 100.0) h = 100.0;
    else if (h < 0.0) h = 0.0;
    *hum = (float)h;

    return 0;
}

//关闭BME280传感器电源
uint8_t BME_POWER_OFF(void)
{
	uint8_t reg_val;
	reg_val = (HEX_CTRL_MEAS &0xfc );
	if(BME_Write(BME280_REG_CTRL_MEAS, reg_val) != 0) return 1;
	return 0;
}

