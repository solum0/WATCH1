#include "mpu6050.h"
#include "math.h"
#include <stdlib.h>
#include "usart.h"
// 1. 传感器原始数据
static float ax, ay, az;      // g
static float gx, gy, gz;      // deg/s
static float temperature_c;
float pitch, roll, yaw;

static float accel_scale = 16384.0f;  // ±2g
static float gyro_scale = 16.4f;      // ±2000°/s

// 2. 算法相关变量
static uint8_t algo_initialized = 0;

// 均值穿越法变量
static float acc_smooth = 0.0f;      // 平滑后的加速度模长
static float acc_baseline = 0.0f;    // 动态基准线 (近似重力)
//static float acc_smooth_prev = 0.0f; // 上一次的平滑值

static float peak_amplitude_in_cycle = 0.0f; // 记录当前波峰的最大幅度
static uint8_t is_above_baseline = 0;        // 状态标志：是否处于波峰上方

// 3. 计步计数
volatile uint16_t step_numbers = 0; // 步数
static uint32_t last_step_time = 0;

// ================= 抬腕检测变量 =================
volatile uint8_t is_wrist_lifted = 0; // 全局标志位
static uint32_t wrist_check_timer = 0; // 计时器

// ================= 硬件 IIC 操作 =================

// 辅助：写寄存器
static uint8_t mpu_write_byte(uint8_t reg, uint8_t data) {
     uint8_t buf[] = {reg, data};
     return h_I2C_SendBytes(MPU6050_ADDRESS_WRITE, buf, 2);
}

// 辅助：读数据
static void mpu_read_bytes(uint8_t reg, uint8_t *buf, uint8_t len) { 
    h_I2C_ReadReceives(MPU6050_ADDRESS, reg, buf, len);
}

void mpu6050_init(void)
{
    // 复位设备
    mpu_write_byte(MPU6050_PWR_MGMT_1, 0x80); 
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // 唤醒设备
    mpu_write_byte(MPU6050_PWR_MGMT_1, 0x00); 
    
    // 陀螺仪配置: ±2000°/s
    mpu_write_byte(MPU6050_GYRO_CONFIG, 0x18);
    
    // 加速度计配置: ±2g
    mpu_write_byte(MPU6050_ACCEL_CONFIG, 0x00);
}

// 读取所有传感器数据
static void mpu6050_read_all(void)
{
    uint8_t buf[14];
    mpu_read_bytes(0x3B, buf, 14);

    int16_t ax_raw = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t ay_raw = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t az_raw = (int16_t)((buf[4] << 8) | buf[5]);
    int16_t temp_raw = (int16_t)((buf[6] << 8) | buf[7]);
    int16_t gx_raw = (int16_t)((buf[8] << 8) | buf[9]);
    int16_t gy_raw = (int16_t)((buf[10] << 8) | buf[11]);
    int16_t gz_raw = (int16_t)((buf[12] << 8) | buf[13]);
		
		//加速度计
    ax = (float)ax_raw / accel_scale;
    ay = (float)ay_raw / accel_scale; 
    az = (float)az_raw / accel_scale;

    gx = (float)gx_raw / gyro_scale;
    gy = (float)gy_raw / gyro_scale;
    gz = (float)gz_raw / gyro_scale;

    temperature_c = (float)temp_raw / 340.0f + 36.53f;
}

// ================= 新算法：均值穿越检测 =================

/**
 * @brief  计算合加速度并进行双重滤波
 * @return void
 */
static void process_signals(void)
{
    // 1. 计算合向量模长 (SVM)
    float raw_magnitude = sqrtf(ax*ax + ay*ay + az*az);
    
    // 初始化处理
    if (algo_initialized == 0) {
        acc_smooth = raw_magnitude;
        acc_baseline = raw_magnitude;
        algo_initialized = 1;
    }
    
    // 保存上一次的值用于检测交叉点
   // acc_smooth_prev = acc_smooth;

    // 2. 第一级滤波：轻度低通，去除毛刺，保留波形特征
    // Alpha = 0.8 (保留80%旧值，吸收20%新值)
    acc_smooth = 0.8f * acc_smooth + 0.2f * raw_magnitude;
    
    // 3. 第二级滤波：重度低通，提取动态基准线 (Gravity/DC Component)
    // Alpha = 0.995 (变化极慢，代表行走的平均重心)
    acc_baseline = 0.995f * acc_baseline + 0.005f * raw_magnitude;
}

/**
 * @brief  核心计步逻辑：检测下降沿穿越基准线
 */
static void detect_step_mean_crossing(void)
{
    mpu6050_data_t msg;

    uint32_t current_time = xTaskGetTickCount();
    
    // 计算当前信号相对于基准线的偏移量
    float diff = acc_smooth - acc_baseline;

    // --- 状态跟踪 ---
    
    // 如果当前在基准线上方
    if (acc_smooth > acc_baseline) {
        // 如果是从下方刚穿上来的，标记状态，重置当前周期的最大幅值
        if (!is_above_baseline) {
            is_above_baseline = 1;
            peak_amplitude_in_cycle = 0.0f;
        }
        
        // 记录这一波峰过程中的最大振幅
        if (diff > peak_amplitude_in_cycle) {
            peak_amplitude_in_cycle = diff;
        }
    }
    // 如果当前在基准线下方 (或相等)
    else {
        // 关键判断：是否发生了【下降沿穿越】(Falling Edge)
        // 条件：上一次在上方，这一次在下方
        if (is_above_baseline) {
           // printf("step++ \n");
            // 1. 幅度检查：刚才那个波峰够高吗？(由 STEP_MIN_AMPLITUDE 决定灵敏度)
            // 2. 时间检查：距离上一步时间够久吗？(去抖动)
            if (peak_amplitude_in_cycle > STEP_MIN_AMPLITUDE && 
               (current_time - last_step_time) > pdMS_TO_TICKS(STEP_MIN_INTERVAL_MS)) 
            {
                 // 确认为有效步数
							
							
                 msg.step_count = ++step_numbers;
                 
                 xQueueOverwrite(mpu6050_queue, &msg);

                // taskENTER_CRITICAL();
                // step_count++;
                // taskEXIT_CRITICAL();
                
                last_step_time = current_time;
            }
            
            // 状态翻转，标记为已回到下方
            is_above_baseline = 0;
        }
    }
}

// ================= 主处理任务 =================

void MPU6050_Proc(void)
{
    // 姿态解算参数
    const float dt = 0.005f;  // 假设任务周期 5ms
    const float RAD_TO_DEG = 57.2957795f;
    const float alpha_pose = 0.98f; 
    
    // 1. 读取硬件数据
    mpu6050_read_all();
    
    // 2. 简单的互补滤波解算姿态 (用于其他用途，不影响计步)
    float pitch_acc = atan2f(ay, az) * RAD_TO_DEG;
    float roll_acc  = atan2f(ax, az) * RAD_TO_DEG;
    
    pitch = alpha_pose * (pitch + gx * dt) + (1.0f - alpha_pose) * pitch_acc;
    roll  = alpha_pose * (roll  - gy * dt) + (1.0f - alpha_pose) * roll_acc;
    yaw  += gz * dt; // 简单的积分，会有漂移
    
    // 3. 执行计步算法
    process_signals();           // 滤波
    detect_step_mean_crossing(); // 判定
    
		// 4. 执行抬腕检测算法
     detect_wrist_gesture();
	
    // 5. 延时，保持采样率 (例如 200Hz -> 5ms)
    vTaskDelay(pdMS_TO_TICKS(5));
}

static void detect_wrist_gesture(void)
{
    // 1. 获取当前时间
    uint32_t current_time = xTaskGetTickCount();
    
    // 2. 定义判定条件
    
    // 条件A: 屏幕必须大体朝上
    // 假设芯片安装时 Z轴垂直屏幕向下或向上。
    // 如果 Z轴数据 > 0.5g (或者 < -0.5g，取决于你的安装方向)，说明屏幕大致水平
    // 这里假设正着放 Z > 0
    uint8_t cond_screen_up = (az > 0.4f); 

    // 条件B: 观察角度合适 (Pitch 和 Roll 都在 ±35度以内)
    // 只有在这个范围内，人眼看屏幕才舒服
    uint8_t cond_angle_ok = (fabsf(pitch) < WRIST_LIFT_ANGLE_LIMIT) && 
                            (fabsf(roll)  < WRIST_LIFT_ANGLE_LIMIT);

    // 3. 状态机逻辑
    if (cond_screen_up && cond_angle_ok) 
    {
        // 姿态符合！
        
        // 如果是刚进入这个姿态，初始化计时器
        if (wrist_check_timer == 0) {
            wrist_check_timer = current_time;
        }
        
        // 检查保持时间
        // 如果保持姿态超过 250ms，且当前状态是“放下(0)”，则触发“抬起(1)”
        if ((current_time - wrist_check_timer) > pdMS_TO_TICKS(WRIST_HOLD_TIME_MS)) 
        {
            if (is_wrist_lifted == 0) {
                is_wrist_lifted = 1; // 标记为抬起
									OLED_WriteCommand(0X8D);  // 设置电荷泵命令
									OLED_WriteCommand(0X14);  // 开启电荷泵
									OLED_WriteCommand(0XAF);  // 唤醒OLED显示
							
                // 【在这里执行特定任务】
                // 例如：发送信号量给显示任务，或者直接调用亮屏函数
                // Screen_TurnOn(); 
                // printf("检测到抬腕！\n");
            }
        }
    } 
    else 
    {
        // 姿态不符合 (手放下了，或者甩手了)
        wrist_check_timer = 0; // 清空计时
        
        if (is_wrist_lifted == 1) {
            is_wrist_lifted = 0; // 标记为放下
            
            // 【在这里执行放下后的任务】
            OLED_WriteCommand(0X8D);  // 设置电荷泵命令
						OLED_WriteCommand(0X10);  // 关闭电荷泵
						OLED_WriteCommand(0XAE);  // 休眠OLED显示
        }
    }
}

// ================= 外部接口 =================
float mpu6050_GetAX(void) { return ax; }
float mpu6050_GetAY(void) { return ay; }
float mpu6050_GetAZ(void) { return az; }
float mpu6050_GetTemp(void) { return temperature_c; }
