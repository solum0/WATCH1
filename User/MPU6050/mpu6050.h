#ifndef MPU6050_H
#define MPU6050_H 

#include "stm32f4xx.h" 
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "hi2c.h"
#include <stdbool.h>

//创建全局消息队列
extern QueueHandle_t mpu6050_queue; 

typedef struct{
    uint16_t step_count;
    bool wrist_lifted;

} mpu6050_data_t;

// ================= 硬件配置 =================
#define MPU6050_ADDRESS                 0x69
#define MPU6050_ADDRESS_WRITE           0xd2
#define MPU6050_ADDRESS_READ            0xd3

#define MPU6050_PWR_MGMT_1              0x6B
#define MPU6050_GYRO_CONFIG             0x1B  
#define MPU6050_ACCEL_CONFIG            0x1C 

// ================= 计步算法参数调节 =================
// 最小步频间隔 (ms)，人走得再快也不太可能小于250ms
#define STEP_MIN_INTERVAL_MS            300   

// 最小震动幅度阈值 (g)
// 值越小越灵敏，但更容易误判；值越大越抗噪，但可能漏记轻微的步伐
#define STEP_MIN_AMPLITUDE              0.13f 

// ================= 抬腕检测参数 =================
// 判定为“看表姿态”的角度阈值 (单位：度)
// 假设板子平放时 Z轴向下，Pitch/Roll 为 0
// 判定角度阈值 (度)
#define WRIST_LIFT_ANGLE_LIMIT  35.0f   // 允许倾斜 ±35度
#define WRIST_HOLD_TIME_MS      250     // 姿态保持 250ms 才确认（防抖）

// ================= 全局变量 =================
// 0: 手放下/无效, 1: 抬腕有效 (外部任务检测到为1时执行特定任务)
extern volatile uint8_t is_wrist_lifted;


// ================= 全局变量 =================

extern float pitch, roll, yaw;       // 姿态角

// ================= 函数声明 =================
void mpu6050_init(void);
void MPU6050_Proc(void); // 在FreeRTOS任务中调用此函数

//抬腕算法
void MPU6050_SetWakeLock(uint8_t locked);
void detect_wrist_gesture(void);

// 数据获取接口
float mpu6050_GetAX(void);
float mpu6050_GetAY(void);
float mpu6050_GetAZ(void);
float mpu6050_GetTemp(void);

#endif
