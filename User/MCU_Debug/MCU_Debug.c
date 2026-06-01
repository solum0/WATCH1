#include "stm32f4xx.h"
#include "usart.h"                      // 串口通信

void HSE_SetSysClock(void) {
    __IO uint32_t HSEStartUpStatus = 0;

    // 1. 复位 RCC 到默认状态(此时系统回落到内部16MHz HSI)
    RCC_DeInit();

    // 2. 开启外部8MHz 晶振 (HSE)
    RCC_HSEConfig(RCC_HSE_ON);

    // 3. 等待 HSE 起振就绪
    HSEStartUpStatus = RCC_WaitForHSEStartUp();

    if (HSEStartUpStatus == SUCCESS) {
        // --- 修复点1：开启 PWR 时钟并设置电压级别为 Scale 1 (100MHz 必须) ---
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
        PWR->CR |= PWR_CR_VOS; // 操作寄存器设置 Scale 1 模式，保证高频稳定供电

        // --- 修复点2：极其重要！设置 AHB/APB 总线分频，防止 APB1 超频死机 ---
        RCC_HCLKConfig(RCC_SYSCLK_Div1);   // AHB = 100MHz (最大100MHz)
        RCC_PCLK2Config(RCC_HCLK_Div1);    // APB2 = 100MHz (最大100MHz)
        RCC_PCLK1Config(RCC_HCLK_Div2);    // APB1 = 50MHz  (最大50MHz，这里必须二分频)

        // 4. 配置 PLL：源为HSE(8M), M=8, N=400, P=4, Q=7
        // 计算公式: (8MHz / 8) * 400 / 4 = 100MHz
        RCC_PLLConfig(RCC_PLLSource_HSE, 8, 400, 4, 7);

        // 5. 使能 PLL 并等待锁定
        RCC_PLLCmd(ENABLE);
        while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

        // --- 修复点3：配置 Flash，开启预取指和缓存(防止 CPU 饿死) ---
        FLASH_SetLatency(FLASH_Latency_3); // 100MHz对应 3WS
        FLASH_PrefetchBufferCmd(ENABLE);   // 开启预取指缓冲
        FLASH_InstructionCacheCmd(ENABLE); // 开启指令缓存
        FLASH_DataCacheCmd(ENABLE);        // 开启数据缓存

        // 6. 切换系统时钟源为 PLL
        RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

        // 7. 等待系统时钟源成功切换为 PLL (0x08)
        while (RCC_GetSYSCLKSource() != 0x08);
        
        printf("Clock Config Success!\r\n");
    } 
    else {
        // ----------------------------------------------------
        // 如果代码掉进了这里，说明你的 8MHz 外部晶振根本没有起振
        // 可能是没焊好、电容不对、或者引脚短路
        // ----------------------------------------------------
        printf("HSE Hardware Failed! System is using 16MHz HSI.\r\n");

    }
		uint8_t pll_source = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) >> 22;
		printf("PLL Source: %d (0 = HSI 16MHz, 1 = HSE 8MHz)\r\n", pll_source);
}


// 启用FPU函数
void FPU_Enable(void)
{
    // 设置CPACR寄存器的CP10和CP11字段为全访问权限
    SCB->CPACR |= ((3UL << 10*2) | (3UL << 11*2));
}

































