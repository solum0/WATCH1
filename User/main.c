/*
 * Key1: switch display mode.
 * Key2: time setting mode.
 * Key3: alarm setting mode.
 * Key4: alarm enable switch.
 */

#include "stdlib.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f4xx.h"
#include "usart.h"                      // 串口通信
#include "oled.h"                       // OLED显示
#include "ds3231.h"                     // DS3231实时时钟
#include "oled_clock.h"                 // OLED时钟显示
#include "key.h"                        // 按键处理
#include "motor.h"                       // 震动马达控制
#include "timer.h"                      // 定时器初始化 and 秒表
#include "bme280.h"											//检测温度，湿度
#include "math.h"
#include "EM7028/EM7028.h"
#include "max30102.h"
#include "mpu6050.h"										//陀螺仪
#include "hi2c.h"
#include "ADC_battery.h"								//监测电池电压
#include "boot_confirm.h"
#include "ota_update.h"
#include "MCU_Debug/MCU_Debug.h"

//freertos
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "timers.h"
#include "event_groups.h"

typedef enum {
    KEY_CMD_MODE_CHANGE = 1,
    KEY_CMD_TIME_SET,
    KEY_CMD_ALARM_SET,
    KEY_CMD_SETTING_KEY,
    TIMER_CMD_BLINK
} status_setting;

typedef struct {
    status_setting command;
    uint8_t data;
} TaskMessage_t;

typedef enum {
    STOPWATCH_CMD_START_PAUSE = 1,
    STOPWATCH_CMD_RESET
} StopwatchCommand_t;

typedef struct {
    StopwatchCommand_t command;
    uint8_t data;
} StopwatchMessage_t;

/* External timer and sensor symbols. */
extern volatile uint8_t start_flag;
extern volatile uint8_t zero_flag;
extern volatile uint8_t m;
extern volatile uint8_t s;
extern volatile uint8_t ms;

extern void show_timer(void);
extern void timer_loop(void);
extern void enableClock(void);
extern void disableClock(void);

/* UI, key, alarm, and setting handlers. */
void handleKeyPress(uint8_t keyNum);
void handleTimeSettingKey(uint8_t keyNum, uint32_t *inactivityTimer);
void handleAlarmSettingKey(uint8_t keyNum, uint32_t *inactivityTimer);
void incrementTimeValue(void);
void decrementTimeValue(void);
void incrementAlarmValue(void);
void decrementAlarmValue(void);
void updateTimeDisplay(uint8_t blinkOn);
void updateAlarmDisplay(uint8_t blinkOn);
void saveTimeSettings(void);
void saveAlarmSettings(void);
void vAlarmBlinkTimerCallback(TimerHandle_t xTimer);
void debounce_timer_callback(TimerHandle_t xTimer);
void alarm_stop_timer_callback(TimerHandle_t xTimer);

/* Application service functions. */
void sensor_Hander(void);
void bell_switch(void);
void main_timer_control(void);
void battery_update(void);
static uint32_t battery_filter_capacity(uint32_t capacity);
void Motor_alarm(void);
void stopwatch(void);
void LED_Switch(BitAction state);
void LED_init(void);
uint8_t isLeapYear(int year);
uint8_t getMaxDaysOfMonth(int year, int month);
static void App_ProcessBleByte(uint8_t BLE_data);
static void App_DrainUsart2Dma(void);
static void App_SetEventBits(EventBits_t bits);
static void App_ClearEventBits(EventBits_t bits);
static uint8_t App_EventBitsSet(EventBits_t bits);
static uint8_t App_OtaUiActive(void);
static uint8_t App_InputLocked(void);
static uint8_t App_CanUseOled(void);
static uint8_t App_BackgroundWorkPaused(void);
static void App_NotifyOtaDisplay(void);
static void App_SetOtaUiActive(uint8_t active);
static void App_OnOtaStatusChanged(const ota_update_status_t *status);
static const char *App_OtaErrorText(uint8_t status);
static void App_OtaLogReset(char lines[][17], uint8_t *line_count);
static void App_OtaLogAppend(char lines[][17], uint8_t *line_count, const char *line);
static void App_OtaRenderLog(char lines[][17]);
static uint8_t App_OtaAppendStatusLines(char lines[][17], uint8_t *line_count,
                                        const ota_update_status_t *status,
                                        ota_update_ui_state_t *last_state,
                                        uint8_t *last_percent);

/* Application state. */
static uint8_t g_app_update_confirmed = 0U;

uint8_t mode = 1;
DateTime calendar;
uint8_t last_second = 0;
bme280_show bme280_data;

uint16_t time_data[7] = {0};
uint8_t bell_data[4] = {0};
uint8_t bell_onoff = 0;

uint8_t set_run = 0;
uint8_t set_bell = 0;
uint8_t set_time = 0;
uint8_t set_shift = 0;
uint8_t set_shift_bell = 0;

static char ble_time_buf[20];
static uint8_t ble_time_index = 0;
static uint8_t ble_time_receiving = 0;

typedef enum {
    BLE_STATE_IDLE,
    BLE_STATE_TIME_SET,
    BLE_STATE_ALARM_SET
} BleState_t;

static BleState_t bleState = BLE_STATE_IDLE;
static uint16_t usart2_dma_read_index = 0U;
static volatile uint8_t g_task_key_ready = 0U;
static volatile uint8_t g_oled_ready = 0U;

volatile uint8_t usart2_receive_key = 0;

#pragma diag_suppress 177
/* Heart-rate and SpO2 algorithm outputs. */
int8_t ch_hr_valid;
int32_t n_heart_rate;
int8_t ch_spo2_valid;
int32_t n_sp02;
uint8_t temp[6];
#pragma diag_default 177

/* FreeRTOS synchronization, queues, and timers. */
SemaphoreHandle_t xGlobalMutex;
SemaphoreHandle_t sem_init;
SemaphoreHandle_t xAlarmSemaphore;
SemaphoreHandle_t xI2CMutex;
SemaphoreHandle_t xStepSemaphore;

QueueHandle_t key_mode_queue;
QueueHandle_t key_alarm_queue;
QueueHandle_t key_timeMode_queue;
QueueHandle_t key_stopwatch_queue;
QueueHandle_t step_queue;
QueueHandle_t mpu6050_queue;
QueueSetHandle_t time_alarm_queue_set;

TimerHandle_t xBlinkTimer;
static TimerHandle_t alarm_stop_timer = NULL;
static TimerHandle_t debounce_timer = NULL;

#define QUEUE_LENGTH_1       20
#define APP_EVENT_OTA_UI_ACTIVE       ((EventBits_t)(1UL << 0))
#define APP_EVENT_INPUT_LOCKED        ((EventBits_t)(1UL << 1))
#define APP_EVENT_OLED_RESERVED       ((EventBits_t)(1UL << 2))
#define APP_EVENT_BACKGROUND_PAUSED   ((EventBits_t)(1UL << 3))
#define APP_EVENT_OTA_STATUS_CHANGED  ((EventBits_t)(1UL << 4))
#define APP_EVENT_OTA_RUNTIME_BITS    (APP_EVENT_OTA_UI_ACTIVE | APP_EVENT_INPUT_LOCKED | \
                                       APP_EVENT_OLED_RESERVED | APP_EVENT_BACKGROUND_PAUSED)

static EventGroupHandle_t app_event_group = NULL;
static volatile EventBits_t g_app_event_bits = 0U;

/* FreeRTOS task configuration. */

// Task 1: init
#define TASK1_STACK 128
#define TASK1_PRIORITY 8
TaskHandle_t task_init_handle;
void task_init(void *pvParameters);

// Task 2: key manager
#define TASK2_STACK 300
#define TASK2_PRIORITY 7
TaskHandle_t task_key_handle;
void task_key(void *pvParameters);

// Task 3: time display
#define TASK3_STACK 300
#define TASK3_PRIORITY 5
TaskHandle_t task_time_handle;
void task_time(void *pvParameters);

// Task 4: time/alarm setting
#define TASK4_STACK 400
#define TASK4_PRIORITY 6
TaskHandle_t SetTime_handle;
void SetTimeAlarm_Hander(void *pvParameters);

// Task 5: battery display and alarm ringing
#define TASK6_STACK 300
#define TASK6_PRIORITY 5
TaskHandle_t task_ring_battery_handle;
void task_ring_battery(void *pvParameters);

// Task 6: step detection and wrist wake
#define TASK_STEP_STACK 300
#define TASK_STEP_PRIORITY 6
TaskHandle_t task_step_handle;
void task_step_detect(void *pvParameters);

// Task 7: OTA forced display
#define TASK_OTA_DISPLAY_STACK 512
#define TASK_OTA_DISPLAY_PRIORITY 8
#define TASK_OTA_DISPLAY_POLL_MS 100U
#define TASK_OTA_DISPLAY_REFRESH_MS 800U
TaskHandle_t task_ota_display_handle;
void task_ota_display(void *pvParameters);

void FreeRTOS_Start(void)
{
		// 创建互斥锁和队列
        sem_init = xSemaphoreCreateBinary();  //初始化任务信号量
        step_queue = xQueueCreate(1, sizeof(uint32_t)); //步数队列
        xGlobalMutex = xSemaphoreCreateMutex();

		xI2CMutex = xSemaphoreCreateMutex();

		key_alarm_queue = xQueueCreate(10, sizeof(TaskMessage_t)); // 创建闹钟按键队列

		key_mode_queue = xQueueCreate(20, sizeof(TaskMessage_t));  //模式切换队列

		key_timeMode_queue = xQueueCreate(10, sizeof(TaskMessage_t));  //时间任务队列

		key_stopwatch_queue = xQueueCreate(10, sizeof(StopwatchMessage_t)); //秒表模式队列

        mpu6050_queue = xQueueCreate(10,sizeof(mpu6050_data_t));

		xStepSemaphore = xSemaphoreCreateBinary();

		xAlarmSemaphore = xSemaphoreCreateBinary();

        app_event_group = xEventGroupCreate();
        Ota_UpdateRegisterStatusCallback(App_OnOtaStatusChanged);
        App_SetOtaUiActive(Ota_UpdateDisplayActive());

        time_alarm_queue_set = xQueueCreateSet(QUEUE_LENGTH_1);

        xQueueAddToSet(key_timeMode_queue, time_alarm_queue_set);
        xQueueAddToSet(key_alarm_queue, time_alarm_queue_set);

		xTaskCreate(task_init, "task_init", TASK1_STACK, NULL, TASK1_PRIORITY, &task_init_handle);
        xTaskCreate(task_key, "task_key", TASK2_STACK, NULL, TASK2_PRIORITY, &task_key_handle);
        xTaskCreate(task_time, "task_time", TASK3_STACK, NULL, TASK3_PRIORITY, &task_time_handle);
        xTaskCreate(SetTimeAlarm_Hander, "SetTime_Alarm", TASK4_STACK, NULL, TASK4_PRIORITY, &SetTime_handle);
        xTaskCreate(task_ring_battery, "ring_battery", TASK6_STACK, NULL, TASK6_PRIORITY, &task_ring_battery_handle);
	    xTaskCreate(task_step_detect, "StepDetect", TASK_STEP_STACK, NULL, TASK_STEP_PRIORITY, &task_step_handle);
        xTaskCreate(task_ota_display, "OTA_Display", TASK_OTA_DISPLAY_STACK, NULL, TASK_OTA_DISPLAY_PRIORITY, &task_ota_display_handle);
//		xTaskCreate(task_stopwatch, "stopwatch", TASK6_STACK, NULL, TASK6_PRIORITY, &task_stopwatch_handle);

//		xTaskCreate(task_beep_alarm,"AlarmBeep",TASK_beep_STACK,NULL,TASK_beep_PRIORITY,&task_beep_handle);
//
//		xTaskCreate(task_battery_showm,"battery_show",TASK_battery_STACK,NULL,TASK_battery_PRIORITY,&task_battery_handle);

	/* Start scheduler. */
	vTaskStartScheduler();
}

//启动任务：用于创建其他Task
void task_init(void *pvParameters)
{
    hi2c_init();                                        //初始化硬件I2C
    vTaskDelay(pdMS_TO_TICKS(30));                      // 短暂延时确保硬件稳定

    TIM2_Int_Init();                                    // 初始化TIM2
    TIM3_Int_Init();                                    // 初始化TIM3
 //   BEEP_GPIO_ON();
    mpu6050_init();                                     //初始化mpu6050
	BME280_Init();
	//	MAX30102_Init();																		//初始化心率传感器

 //

	// printf("串口测试233\n"); 	                        //测试串口打印

    OLED_Init();
    vTaskDelay(pdMS_TO_TICKS(30));

    OLED_Clear();                                       // 再次清空屏幕

	vTaskDelay(pdMS_TO_TICKS(30));

    g_oled_ready = 1U;
    App_NotifyOtaDisplay();



    Key_Init();                                         // 初始化按键GPIO
	Motor_GPIO_Init();
 //  GPIO_SetBits(GPIOA, GPIO_Pin_8);
    ADC_Start();                                        // 开启ADC采集引脚
    ADC_Battery_Init();						            // 初始化ADC

	LED_init();
	LED_Switch(Bit_SET);

//
//  	RCC_ClocksTypeDef get_rcc_clock;
//  	RCC_GetClocksFreq(&get_rcc_clock);

//  	uint32_t PCLK2 = get_rcc_clock.PCLK2_Frequency;
//  	uint32_t PCLK1 = get_rcc_clock.PCLK1_Frequency;
//  	uint32_t HCLK = get_rcc_clock.HCLK_Frequency;
//  	uint32_t SYSCLK = get_rcc_clock.SYSCLK_Frequency;


//  	printf("PCLK2: %lu Hz\n", PCLK2);
//  	printf("PCLK1: %lu Hz\n", PCLK1);
//  	printf("HCLK: %lu Hz\n", HCLK);
//  	printf("SYSCLK: %lu Hz\n", SYSCLK);
//  if (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET) {
//     printf("HSE Failed!\r\n");
//  }

//  	uint8_t sws_status = (RCC->CFGR & RCC_CFGR_SWS) >> 2;
//     printf("System Clock Source (SWS): %d (0=HSI, 1=HSE, 2=PLL)\r\n", sws_status);

//  // 2. 判断 PLL 是否就绪
//  if (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET) {
//     printf("PLL Failed! (PLL Not Ready)\r\n");
//  } else {
//     printf("PLL Ready.\r\n");
//  }

//  // 开启LSE
// RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
// PWR_BackupAccessCmd(ENABLE);
// RCC_LSEConfig(RCC_LSE_ON);

// // 等待就绪
// uint32_t LSE_TimeOut = 0;
// while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET && LSE_TimeOut < 0xFFFF) {
//     LSE_TimeOut++;
// }

// if (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET) {
//     printf("LSE Failed!\r\n");
// } else {
//     printf("LSE Ready.\r\n");
// }

	xSemaphoreGive(sem_init);

    vTaskDelay(pdMS_TO_TICKS(20));


    if ((g_app_update_confirmed != 0U) && (App_CanUseOled() != 0U)) {             /* 等于1升级成功 */
//        OLED_ShowString(40,4,"OTA Y",OLED_6X8);
//        OLED_Update();
    //    vTaskDelay(pdMS_TO_TICKS(1500));
    }
		else if (App_CanUseOled() != 0U) {
//		 OLED_ShowString(40,4,"OTA N",OLED_6X8);
//        OLED_Update();
		}
		g_app_update_confirmed = AppBoot_ConfirmIfTrial();	// 返回升级结果
  	vTaskDelete(NULL);                                  //删除任务
}


//
 void debounce_timer_callback(TimerHandle_t xTimer){

    uint8_t current_state = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4);
			printf("%d",current_state);           // debug test
        if(current_state == RESET){
				//
				GPIO_WriteBit(GPIOB, GPIO_Pin_13, Bit_RESET);
//				GPIO_WriteBit(GPIOB, GPIO_Pin_12, Bit_SET);
        }
        else {
                //短按逻辑
                }
 }

// EXTI4中断 - 仅触发定时器 - 开关机
void EXTI4_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (EXTI_GetITStatus(EXTI_Line4) != RESET) {
			//
        xTimerResetFromISR(debounce_timer, &xHigherPriorityTaskWoken);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
				EXTI_ClearITPendingBit(EXTI_Line4);
    }
}

// //wk-up
// void EXTI0_IRQHandler(void)
// {
// 		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//    if (EXTI_GetITStatus(EXTI_Line0) != RESET)       // 检查EXTI1中断是否发生
//    {
// 				xTimerResetFromISR(Sleep_switch,&xHigherPriorityTaskWoken);
// 				portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
// 				//唤醒
// 				 if(PWR_GetFlagStatus(PWR_FLAG_SB) != RESET)
// 					{
// 							PWR_ClearFlag(PWR_FLAG_SB);
// 					}
//
//    }
// }

void task_key(void *pvParameters)
{
	//
		// debounce_timer = xTimerCreate(
        //             "KEYTimer",
        //             pdMS_TO_TICKS(400),	//三秒时间
        //             pdTRUE,
        //             (void *)0,
        //             debounce_timer_callback
		// 								);
    //
	alarm_stop_timer = xTimerCreate(
                    "RingStop",
                    pdMS_TO_TICKS(3000), //三秒时间
                    pdTRUE,
                    (void *)0,
                   alarm_stop_timer_callback
                                        );

	//
	// Sleep_switch = xTimerCreate(
	// 								"SLEEPTimer",
	//
	// 								pdTRUE,
	// 								(void *)0,
	// 								sleep_timer_callback
	// 								);

    uint8_t keyNum = 0;

    g_task_key_ready = 1U;

    while(1){
        App_DrainUsart2Dma();
        Ota_UpdatePoll();

        keyNum = GetKeyNum();
        if (keyNum != 0U) {
            vTaskDelay(pdMS_TO_TICKS(20));
            if (App_InputLocked() == 0U) {
                handleKeyPress(keyNum);
            }
        }

     ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20)); // 20ms扫描周期
	}
}

// 按键按下处理函数
static void App_DrainUsart2Dma(void)
{
    uint8_t rx_data[64];
    uint16_t count;
    uint16_t i;
    uint16_t consumed;

    do {
        count = Usart_RxDmaRead(&usart2_dma_read_index, rx_data, (uint16_t)sizeof(rx_data));
        i = 0U;
        while (i < count) {
            if ((Ota_UpdateInProgress() != 0U) ||
                ((bleState == BLE_STATE_IDLE) && (ble_time_receiving == 0U))) {
                consumed = Ota_UpdateFeedBuffer(&rx_data[i], (uint16_t)(count - i));
                if (consumed != 0U) {
                    i = (uint16_t)(i + consumed);
                    continue;
                }
            }

            if (App_InputLocked() == 0U) {
                App_ProcessBleByte(rx_data[i]);
            }
            ++i;
        }
    } while (count == sizeof(rx_data));
}

static void App_SetEventBits(EventBits_t bits)
{
    g_app_event_bits |= bits;
    if (app_event_group != NULL) {
        (void)xEventGroupSetBits(app_event_group, bits);
    }
}

static void App_ClearEventBits(EventBits_t bits)
{
    g_app_event_bits &= ~bits;
    if (app_event_group != NULL) {
        (void)xEventGroupClearBits(app_event_group, bits);
    }
}

static uint8_t App_EventBitsSet(EventBits_t bits)
{
    return (uint8_t)(((g_app_event_bits & bits) != 0U) ? 1U : 0U);
}

static uint8_t App_OtaUiActive(void)
{
    return App_EventBitsSet(APP_EVENT_OTA_UI_ACTIVE);
}

static uint8_t App_InputLocked(void)
{
    return App_EventBitsSet(APP_EVENT_INPUT_LOCKED);
}

static uint8_t App_CanUseOled(void)
{
    return (uint8_t)(((g_oled_ready != 0U) &&
                      (App_EventBitsSet(APP_EVENT_OLED_RESERVED) == 0U)) ? 1U : 0U);
}

static uint8_t App_BackgroundWorkPaused(void)
{
    return App_EventBitsSet(APP_EVENT_BACKGROUND_PAUSED);
}

static void App_NotifyOtaDisplay(void)
{
    if (task_ota_display_handle != NULL) {
        xTaskNotifyGive(task_ota_display_handle);
    }
}

static void App_SetOtaUiActive(uint8_t active)
{
    if (active != 0U) {
        App_SetEventBits(APP_EVENT_OTA_RUNTIME_BITS | APP_EVENT_OTA_STATUS_CHANGED);
    } else {
        App_ClearEventBits(APP_EVENT_OTA_RUNTIME_BITS);
        App_SetEventBits(APP_EVENT_OTA_STATUS_CHANGED);
    }

    Key_SetSleepLock(active);
    MPU6050_SetWakeLock(active);
}

static void App_OnOtaStatusChanged(const ota_update_status_t *status)
{
    static ota_update_ui_state_t last_ui_state = OTA_UPDATE_UI_IDLE;
    uint8_t was_active;
    uint8_t active;

    if (status == NULL) {
        return;
    }

    was_active = App_OtaUiActive();
    active = (status->ui_state != OTA_UPDATE_UI_IDLE) ? 1U : 0U;
    App_SetOtaUiActive(active);

    if ((active != was_active) ||
        (status->ui_state != last_ui_state) ||
        (status->ui_state != OTA_UPDATE_UI_RECEIVING)) {
        App_NotifyOtaDisplay();
    }

    last_ui_state = status->ui_state;
}

static const char *App_OtaErrorText(uint8_t status)
{
    switch (status) {
    case OTA_UPDATE_STATUS_BAD_MAGIC:
        return "BAD MAGIC";
    case OTA_UPDATE_STATUS_BAD_SIZE:
        return "BAD SIZE";
    case OTA_UPDATE_STATUS_BAD_SEQUENCE:
        return "BAD SEQ";
    case OTA_UPDATE_STATUS_BAD_OFFSET:
        return "BAD OFFSET";
    case OTA_UPDATE_STATUS_BAD_PACKET_CRC:
        return "PKT CRC";
    case OTA_UPDATE_STATUS_FLASH_FAILED:
        return "FLASH FAIL";
    case OTA_UPDATE_STATUS_IMAGE_CRC_FAILED:
        return "IMG CRC";
    case OTA_UPDATE_STATUS_VERSION_REJECTED:
        return "VERSION OLD";
    case OTA_UPDATE_STATUS_RX_TIMEOUT:
        return "RX TIMEOUT";
    default:
        return "ERROR";
    }
}

static void App_OtaLogReset(char lines[][17], uint8_t *line_count)
{
    uint8_t i;

    for (i = 0U; i < 4U; ++i) {
        memset(lines[i], 0, 17U);
    }

    *line_count = 0U;
}

static void App_OtaLogAppend(char lines[][17], uint8_t *line_count, const char *line)
{
    uint8_t i;
    uint8_t dst;

    if (*line_count < 4U) {
        dst = *line_count;
        *line_count = (uint8_t)(*line_count + 1U);
    } else {
        for (i = 0U; i < 3U; ++i) {
            memcpy(lines[i], lines[i + 1U], 17U);
        }
        dst = 3U;
    }

    memset(lines[dst], 0, 17U);
    strncpy(lines[dst], line, 16U);
    lines[dst][16] = '\0';
}

static void App_OtaRenderLog(char lines[][17])
{
    uint8_t i;

    OLED_WriteCommand(0x8D);
    OLED_WriteCommand(0x14);
    OLED_WriteCommand(0xAF);
    OLED_Clear();
    for (i = 0U; i < 4U; ++i) {
        if (lines[i][0] != '\0') {
            OLED_ShowString(0, (int16_t)(i * 16U), lines[i], OLED_8X16);
        }
    }
    OLED_Update();
}

static uint8_t App_OtaAppendStatusLines(char lines[][17], uint8_t *line_count,
                                        const ota_update_status_t *status,
                                        ota_update_ui_state_t *last_state,
                                        uint8_t *last_percent)
{
    char line[17];
    uint8_t changed = 0U;
    uint8_t percent = status->percent;

    if ((status->ui_state == *last_state) &&
        ((status->ui_state != OTA_UPDATE_UI_RECEIVING) || (percent == *last_percent))) {
        return 0U;
    }

    App_OtaLogReset(lines, line_count);

    switch (status->ui_state) {
    case OTA_UPDATE_UI_RECEIVING:
        App_OtaLogAppend(lines, line_count, "OTA RUNNING");
        sprintf(line, "RX %03u%%", (unsigned int)percent);
        App_OtaLogAppend(lines, line_count, line);
        sprintf(line, "SIZE %luK", (unsigned long)((status->image_size + 1023U) / 1024U));
        App_OtaLogAppend(lines, line_count, line);
        App_OtaLogAppend(lines, line_count, "DO NOT POWER");
        *last_percent = percent;
        changed = 1U;
        break;
    case OTA_UPDATE_UI_VERIFYING:
        App_OtaLogAppend(lines, line_count, "OTA RUNNING");
        App_OtaLogAppend(lines, line_count, "RX 100%");
        App_OtaLogAppend(lines, line_count, "VERIFYING");
        App_OtaLogAppend(lines, line_count, "DO NOT POWER");
        *last_percent = 100U;
        changed = 1U;
        break;
    case OTA_UPDATE_UI_REBOOTING:
        App_OtaLogAppend(lines, line_count, "OTA DONE");
        App_OtaLogAppend(lines, line_count, "VERIFY OK");
        App_OtaLogAppend(lines, line_count, "REBOOTING");
        App_OtaLogAppend(lines, line_count, "DO NOT POWER");
        *last_percent = 100U;
        changed = 1U;
        break;
    case OTA_UPDATE_UI_FAILED:
        App_OtaLogAppend(lines, line_count, "OTA FAILED");
        App_OtaLogAppend(lines, line_count, App_OtaErrorText(status->error_status));
        *last_percent = 0xFFU;
        changed = 1U;
        break;
    default:
        *last_percent = 0xFFU;
        changed = 1U;
        break;
    }

    *last_state = status->ui_state;

    return changed;
}

void task_ota_display(void *pvParameters)
{
    char lines[4][17];
    uint8_t line_count = 0U;
    uint8_t was_active = 0U;
    uint8_t pending_render = 0U;
    uint8_t last_percent = 0xFFU;
    ota_update_ui_state_t last_state = OTA_UPDATE_UI_IDLE;
    ota_update_status_t status;
    TickType_t last_render_tick = 0U;
    TickType_t now_tick;

    (void)pvParameters;
    App_OtaLogReset(lines, &line_count);

    while (1) {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TASK_OTA_DISPLAY_POLL_MS));
        App_ClearEventBits(APP_EVENT_OTA_STATUS_CHANGED);
        now_tick = xTaskGetTickCount();

        if (App_OtaUiActive() == 0U) {
            if (was_active != 0U) {
                App_OtaLogReset(lines, &line_count);
                last_state = OTA_UPDATE_UI_IDLE;
                last_percent = 0xFFU;
                pending_render = 0U;
                was_active = 0U;
                last_render_tick = 0U;
            }
            continue;
        }

        Ota_UpdateGetStatus(&status);
        if (was_active == 0U) {
            App_OtaLogReset(lines, &line_count);
            last_state = OTA_UPDATE_UI_IDLE;
            last_percent = 0xFFU;
            was_active = 1U;
        }

        if (App_OtaAppendStatusLines(lines, &line_count, &status, &last_state, &last_percent) != 0U) {
            pending_render = 1U;
        }

        if ((pending_render == 0U) &&
            (last_render_tick != 0U) &&
            ((now_tick - last_render_tick) >= pdMS_TO_TICKS(TASK_OTA_DISPLAY_REFRESH_MS))) {
            pending_render = 1U;
        }

        if ((pending_render != 0U) && (g_oled_ready == 0U)) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if ((pending_render != 0U) &&
            (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(20)) == pdTRUE)) {
            App_OtaRenderLog(lines);
            xSemaphoreGive(xI2CMutex);
            last_render_tick = xTaskGetTickCount();
            pending_render = 0U;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void App_ProcessBleByte(uint8_t BLE_data)
{
    if (BLE_data == 'T' || BLE_data == 'A') {
        ble_time_receiving = 1U;
        ble_time_index = 0U;
        return;
    }

    if (ble_time_receiving != 0U) {
        if (BLE_data == '\r') {
            return;
        }

        if (BLE_data == '\n') {
            ble_time_buf[ble_time_index] = '\0';

            if (ble_time_index == 19U) {
                uint8_t ad[6];

                if (parse_app_time(ble_time_buf, ad)) {
                    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        DS3231_SetDateTime(ad);
                        xSemaphoreGive(xI2CMutex);
                    }
                }
            }

            if ((ble_time_index == 3U) || (ble_time_index == 5U)) {
                uint8_t add[2];

                if (parse_app_alarm(ble_time_buf, ble_time_index, add)) {
                    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        DS3231_SetAlarm2Daily(add[1], add[0]);
                        xSemaphoreGive(xI2CMutex);
                    }
                }
            }

            ble_time_index = 0U;
            ble_time_receiving = 0U;
            return;
        }

        if (ble_time_index < (sizeof(ble_time_buf) - 1U)) {
            ble_time_buf[ble_time_index++] = BLE_data;
        } else {
            ble_time_index = 0U;
            ble_time_receiving = 0U;
        }

        return;
    }

    switch (BLE_data) {
    case 0x05:
        if (bleState == BLE_STATE_IDLE) {
            bleState = BLE_STATE_TIME_SET;
            handleKeyPress(1);
        } else if (bleState == BLE_STATE_TIME_SET) {
            bleState = BLE_STATE_IDLE;
            handleKeyPress(1);
        }
        break;

    case 0x06:
        if (bleState == BLE_STATE_IDLE) {
            bleState = BLE_STATE_ALARM_SET;
            handleKeyPress(2);
        } else if (bleState == BLE_STATE_ALARM_SET) {
            bleState = BLE_STATE_IDLE;
            handleKeyPress(2);
        }
        break;

    case 0x03:
        handleKeyPress(9);
        break;

    default:
        if ((bleState == BLE_STATE_TIME_SET) || (bleState == BLE_STATE_ALARM_SET)) {
            handleKeyPress(BLE_data);
        }
        break;
    }
}

void handleKeyPress(uint8_t keyNum)
{

    TaskMessage_t msg;
    uint8_t current_set_run, current_set_time, current_set_bell, current_mode;

    if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        current_set_run = set_run;
        current_set_time = set_time;
        current_set_bell = set_bell;		//闹钟模式
        current_mode = mode; 						//传感器画面标志位
        xSemaphoreGive(xGlobalMutex);
    } else {
		current_set_run = set_run;
        current_set_time = set_time;
        current_set_bell = set_bell;
		current_mode = mode;
    }
    // 正常模式下的按键处理
    if (current_set_run == 0) {

        if (current_mode == 3) { // 秒表模式
            StopwatchMessage_t stopwatch_msg;

            switch (keyNum) {
                case 9:
					{
                        static TickType_t last_button_time = 0;
                        static uint8_t key2_resetwatch = 0;
                        static uint8_t key2_stopwatch = 0;
                        TickType_t current_time = xTaskGetTickCount();

                        //
                        if(key2_resetwatch == 0){
                            last_button_time = current_time;

                            key2_resetwatch = 1;


                            stopwatch_msg.command = STOPWATCH_CMD_START_PAUSE;
                            stopwatch_msg.data = 0;
                            xQueueSend(key_stopwatch_queue, &stopwatch_msg, 10);
                        }
                        //
                        else if ( key2_resetwatch == 1)
                        {
                                    TickType_t time_diff = current_time - last_button_time;

                                key2_resetwatch = 0;

                            //如果间隔小于1秒，复位
                            if( time_diff < pdMS_TO_TICKS(500) ) {

                                stopwatch_msg.command = STOPWATCH_CMD_RESET;
                                stopwatch_msg.data = 0;
                                xQueueSend(key_stopwatch_queue, &stopwatch_msg, 0);
                            }
                            //
                            else{

                                last_button_time = current_time;
                                key2_stopwatch = !key2_stopwatch;
                                key2_resetwatch = 1;
                                stopwatch_msg.command = STOPWATCH_CMD_START_PAUSE;
                                stopwatch_msg.data = key2_stopwatch;
                                xQueueSend(key_stopwatch_queue, &stopwatch_msg, 0);
                            }
                        }
                    }
					break;
                }
            }
        // 其他模式切换
        switch (keyNum) {
            case 4: // 模式切换
						{
                if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(100))) {
                    set_run = 0;
                    mode = (mode % 4) + 1;
				if ((App_CanUseOled() != 0U) &&
                    (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE)) {
                    OLED_Clear();

                    switch(mode){
                        case 1 :
                            (void)EM7028_hrs_DisEnable();
                            EM7028_HR_SetEnabled(0U);
                            break;
#if 0
                            MAX30102_off();
                        break;

#endif
                        case 3:
                            BME_POWER_OFF();
                            break;

                        case 4:
                            if ((EM7028_hrs_init() == 0U) && (EM7028_hrs_Enable() == 0U)) {
                                EM7028_HR_SetEnabled(1U);
                                HR_SpO2_showm(0, 0, 0);
                            } else {
                                EM7028_HR_SetEnabled(0U);
                                HR_SpO2_showm(0, 0, 0);
                            }
                            vTaskDelay (pdMS_TO_TICKS(10));
                            break;
#if 0
                            MAX30102_Reset();    //复机心传感器
                            vTaskDelay (pdMS_TO_TICKS(10));
                        break;
#endif
                    }
                    xSemaphoreGive(xI2CMutex);
				}
                    msg.command = KEY_CMD_MODE_CHANGE;
                    msg.data = mode;
						xQueueReset(key_mode_queue);
                    xQueueSend(key_mode_queue, &msg, pdMS_TO_TICKS(10));
                    xSemaphoreGive(xGlobalMutex);
                }
						}
                break;

            case 9:
						{
							if(set_run== 0){
                            bell_switch();
							}
						}
                  break;
            case 2: // 进入闹钟设置
                if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(100))) {
						set_run = 1;
                        set_bell = 1;
						set_time = 0;
                        msg.command = KEY_CMD_ALARM_SET;
                        msg.data = 1;
                        xQueueReset(key_alarm_queue);
                        xQueueSend(key_alarm_queue, &msg, pdMS_TO_TICKS(10));

                    xSemaphoreGive(xGlobalMutex);
                }
                    break;

            case 1: // 时间设置
                if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(100))) {

						set_run = 1;
						set_bell = 0;
                        set_time = 1;
                        msg.command = KEY_CMD_TIME_SET;
                        msg.data = 1;
                        xQueueReset(key_timeMode_queue);
                        xQueueSend(key_timeMode_queue, &msg, pdMS_TO_TICKS(10));
                        xSemaphoreGive(xGlobalMutex);
				}
					break;
			}
		}
		//设置模式下的按键处理
    else if( current_set_run == 1 ) {
        if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(100))) {
            if (current_set_time) {

                // 时间设置模式下的按键处理
                msg.command = KEY_CMD_SETTING_KEY;
                msg.data = keyNum;

                xQueueReset(key_timeMode_queue);
                xQueueSend(key_timeMode_queue, &msg, pdMS_TO_TICKS(10));
            } else if (current_set_bell) {
                // 闹钟设置模式下的按键处理
                msg.command = KEY_CMD_SETTING_KEY;
                msg.data = keyNum;

                xQueueReset(key_alarm_queue);
                xQueueSend(key_alarm_queue, &msg, pdMS_TO_TICKS(10));
            }
            xSemaphoreGive(xGlobalMutex);
        }
	}
}

void task_time(void *pvParameters)
{
    uint8_t current_mode = 1;
    TaskMessage_t msg;

    //
    xSemaphoreTake(sem_init, portMAX_DELAY);


    while(1) {
        if (App_CanUseOled() == 0U) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

		if(xQueueReceive(key_mode_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (msg.command == KEY_CMD_MODE_CHANGE) {
                current_mode = msg.data;
						}
					}

		if (set_run == 0) {			//检查是否为设置模式
        if (App_CanUseOled() == 0U) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

            switch(current_mode) {
                case 1:  // 时间模式
                    {
                        if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                            DS3231_getdate(&calendar);
                            DS3231_gettime(&calendar);

                            if(calendar.second != last_second ) {
                            last_second = calendar.second;

                                //更新时间
                                if (App_CanUseOled() != 0U) {
                                    show_time(&calendar);
                                }
                            }
                                xSemaphoreGive(xI2CMutex);
                        }
                    }
                    break;

                case 2:
                 if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(10))) {
						sensor_Hander();
                        xSemaphoreGive(xGlobalMutex);
                }
                    break;

                case 3:  // 秒表模式
                {
                    if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(10))) {
						stopwatch();
                    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        if (App_CanUseOled() != 0U) {
                            show_timer();
                        }
                        xSemaphoreGive(xI2CMutex);
                        }
                        timer_loop();
                        xSemaphoreGive(xGlobalMutex);
                    }
                }
                break;

                case 4:  // 心率模式
                 {
                     if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(10))) {
                        if (App_CanUseOled() != 0U) {
                            HR_SpO2_Hander();
                        }
                        xSemaphoreGive(xGlobalMutex);
                        }
                }
                break;
			}
		}
	        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

//
void vBlinkTimerCallback(TimerHandle_t xTimer)
{
    static uint8_t blinkState = 0;
    blinkState = !blinkState;

    // 发送闪烁消息到时间设置任务
    TaskMessage_t msg;
    msg.command = TIMER_CMD_BLINK;
    msg.data = blinkState;

    xQueueSend(key_timeMode_queue, &msg, pdMS_TO_TICKS(0));

}

void SetTimeAlarm_Hander(void *pvParameters)
{
    QueueSetMemberHandle_t xActivatedMember;

    while(1) {

        xActivatedMember = xQueueSelectFromSet(time_alarm_queue_set, portMAX_DELAY);
        if(xActivatedMember == key_timeMode_queue) {
            TaskMessage_t msg;
            TimerHandle_t xBlinkTimer = NULL;

        if (xQueueReceive(key_timeMode_queue, &msg, 0) == pdTRUE) {
            if (msg.command == KEY_CMD_TIME_SET) {

                //
                xBlinkTimer = xTimerCreate(
                    "BlinkTimer",
                    pdMS_TO_TICKS(300),
                    pdTRUE,
                    (void *)0,
                    vBlinkTimerCallback
                );

                if (xBlinkTimer == NULL) {
                    printf("Failed to create blink timer\n");
                    continue;
                }

								uint8_t blinkOn = 1;
                uint32_t inactivityTimer = xTaskGetTickCount();


                //
                if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(100))) {
                    DateTime temp_calendar;
                    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        DS3231_getdate(&temp_calendar);
                        DS3231_gettime(&temp_calendar);

                        time_data[0] = temp_calendar.year - 2000;
                        time_data[1] = temp_calendar.month;
                        time_data[2] = temp_calendar.dayofmonth;
                        time_data[3] = temp_calendar.hour;
                        time_data[4] = temp_calendar.minute;
                        time_data[5] = temp_calendar.second;
                        time_data[6] = temp_calendar.dayOfWeek;

									  if (App_CanUseOled() != 0U) {
									      OLED_Clear();
									  }
                        xSemaphoreGive(xI2CMutex);
                    }

                    set_shift = 0;
                    set_run =  1;
                    set_time = 1;
                    set_bell = 0;
										msg.command = TIMER_CMD_BLINK;
                    xSemaphoreGive(xGlobalMutex);
                }

								//
                if (xTimerStart(xBlinkTimer, 0) != pdPASS) {
                    printf("Failed to start blink timer\n");
                }

                //
                updateTimeDisplay(1);

                //
                while (set_run) {
                   BaseType_t xResult = xQueueReceive(key_timeMode_queue, &msg, pdMS_TO_TICKS(100));

                     if (xResult == pdTRUE) {
                        inactivityTimer = xTaskGetTickCount();
                        switch (msg.command) {
                            case KEY_CMD_SETTING_KEY:

                                handleTimeSettingKey(msg.data, &inactivityTimer);
                                 updateTimeDisplay(1);
                                break;
                            case TIMER_CMD_BLINK:
																 blinkOn = msg.data;
                                updateTimeDisplay(blinkOn);
                                break;
                            default:
                                printf("Unknown message command: %d\n", msg.command);
                                break;
                        }

											}

                    // 检查无操作超时(2分钟)
                    if ((xTaskGetTickCount() - inactivityTimer) > pdMS_TO_TICKS(120000)) {
                        if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(100))) {
                                set_run = 0;
                                set_time = 0;
                                xSemaphoreGive(xGlobalMutex);

                        printf("Auto-saving due to inactivity\n");
                    }

									}
							}
                // 保存时间设置
                saveTimeSettings();

                // 停止并删除定时器
                if (xBlinkTimer != NULL) {
                    xTimerStop(xBlinkTimer, 0);
                    xTimerDelete(xBlinkTimer, 0);
                    xBlinkTimer = NULL;
                }

                //
                if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(100))) {
                    set_run = 0;
                    set_time = 0;
                    xSemaphoreGive(xGlobalMutex);
                    printf("Global variables updated: set_run=%d, set_time=%d\n", set_run, set_time);
                } else {
                    printf("Warning: Failed to take mutex for variable update\n");
                    // 强制更新
                    set_run = 0;
                    set_time = 0;
                }

               //
                if (App_CanUseOled() != 0U) {
                    OLED_Clear();
                }
                }
            }
        }

        //
        else if(xActivatedMember == key_alarm_queue){
        TaskMessage_t msg;
        TimerHandle_t xAlarmBlinkTimer = NULL;
        //
        if (xQueueReceive(key_alarm_queue, &msg, 0) == pdTRUE) {
            if (msg.command == KEY_CMD_ALARM_SET ) {
        // 2. 创建用于闪烁的定时器
                xAlarmBlinkTimer = xTimerCreate(
                    "AlarmBlinkTimer",
                    pdMS_TO_TICKS(300),
                    pdTRUE,
                    (void *)0,
                    vAlarmBlinkTimerCallback
                );

                if (xAlarmBlinkTimer == NULL) {
                    printf("Failed to create alarm blink timer\n");
                    continue;
                }

				uint8_t blinkOn = 1;
                uint32_t inactivityTimer = xTaskGetTickCount();


                // 4. 初始化闹钟设置的临时数据
                if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(100))) {
                    // 如果闹钟数据为空，则用当前时间初始化
                    if (bell_data[0] == 0 && bell_data[1] == 0 && bell_data[2] == 0) {
                        DateTime temp_calendar;
                        if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
														DS3231_getdate(&temp_calendar);
                            DS3231_gettime(&temp_calendar);

                            bell_data[0] = temp_calendar.hour;
                            bell_data[1] = temp_calendar.minute;
                            bell_data[2] = 0;
                            bell_data[3] = temp_calendar.dayofmonth;
														if (App_CanUseOled() != 0U) {
														    OLED_Clear();
														}
														xSemaphoreGive(xI2CMutex);
                        }
                    }

										//
                    set_shift_bell = 0;
                    set_run = 1;
                    set_bell = 1;
                    set_time = 0;
                    msg.command = TIMER_CMD_BLINK;
                    xSemaphoreGive(xGlobalMutex);
                }

                // 启动闪烁定时器并更新初始显示
                if (xTimerStart(xAlarmBlinkTimer, 0) != pdPASS) {
                    printf("Failed to start alarm blink timer\n");
                }

                //
                updateAlarmDisplay(1);

                //
                while (set_run) {
                    // 等待按键或闪烁消息，500ms超时
                    BaseType_t xResult = xQueueReceive(key_alarm_queue, &msg, pdMS_TO_TICKS(100));

                    if (xResult == pdTRUE) {
                        inactivityTimer = xTaskGetTickCount(); // 重置无操作计时器

                        switch (msg.command) {
                            case KEY_CMD_SETTING_KEY:
                                handleAlarmSettingKey(msg.data, &inactivityTimer);
                                updateAlarmDisplay(1);
                                break;

                            case TIMER_CMD_BLINK:
																blinkOn = msg.data;
                                updateAlarmDisplay(blinkOn);
                                break;
														default:
                                printf("Unknown message command: %d\n", msg.command);
                                break;
                        }
                    }

                    // 检查无操作超时 (2分钟)
                   if ((xTaskGetTickCount() - inactivityTimer) > pdMS_TO_TICKS(120000)) {
                        if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(100))) {
                            set_run = 0;
                            set_bell = 0;
                            set_shift_bell = 0;

                            msg.command = KEY_CMD_MODE_CHANGE;
                            msg.data = 1;
                            xQueueSend(key_mode_queue, &msg, pdMS_TO_TICKS(100));

                            xSemaphoreGive(xGlobalMutex);
                        printf("Auto-saving due to inactivity\n");
                    }
				}
			}

                //
                saveAlarmSettings(); // 保存设置到DS3231

                if (xAlarmBlinkTimer != NULL) {
                    xTimerStop(xAlarmBlinkTimer, 0);
                    xTimerDelete(xAlarmBlinkTimer, 0);
                    xAlarmBlinkTimer = NULL;
                }

                //
                if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(100))) {
                    set_run = 0;
                    set_bell = 0;
                    xSemaphoreGive(xGlobalMutex);
                } else {
                    set_run = 0; // 强制更新
                    set_bell = 0;
                }

                //
                if (App_CanUseOled() != 0U) {
                    OLED_Clear();
                }
                printf("Alarm set mode exited successfully.\n");
                }
            }
        }
    }
}



// 时间设置按键处理
void handleTimeSettingKey(uint8_t keyNum, uint32_t *inactivityTimer)
{
    *inactivityTimer = xTaskGetTickCount(); // 重置无操作计时器
     TaskMessage_t msg;
    if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(100))) {
        switch (keyNum) {
            case 1:
                printf("Saving and exiting time set mode - Key 1 pressed\n");

				//
                set_run = 0;
                set_time = 0;
                set_shift = 0;
                msg.command = KEY_CMD_MODE_CHANGE;
                msg.data = 1;
                xQueueSend(key_mode_queue, &msg, pdMS_TO_TICKS(0));
                break;

            case 2: // 切换设置位置
                set_shift = (set_shift + 1) % 7; // 0-6循环
                printf("Set shift changed to: %d\n", set_shift);
                break;

            case 3:
                incrementTimeValue();
                break;

            case 4:
                decrementTimeValue();
                break;
        }
        xSemaphoreGive(xGlobalMutex);
    }
}

//
void incrementTimeValue(void)
{
 switch (set_shift) {
        case 0:
            time_data[5] = (time_data[5] + 1) % 60;
            break;
        case 1:
            time_data[4] = (time_data[4] + 1) % 60;
            break;
        case 2:
            time_data[3] = (time_data[3] + 1) % 24;
            break;
        case 3:
            {
                int maxDays = getMaxDaysOfMonth(time_data[0] + 2000, time_data[1]);
                time_data[2] = (time_data[2] % maxDays) + 1;
            }
            break;
        case 4:
             time_data[1] = (time_data[1] % 12) + 1;
            if (time_data[1] == 0) time_data[1] = 1;  // 确保不为0
            break;
        case 5:
            time_data[0] = (time_data[0] + 1) % 100;
            break;
        case 6: // 星期
              time_data[6] = (time_data[6] % 7) + 1;   // 确保范围1-7
            break;
    }
}

//
void decrementTimeValue(void)
{
   switch (set_shift) {
        case 0:
            time_data[5] = (time_data[5] == 0) ? 59 : (time_data[5] - 1);
            break;
        case 1:
            time_data[4] = (time_data[4] == 0) ? 59 : (time_data[4] - 1);
            break;
        case 2:
            time_data[3] = (time_data[3] == 0) ? 23 : (time_data[3] - 1);
            break;
        case 3:
            {
                int maxDays = getMaxDaysOfMonth(time_data[0] + 2000, time_data[1]);
                time_data[2] = (time_data[2] == 1) ? maxDays : (time_data[2] - 1);
            }
            break;
        case 4:
            time_data[1] = (time_data[1] == 1) ? 12 : (time_data[1] - 1);
            break;
        case 5:
            time_data[0] = (time_data[0] == 0) ? 99 : (time_data[0] - 1);
            break;
        case 6: // 星期
            time_data[6] = (time_data[6] == 1) ? 7 : (time_data[6] - 1);
            break;
    }
}

// 更新时间显示
void updateTimeDisplay(uint8_t blinkOn)
{
   if (App_CanUseOled() == 0U) {
        return;
   }

   if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(0))) {

        // 显示所有时间组件，确保切换位置时原位置不会消失
        display_sec(time_data[5], (set_shift == 0) ? blinkOn : 1);
        display_min(time_data[4], (set_shift == 1) ? blinkOn : 1);
        display_hour(time_data[3], (set_shift == 2) ? blinkOn : 1);
        display_day(time_data[2], (set_shift == 3) ? blinkOn : 1);
        display_month(time_data[1], (set_shift == 4) ? blinkOn : 1);
        display_year(time_data[0] + 2000, (set_shift == 5) ? blinkOn : 1);
        display_week(time_data[6], (set_shift == 6) ? blinkOn : 1);     // 星期

        xSemaphoreGive(xI2CMutex);
    } else {
        printf("Failed to take mutex for display update\n");
    }
}

// 保存时间设置
void saveTimeSettings(void)
{
   if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100))) {
        printf("Saving time settings: %02d-%02d-%02d %02d:%02d:%02d Week:%d\n",
               time_data[0], time_data[1], time_data[2],
               time_data[3], time_data[4], time_data[5], time_data[6]);

        DS3231_setTime(time_data[3], time_data[4], time_data[5]);
        DS3231_setDate(time_data[0], time_data[1], time_data[2], time_data[6]);

        xSemaphoreGive(xI2CMutex);
    } else{
		printf("faild to save time setting");
	}
}

//保存闹钟设置
void saveAlarmSettings(void)
{
    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100))) {

        DS3231_SetAlarm1Daily(bell_data[2], bell_data[1], bell_data[0], bell_data[3]); // bell_data[0]=hour, [1]=minute, [2]=second, [3]=day

        printf("Alarm time saved to DS3231: Day %d, %02d:%02d:%02d\n",
               bell_data[3], bell_data[0], bell_data[1], bell_data[2]);

        xSemaphoreGive(xI2CMutex);
    } else {
        printf("Error: Failed to take I2C mutex to save alarm settings.\n");
    }
}

// 闹钟设置按键处理
void handleAlarmSettingKey(uint8_t keyNum, uint32_t *inactivityTimer)
{
   *inactivityTimer = xTaskGetTickCount();
    TaskMessage_t msg;
    if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(100))) {

        // KEY1: 移动光标（切换设置位置）
        //
        //
        //

       switch (keyNum) {
            case 1: // KEY1: 移动光标（切换设置位置）
                set_shift_bell = (set_shift_bell + 1) % 4; // 0-3循环：时→分→秒→日

                break;

            case 2:
                printf("Saving alarm settings and exiting\n");

                set_run = 0;
                set_bell = 0;
                set_shift_bell = 0;
                msg.command = KEY_CMD_MODE_CHANGE;
                msg.data = 1;
                xQueueSend(key_mode_queue, &msg, pdMS_TO_TICKS(100));
                break;

            case 3:

                incrementAlarmValue();
                break;

            case 4:

                decrementAlarmValue();
                break;

            default:
                printf("Unknown alarm setting key: %d\n", keyNum);
                break;
        }
        xSemaphoreGive(xGlobalMutex);
    }
}

//
void incrementAlarmValue(void)
{
    switch (set_shift_bell) {
        case 0:
            bell_data[2] = (bell_data[2] + 1) % 60;
            break;
        case 1:
            bell_data[1] = (bell_data[1] + 1) % 60;
            break;
        case 2:
            bell_data[0] = (bell_data[0] + 1) % 24;
            break;
        case 3:
            bell_data[3] = (bell_data[3] % 31) + 1;
            if (bell_data[3] == 0) bell_data[3] = 1;
            break;
    }
}

//
void decrementAlarmValue(void)
{
    switch (set_shift_bell) {
        case 0:
            bell_data[2] = (bell_data[2] == 0) ? 59 : (bell_data[2] - 1);
            break;
        case 1:
            bell_data[1] = (bell_data[1] == 0) ? 59 : (bell_data[1] - 1);
            break;
        case 2:
            bell_data[0] = (bell_data[0] == 0) ? 23 : (bell_data[0] - 1);
            break;
        case 3:
            bell_data[3] = (bell_data[3] == 1) ? 31 : (bell_data[3] - 1);
            break;
    }
}

// 更新闹钟显示
void updateAlarmDisplay(uint8_t blinkOn)
{
    if (App_CanUseOled() == 0U) {
        return;
    }

    if (xSemaphoreTake(xI2CMutex, portMAX_DELAY)) {

        // 显示标题
        OLED_ShowString(40, 0, "ALARM SET",OLED_8X16);

        //
        display_hour(bell_data[0], (set_shift_bell == 2) ? blinkOn : 1);
        display_min(bell_data[1], (set_shift_bell == 1) ? blinkOn : 1);
        display_sec(bell_data[2], (set_shift_bell == 0) ? blinkOn : 1);
        display_day(bell_data[3], (set_shift_bell == 3) ? blinkOn : 1);

				 //
        if (set_shift_bell==3) {
           // OLED_ClearChar12(set_shift_bell * 6, 0);
					OLED_ClearArea(24,0,8,16);
        }

        // 显示当前位置指示
         char* positions[] = {"SEC", "MIN", "HOUR", "DAY"};
					OLED_ShowString(0, 0, positions[set_shift_bell],OLED_8X16);

        xSemaphoreGive(xI2CMutex);
    } else {
        printf("Failed to take mutex for alarm display update\n");
    }
}
//
void vAlarmBlinkTimerCallback(TimerHandle_t xTimer)
{
    static uint8_t blinkState = 0;
    blinkState = !blinkState;

    TaskMessage_t msg;
    msg.command = TIMER_CMD_BLINK;
    msg.data = blinkState;

    xQueueSend(key_alarm_queue, &msg, pdMS_TO_TICKS(0));

}

void stopwatch(void )
{
    StopwatchMessage_t msg;

    if (xQueueReceive(key_stopwatch_queue, &msg, pdMS_TO_TICKS(0)) == pdTRUE) {

            switch (msg.command) {
                case STOPWATCH_CMD_START_PAUSE:
                    if(msg.data==0){
                        enableClock();

                    }
                    else if(msg.data==1){           //暂停
                        disableClock();
                    }
                   break;

                case STOPWATCH_CMD_RESET:       //复位
                  disableClock();
                    zero_timer();

                    break;

                default:
                    break;
       }

   }
}

int main(void)
{
    Ota_UpdateCaptureResetCause();

	USART_Config();
	FPU_Enable();
	power_switch_init();
	// HSE_SetSysClock();
    FreeRTOS_Start();                                   // 启动freertos
}

//
uint8_t isLeapYear(int year)
{
    if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
    {
        return 1;
    }
    else
    {
        return 0; // 不是闰年
    }
}

//
uint8_t getMaxDaysOfMonth(int year, int month)
{
    //
    uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // 闰年二月特殊处理
    if (month == 2 && isLeapYear(year))
    {
        return 29;
    }
    else
    {
        return daysInMonth[month - 1]; // 返回对应月份天数
    }
}
//
void sensor_Hander(void)
{
    if (App_CanUseOled() == 0U) {
        return;
    }


	 if (xSemaphoreTake(xI2CMutex, portMAX_DELAY) == pdTRUE) {

    float temp, hum, press;
    int temp1, temp2;
    int hum1, hum2;

     BME280_Read_All(&temp, &hum, &press);

        temp1 = (int)temp;                    //整数部分
        temp2 = (int)((temp - temp1) * 100);  //小数部分

        hum1 = (int)hum;                      //整数部分
        hum2 = (int)((hum - hum1) * 100);    //小数部分

        bme280_data.Hum1 = hum1;
        bme280_data.Hum2 = hum2;
        bme280_data.Press = press;
        bme280_data.Temp1 = temp1;
        bme280_data.Temp2 = temp2;


	    if (App_CanUseOled() != 0U) {
	        show_bme280_time(&bme280_data); //只显示温度和湿度
	    }
		xSemaphoreGive(xI2CMutex);
	}
}

#if 0
static void HR_SpO2_Hander_MAX30102_Legacy(void)
{
    static int32_t last_hr = 0;
    static int32_t last_spo2 = 0;
    static int invalid_count = 0;

    static uint8_t alg_state = 0; 	// 0: 缓冲填充阶段, 1: 稳定运行阶段
    static int32_t sample_cnt = 0;

    uint8_t num_samples_to_read = 0;
    uint8_t write_ptr, read_ptr;

		int32_t n_ir_buffer_length = 500;

    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

        // 1. 读取读写指针，计算FIFO里实际有多少数据
        MAX30102_IIC_ReadByte(REG_FIFO_WR_PTR, &write_ptr);
        MAX30102_IIC_ReadByte(REG_FIFO_RD_PTR, &read_ptr);

				//屏蔽高位
				write_ptr &= 0x1F;
				read_ptr &= 0x1F;

        //
        if (write_ptr >= read_ptr) {
            num_samples_to_read = write_ptr - read_ptr;
		//			printf("a = %d \n", num_samples_to_read);
        } else {
            num_samples_to_read = (write_ptr + 32) - read_ptr;
			//		printf("b = %d \n", num_samples_to_read);
        }

		//
    if(num_samples_to_read > 32) num_samples_to_read = 32;

        //
        if(num_samples_to_read == 0 ) {
						HR_SpO2_showm(last_hr, last_spo2, 0);
            xSemaphoreGive(xI2CMutex);
            return;
        }

        //
        for (int i = 0; i < num_samples_to_read; i++) {
            //
            max30102_FIFO_ReadBytes(REG_FIFO_DATA, temp);

            uint32_t new_red = (long)((long)((long)temp[0] & 0x03) << 16) | (long)temp[1] << 8 | (long)temp[2];
            uint32_t new_ir  = (long)((long)((long)temp[3] & 0x03) << 16) | (long)temp[4] << 8 | (long)temp[5];

            //
            // 这种方法虽然效率略低(内存拷贝)，但能完美适配Maxim算法
            //
            if (sample_cnt < n_ir_buffer_length) {
                aun_red_buffer[sample_cnt] = new_red;
                aun_ir_buffer[sample_cnt]  = new_ir;
                sample_cnt++;
            }
            else {
                //
                //
                // 优化：可以使用memmove 替代循环
                // for(int j=0; j<n_ir_buffer_length-1; j++) { ... }

                memmove(&aun_red_buffer[0], &aun_red_buffer[1], (n_ir_buffer_length - 1) * sizeof(uint32_t));
                memmove(&aun_ir_buffer[0],  &aun_ir_buffer[1],  (n_ir_buffer_length - 1) * sizeof(uint32_t));

                aun_red_buffer[n_ir_buffer_length - 1] = new_red;
                aun_ir_buffer[n_ir_buffer_length - 1]  = new_ir;

                alg_state = 1; // 标记缓冲区已满，可以计算
            }
        }

        xSemaphoreGive(xI2CMutex);
    }

    // 4. 执行算法 (仅当缓冲区满，且每隔一定样本数执行一次，防止CPU过载)
    //
    static int samples_since_last_run = 0;
    samples_since_last_run += num_samples_to_read;

    if (alg_state == 1 && samples_since_last_run >= 50) {

        samples_since_last_run -= 50;

        maxim_heart_rate_and_oxygen_saturation(aun_ir_buffer, n_ir_buffer_length, aun_red_buffer,
                                               &n_sp02, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid);

        // --- 结果平滑处理 ---
        if (ch_hr_valid && ch_spo2_valid && n_heart_rate > 40 && n_heart_rate < 200 && n_sp02 > 50 && n_sp02 <= 100) {

            //
            if (last_hr == 0) last_hr = n_heart_rate;
            else last_hr = (last_hr * 90 + n_heart_rate * 10) / 100;

            if (last_spo2 == 0) last_spo2 = n_sp02;
            else last_spo2 = (last_spo2 * 95 + n_sp02 * 5) / 100;

            invalid_count = 0;
            HR_SpO2_showm(last_hr, last_spo2, 1); // 显示有效数据

        } else {

            invalid_count++;
            if (invalid_count >= 5) {
								last_hr = 0;
                last_spo2 = 0;
                HR_SpO2_showm(last_hr, last_spo2, 0);

            }
        }
    }
		if(last_hr == 0 ||  last_spo2 == 0)
		{
		HR_SpO2_showm(last_hr, last_spo2, 0);

		}
}

//
#endif

void bell_switch(void)
{

	static bool bell_flag = true;

    if ((App_CanUseOled() != 0U) &&
        (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE)) {

    if (bell_flag) {
				bell_flag = !bell_flag;
        Motor_GPIO_off();
        DateTime current_time;
        DS3231_getdate(&current_time);
        DS3231_gettime(&current_time);

        // 使用 bell_data 中存储的日期，如果未设置则默认为当天
        uint8_t alarm_day = bell_data[3];

        // 如果闹钟时间已过，则设置为明天。bell_data[0] 小时，bell_data[1] 分钟，bell_data[2] 秒，bell_data[3] 日期
        if (bell_data[0] < current_time.hour ||
           (bell_data[0] == current_time.hour && bell_data[1] < current_time.minute))
        {
            // 日期加一，并处理月末和年末的情况
            alarm_day = current_time.dayofmonth + 1;
            uint8_t max_days = getMaxDaysOfMonth(current_time.year, current_time.month);
            if (alarm_day > max_days) {
                alarm_day = 1;
            }
        }

				OLED_ShowImage(100,45,16,16,alalrm_data);
				DS3231_ClearAlarmFlag(1);
        DS3231_SetAlarm1Daily(bell_data[2], bell_data[1], bell_data[0], alarm_day);
        DS3231_EnableAlarmInterrupt(1, 1);

    } else if (!(bell_flag)){					//关闭闹钟

				bell_flag = !bell_flag;
				Motor_GPIO_off();
				OLED_ClearArea(100,45,16,16);
        DS3231_EnableAlarmInterrupt(1, 0);
        DS3231_ClearAlarmFlag(1);

    }
		xSemaphoreGive(xI2CMutex);
	}
}

//中断接收ble数据

void USART2_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if ((USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET) ||
        (USART_GetFlagStatus(USART2, USART_FLAG_FE) != RESET) ||
        (USART_GetFlagStatus(USART2, USART_FLAG_NE) != RESET)) {
        (void)USART2->SR;
        (void)USART2->DR;
        Ota_UpdateOnUartRxDrop();
    }

    if (USART_GetITStatus(USART2, USART_IT_IDLE) != RESET) {
        (void)USART2->SR;
        (void)USART2->DR;
        if ((task_key_handle != NULL) && (g_task_key_ready != 0U)) {
            vTaskNotifyGiveFromISR(task_key_handle, &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void DMA1_Stream5_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (DMA_GetITStatus(DEBUG_USART_RX_DMA_STREAM, DMA_IT_HTIF5) != RESET) {
        DMA_ClearITPendingBit(DEBUG_USART_RX_DMA_STREAM, DMA_IT_HTIF5);
        if ((task_key_handle != NULL) && (g_task_key_ready != 0U)) {
            vTaskNotifyGiveFromISR(task_key_handle, &xHigherPriorityTaskWoken);
        }
    }

    if (DMA_GetITStatus(DEBUG_USART_RX_DMA_STREAM, DMA_IT_TCIF5) != RESET) {
        DMA_ClearITPendingBit(DEBUG_USART_RX_DMA_STREAM, DMA_IT_TCIF5);
        if ((task_key_handle != NULL) && (g_task_key_ready != 0U)) {
            vTaskNotifyGiveFromISR(task_key_handle, &xHigherPriorityTaskWoken);
        }
    }

    if ((DMA_GetITStatus(DEBUG_USART_RX_DMA_STREAM, DMA_IT_TEIF5) != RESET) ||
        (DMA_GetITStatus(DEBUG_USART_RX_DMA_STREAM, DMA_IT_DMEIF5) != RESET) ||
        (DMA_GetITStatus(DEBUG_USART_RX_DMA_STREAM, DMA_IT_FEIF5) != RESET)) {
        DMA_ClearITPendingBit(DEBUG_USART_RX_DMA_STREAM,
                              DMA_IT_TEIF5 | DMA_IT_DMEIF5 | DMA_IT_FEIF5);
        Ota_UpdateOnUartRxDrop();
        if ((task_key_handle != NULL) && (g_task_key_ready != 0U)) {
            vTaskNotifyGiveFromISR(task_key_handle, &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void TIM2_IRQHandler (void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        // 释放信号量，通知计步任务
        xSemaphoreGiveFromISR(xStepSemaphore, &xHigherPriorityTaskWoken);

        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

        //
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void task_step_detect(void *pvParameters)
{
	uint8_t current_set_run = 0; // 用于存储从全局变量读取的set_run
	static uint32_t last_step = 0;

    mpu6050_data_t msg;

      // 1. 定义周期变量
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(20);

    xLastWakeTime = xTaskGetTickCount();

    while(1)
    {
          if (App_BackgroundWorkPaused() != 0U) {
              vTaskDelay(pdMS_TO_TICKS(100));
              xLastWakeTime = xTaskGetTickCount();
              continue;
          }
          vTaskDelayUntil(&xLastWakeTime, xFrequency);
            //
      if (xSemaphoreTake(xGlobalMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
             //   current_mode = mode;
                current_set_run = set_run;
				xSemaphoreGive(xGlobalMutex);

            //
            if (current_set_run == 0)
            {
                //
                if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(10)) == pdTRUE)
                {
                    MPU6050_Proc(); // 调用 MPU6050 计步算法
                    xSemaphoreGive(xI2CMutex);
                }
			}
                if(xQueueReceive(mpu6050_queue,&msg,0)==pdTRUE){
                    if ((msg.step_count > 1) &&
                        (last_step != msg.step_count) &&
                        (App_CanUseOled() != 0U))
                    {
                        show_step( msg.step_count);
                        last_step=msg.step_count;
                    }
                }
		}
    }
}

//
void EXTI1_IRQHandler (void)
{

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	 if(EXTI_GetITStatus(EXTI_Line1) != RESET)
    {

        EXTI_ClearITPendingBit(EXTI_Line1);
         xTaskNotifyFromISR(task_ring_battery_handle, 0x02, eSetBits, &xHigherPriorityTaskWoken);
        // 如果有更高优先级的任务被唤醒，则进行任务切换
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void DMA2_Stream0_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint16_t half = (uint16_t)(ADC_BUFFER_SIZE / 2U);

    if (DMA_GetITStatus(DMA2_Stream0, DMA_IT_HTIF0) != RESET)
    {
        DMA_ClearITPendingBit(DMA2_Stream0, DMA_IT_HTIF0);

        // HT: first half of current target is complete
        if (DMA_GetCurrentMemoryTarget(DMA2_Stream0) == DMA_Memory_0)
        {
            adc_ready_buffer = adc_buffer;
        }
        else
        {
            adc_ready_buffer = adc_buffer_b;
        }
        adc_ready_count = half;
        xTaskNotifyFromISR(task_ring_battery_handle, 0x01, eSetBits, &xHigherPriorityTaskWoken);
    }

    if (DMA_GetITStatus(DMA2_Stream0, DMA_IT_TCIF0) != RESET)
    {
        DMA_ClearITPendingBit(DMA2_Stream0, DMA_IT_TCIF0);

        // TC: second half of the just-completed target is complete
        if (DMA_GetCurrentMemoryTarget(DMA2_Stream0) == DMA_Memory_0)
        {
            adc_ready_buffer = &adc_buffer_b[half];
        }
        else
        {
            adc_ready_buffer = &adc_buffer[half];
        }
        adc_ready_count = (uint16_t)(ADC_BUFFER_SIZE - half);
        xTaskNotifyFromISR(task_ring_battery_handle, 0x01, eSetBits, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// 启用FPU函数
//static void FPU_Enable(void)
//{
//
//    // 设置CPACR寄存器的CP10和CP11字段为全访问权限
//    SCB->CPACR |= ((3UL << 10*2) | (3UL << 11*2));
//}

void task_ring_battery(void *pvParameters)
{
    static volatile uint32_t ADC_battery_value = 0;
    uint32_t notify_value = 0;
    while (1)
    {
       if(xTaskNotifyWait(0x00, 0xffffffff,&notify_value, portMAX_DELAY))
       {
        if (App_BackgroundWorkPaused() != 0U) {
            continue;
        }

        if ((notify_value & 0x01) != 0)
            {
                // 电量更新
                 battery_update();
            }

        else if ((notify_value & 0x02) != 0)
            {
            // 处理震动马达播放
           Motor_alarm();
				Motor_GPIO_ON();
          }
        else if((notify_value & 0x03) != 0){
            // 同时收到电量更新和铃声播放的通知
            battery_update();
            Motor_alarm();

        }
       }
    }
}

//电量更新
void battery_update(void)
{
    static volatile uint32_t ADC_battery_value = 0;
    uint32_t adc_avg = 0;
    volatile uint16_t *buf = adc_ready_buffer;
    uint16_t count = adc_ready_count;
    if (count == 0U){
        return;
    }
    for(uint16_t i = 0; i < count; i++){
        adc_avg += buf[i];
    }
    ADC_battery_value = (uint32_t)(adc_avg / count);
 //   printf("Battery ADC Value: %lu\n", ADC_battery_value);
    // 更新电量显示
    uint32_t Bat_capacity = 0;

    //电池电量2.7v及以下设置为0%
    //ADC采样0-4095对应电压0-3.3V
    //电池电压2.7V对应ADC采样值约3276
//    if (ADC_battery_value > 3211) {
//        Bat_capacity = (ADC_battery_value - 3211) * 100 / 759;
//    } else {
	if (ADC_battery_value > 2048) {
        Bat_capacity = (ADC_battery_value - 2048) * 100 / 480;
    } else {
        Bat_capacity = 0;
    }

    if (Bat_capacity > 100) Bat_capacity = 100; // 防止溢出

    Bat_capacity = battery_filter_capacity(Bat_capacity);

    if ((App_CanUseOled() != 0U) &&
        (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE))
    {
        battery_show(Bat_capacity);
        xSemaphoreGive(xI2CMutex);
    }
}

static uint32_t battery_filter_capacity(uint32_t capacity)
{
    enum {
        BATTERY_CONFIRM_SAMPLES = 2,
        BATTERY_FAST_CHANGE_STEP = 2
    };
    static uint32_t displayed_capacity = 0;
    static uint32_t pending_capacity = 0;
    static uint8_t confirm_count = 0;
    static uint8_t initialized = 0;
    uint32_t diff;

    if (initialized == 0U) {
        displayed_capacity = capacity;
        pending_capacity = capacity;
        initialized = 1U;
        return displayed_capacity;
    }

    if (capacity == displayed_capacity) {
        pending_capacity = capacity;
        confirm_count = 0;
        return displayed_capacity;
    }

    diff = (capacity > displayed_capacity) ?
           (capacity - displayed_capacity) :
           (displayed_capacity - capacity);

    if (diff >= BATTERY_FAST_CHANGE_STEP) {
        displayed_capacity = capacity;
        pending_capacity = capacity;
        confirm_count = 0;
        return displayed_capacity;
    }

    if (capacity != pending_capacity) {
        pending_capacity = capacity;
        confirm_count = 1;
        return displayed_capacity;
    }

    if (++confirm_count >= BATTERY_CONFIRM_SAMPLES) {
        displayed_capacity = capacity;
        confirm_count = 0;
    }

    return displayed_capacity;
}

void Motor_alarm(void)
{
            printf(" Alarm Triggered!\r\n");
            // 1. 打开震动马达
            Motor_GPIO_ON();

			//
            if (xSemaphoreTake(xI2CMutex, portMAX_DELAY))
            {
                DS3231_ClearAlarmFlag(1);
                xSemaphoreGive(xI2CMutex);
            }
            xTimerStart(alarm_stop_timer, pdMS_TO_TICKS(0));

           //关闭震动马达
   //        Motor_GPIO_off();

}

void alarm_stop_timer_callback (TimerHandle_t xTimer)
{
    // 停止震动马达并停止定时器
    Motor_GPIO_off();
    xTimerStop(alarm_stop_timer, 0);
    xTimerDelete(alarm_stop_timer, 0);
    alarm_stop_timer = NULL;
}

/*
 *
* @arg Bit_RESET: to clear the port pin
* @arg Bit_SET: to set the port pin
*/
void LED_Switch(BitAction state)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_6, state);
}

void LED_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

}
