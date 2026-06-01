/*
 * FreeRTOS V202212.01
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
 * FreeRTOS API DOCUMENTATION AVAILABLE ON THE FreeRTOS.org WEB SITE.
 *
 * See http://www.freertos.org/a00110.html
 *----------------------------------------------------------*/

#define configUSE_PREEMPTION		1
#define configUSE_IDLE_HOOK			0                   //绌洪棽閽╁瓙
#define configUSE_TICK_HOOK			0
#define configCPU_CLOCK_HZ			( ( unsigned long ) 100000000 )
#define configTICK_RATE_HZ			( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES		( 10 )
#define configMINIMAL_STACK_SIZE	( ( unsigned short ) 256 )
#define configTOTAL_HEAP_SIZE		( ( size_t ) ( 40 * 1024 ) )
#define configMAX_TASK_NAME_LEN		( 16 )
#define configUSE_TRACE_FACILITY	1
#define configUSE_16_BIT_TICKS		0
#define configIDLE_SHOULD_YIELD		1


/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */

#define INCLUDE_vTaskPrioritySet		1
#define INCLUDE_uxTaskPriorityGet		1
#define INCLUDE_vTaskDelete				1
#define INCLUDE_vTaskCleanUpResources	0
#define INCLUDE_vTaskSuspend			1
#define INCLUDE_vTaskDelayUntil			1
#define INCLUDE_vTaskDelay				1

#define configPRIO_BITS                         4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY         ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )
#define configLIBRARY_KERNEL_INTERRUPT_PRIORITY configLIBRARY_LOWEST_INTERRUPT_PRIORITY

/*娣诲姞涓変釜蹇呰鐨勫畯 */
#define xPortPendSVHandler  PendSV_Handler
#define vPortSVCHandler     SVC_Handler
#define INCLUDE_xTaskGetSchedulerState   1
#define xPortSysTickHandler SysTick_Handler

//鍚敤鍔ㄦ€佸唴瀛樺垎閰?
#define configSUPPORT_DYNAMIC_ALLOCATION 1

//鍚敤缁熻淇℃伅鐨勬牸寮忓寲鍑芥暟
//涓巚TaskList() 鍜?vTaskGetRunTimeStats()鍑芥暟鏈夊叧
#define configGENERATE_RUN_TIME_STATS 0

//鍚敤 FreeRTOS 鐨勮繍琛屾椂缁熻鍔熻兘
#define configUSE_STATS_FORMATTING_FUNCTIONS 1

//鍚姩闃熷垪闆?
#define configUSE_QUEUE_SETS    1

//杞欢瀹氭椂鍣ㄥ叧閿畯
#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY 9
#define configTIMER_TASK_STACK_DEPTH         256
#define configTIMER_QUEUE_LENGTH             10


//浜掓枼閿?
#define configUSE_MUTEXES 1

//鑾峰彇浠诲姟鏍堝墿浣欐渶灏忕┖闂?
#define INCLUDE_uxTaskGetStackHighWaterMark 1

#define configUSE_FPU   1 

//鎵撳紑浣庡姛鑰楁ā寮?
#define configUSE_TICKLESS_IDLE 1
//杩涘叆浣庡姛鑰楁ā寮忕殑鏈€鐭樆濉炴椂闂?
#define configEXPECTED_IDLE_TIME_BEFORE_SLEEP 2

//寮€鍚秷鎭槦鍒楅泦

//寮€鍚洿鎺ヤ换鍔￠€氱煡
#define configUSE_TASK_NOTIFICATIONS 1

//浠诲姟閫氱煡鏁扮粍闀垮害
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 2

#endif /* FREERTOS_CONFIG_H */

