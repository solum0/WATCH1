#include "hi2c.h" 

//PB6 - SCL, PB7 - SDA
void hi2c_init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1,ENABLE);
	
	GPIO_InitTypeDef hi2_structure;
	hi2_structure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	hi2_structure.GPIO_Mode = GPIO_Mode_AF;
	hi2_structure.GPIO_OType = GPIO_OType_OD;
	
	hi2_structure.GPIO_PuPd = GPIO_PuPd_UP;
	hi2_structure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB,&hi2_structure);
	
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_I2C1); // SCL 
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_I2C1); // SDA I2C_InitTypeDef
	
	I2C_InitTypeDef hi2c_InitStructure;
	hi2c_InitStructure.I2C_Mode = I2C_Mode_I2C;
	hi2c_InitStructure.I2C_Ack = I2C_Ack_Enable;             // 使能应答
	hi2c_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit; // 7位地址
	hi2c_InitStructure.I2C_ClockSpeed = 400000; 	 // 400kHz快速模式
	hi2c_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;      // 时钟占空比

	
	I2C_Init(I2C1,&hi2c_InitStructure);
	I2C_Cmd(I2C1,ENABLE);
}	

	/*
	将7位地址左移1位
	写操作：`uint8_t write_addr = (addr << 1) | 0;
	读操作：`uint8_t read_addr = (addr << 1) | 1;`
	*/
	
uint8_t h_I2C_SendBytes(uint8_t Addr, uint8_t *pData, uint8_t Size)
{
	// #1. 总线空闲检测，BUSY总线忙标志位，0总线空闲，1总线忙
	while(I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) == SET);
	
	// #2. 发送起始位1函数，传参
	I2C_GenerateSTART(I2C1, ENABLE);
	
	//SB标志位，判断起始位是否发送完成，0起始位未发送，1起始位发送完成
	while(I2C_GetFlagStatus(I2C1, I2C_FLAG_SB) == RESET);
	
	// #3. 寻址阶段（写模式）
	
	//清除AF应答失败标志位
	I2C_ClearFlag(I2C1, I2C_FLAG_AF);
	
	//从机地址为7位，确保最低位为0，写模式
	I2C_SendData(I2C1, Addr);

	while(1)
	{
		//ADDR1寻址成功标志位
		if(I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR) == SET)
		{
			break;
		}
		if(I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == SET)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);
			return 3; // 寻址失败
		}
	}
	
	// 清除ADDR标志位，强制
	I2C_ReadRegister(I2C1, I2C_Register_SR1);
	I2C_ReadRegister(I2C1, I2C_Register_SR2);
	
	// #4. 发送数据
	for(uint16_t i=0; i<Size; i++)
	{
		while(1)
		{
			if(I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == SET)
			{
				I2C_GenerateSTOP(I2C1, ENABLE);
				return 2; // 数据被拒收
			}
			
			//TXE标志位判断发送寄存器是否为空	，1为空
			if(I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE) == SET)
			{
				break;
			}
		}
		I2C_SendData(I2C1, pData[i]);
	}
	
	//4.1 判断数据是否发送成功
	while(1)
	{
		if(I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == SET)
		{
				I2C_GenerateSTOP(I2C1, ENABLE);
				return 1; // 数据被拒收			
		}
		
		// #5. 结束传输
		
		//BTF标志位判断发送和移位寄存器是否为空，1为空
		if(I2C_GetFlagStatus(I2C1, I2C_FLAG_BTF) == SET)
		{
			break;
		}
	}
	
	//5.1 发送完成，发送停止位
	I2C_GenerateSTOP(I2C1, ENABLE);
	return 0; // 成功


}

uint8_t h_I2C_ReadReceives(uint8_t DevAddr, uint8_t ReadAddr, uint8_t *pBuffer, uint8_t Size)
{
    // 1. 检查总线繁忙
    while(I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) == SET);
    
    // 2. 发送起始位
    I2C_GenerateSTART(I2C1, ENABLE);
    while(I2C_GetFlagStatus(I2C1, I2C_FLAG_SB) == RESET);
    
    // 3. 发送设备地址（写模式）
    I2C_ClearFlag(I2C1, I2C_FLAG_AF);
    I2C_SendData(I2C1, DevAddr <<1 );
    
while(1)
	{
		//ADDR1寻址成功标志位
		if(I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR) == SET)
		{
			break;
		}
		if(I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == SET)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);
			return 1; // 寻址失败
		}
	}
    
    // 清除ADDR标志
    I2C_ReadRegister(I2C1, I2C_Register_SR1);
    I2C_ReadRegister(I2C1, I2C_Register_SR2);
    
    // 4. 发送寄存器地址
    while(I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE) == RESET);
    I2C_SendData(I2C1, ReadAddr);
    while(I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE) == RESET);
    
    // 5. 发送重复起始条件（不是停止条件！）
    I2C_GenerateSTART(I2C1, ENABLE);
    while(I2C_GetFlagStatus(I2C1, I2C_FLAG_SB) == RESET);
    
    // 6. 发送设备地址（读模式）
    I2C_ClearFlag(I2C1, I2C_FLAG_AF);
    I2C_SendData(I2C1,  (DevAddr << 1) | 1);
    
    while(1) {
        if(I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR) == SET) break;
        if(I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == SET) {
            I2C_GenerateSTOP(I2C1, ENABLE);
            return 2; // 寻址失败
        }
    }
    
		if(Size == 1)
			{
				// 清除ADDR
				I2C_ReadRegister(I2C1, I2C_Register_SR1);
				I2C_ReadRegister(I2C1, I2C_Register_SR2);
				
				// ACK=0 STOP=1，清除ADDR后立即关闭ACK并发送STOP
				I2C_AcknowledgeConfig(I2C1, DISABLE);
				I2C_GenerateSTOP(I2C1, ENABLE);
				
				// RxNE -> 1，等待数据接收
				while(I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) == RESET);
				
				// 读取数据
				pBuffer[0] = I2C_ReceiveData(I2C1);
			}
			else if(Size == 2)
			{
				// 清除ADDR标志位
				I2C_ReadRegister(I2C1, I2C_Register_SR1);
				I2C_ReadRegister(I2C1, I2C_Register_SR2);
				
				// ACK=1
				I2C_AcknowledgeConfig(I2C1, ENABLE);
				
				// 等待接收完成
				while(I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) == RESET);
				
				// 读取第一个字节
				pBuffer[0] = I2C_ReceiveData(I2C1);
				
				// ACK=0 STOP=1
				I2C_AcknowledgeConfig(I2C1, DISABLE);
				I2C_GenerateSTOP(I2C1, ENABLE);
				
				// 等待接收完成
				while(I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) == RESET);
				
				// 读取第二个字节
				pBuffer[1] = I2C_ReceiveData(I2C1);
			}
			else
			{
				// 清除ADDR标志位
				I2C_ReadRegister(I2C1, I2C_Register_SR1);
				I2C_ReadRegister(I2C1, I2C_Register_SR2);
				
				// ACK=1
				I2C_AcknowledgeConfig(I2C1, ENABLE);
				
				for(uint16_t i=0; i<Size-1; i++)
				{
					// 等待接收完成
					while(I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) == RESET);
					
					// 读取数据
					pBuffer[i] = I2C_ReceiveData(I2C1);
				}
				
				// ACK=0 STOP=1， 最后一字节前关闭ACK并发送STOP
				I2C_AcknowledgeConfig(I2C1, DISABLE);
				I2C_GenerateSTOP(I2C1, ENABLE);
				
				// 等待接收完成
				while(I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) == RESET);
				
				// 读取最后一个数据
				pBuffer[Size-1] = I2C_ReceiveData(I2C1);
			}
			
			return 0; // 接收成功
}

//标准读取流程
uint8_t h_I2C_ReceiveBytes(uint8_t Addr, uint8_t *pBuffer, uint8_t Size)
{
		// #1. 发送起始位
	I2C_GenerateSTART(I2C1, ENABLE);
	
	while(I2C_GetFlagStatus(I2C1, I2C_FLAG_SB) == RESET); 
	
	// #2. 寻址阶段（读模式）
	I2C_ClearFlag(I2C1, I2C_FLAG_AF);
	
	I2C_SendData(I2C1, (Addr << 1) | 1);
	
	while(1)
	{
		if(I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == SET)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);
			return 1; // 寻址失败
		}
		
		if(I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR) == SET)
		{
			break;
		}
	}
	
	// #3. 接收数据
	if(Size == 1)
	{
		// 清除ADDR
		I2C_ReadRegister(I2C1, I2C_Register_SR1);
		I2C_ReadRegister(I2C1, I2C_Register_SR2);
		
		// ACK=0 STOP=1，清除ADDR后立即关闭ACK并发送STOP
		I2C_AcknowledgeConfig(I2C1, DISABLE);
		I2C_GenerateSTOP(I2C1, ENABLE);
		
		// RxNE -> 1，等待数据接收
		while(I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) == RESET);
		
		// 读取数据
		pBuffer[0] = I2C_ReceiveData(I2C1);
	}
	else if(Size == 2)
	{
		// 清除ADDR标志位
		I2C_ReadRegister(I2C1, I2C_Register_SR1);
		I2C_ReadRegister(I2C1, I2C_Register_SR2);
		
		// ACK=1
		I2C_AcknowledgeConfig(I2C1, ENABLE);
		
		// 等待接收完成
		while(I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) == RESET);
		
		// 读取第一个字节
		pBuffer[0] = I2C_ReceiveData(I2C1);
		
		// ACK=0 STOP=1
		I2C_AcknowledgeConfig(I2C1, DISABLE);
		I2C_GenerateSTOP(I2C1, ENABLE);
		
		// 等待接收完成
		while(I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) == RESET);
		
		// 读取第二个字节
		pBuffer[1] = I2C_ReceiveData(I2C1);
	}
	else
	{
		// 清除ADDR标志位
		I2C_ReadRegister(I2C1, I2C_Register_SR1);
		I2C_ReadRegister(I2C1, I2C_Register_SR2);
    
		// ACK=1
		I2C_AcknowledgeConfig(I2C1, ENABLE);
		
		for(uint16_t i=0; i<Size-1; i++)
		{
			// 等待接收完成
			while(I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) == RESET);
			
			// 读取数据
			pBuffer[i] = I2C_ReceiveData(I2C1);
		}
		
		// ACK=0 STOP=1， 最后一字节前关闭ACK并发送STOP
		I2C_AcknowledgeConfig(I2C1, DISABLE);
		I2C_GenerateSTOP(I2C1, ENABLE);
		
		// 等待接收完成
		while(I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) == RESET);
		
		// 读取最后一个数据
		pBuffer[Size-1] = I2C_ReceiveData(I2C1);
	}
	
	return 0; // 接收成功

}

//江协OLED缓存区专用函数
uint8_t OLED_WriteData(uint8_t *Data, uint8_t Count)
{
	uint8_t i;
	// #1. 总线空闲检测，BUSY总线忙标志位，0总线空闲，1总线忙
	while(I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) == SET);
	
	// #2. 发送起始位1函数，传参
	I2C_GenerateSTART(I2C1, ENABLE);
	
	//SB标志位，判断起始位是否发送完成，0起始位未发送，1起始位发送完成
	while(I2C_GetFlagStatus(I2C1, I2C_FLAG_SB) == RESET);
	
	// #3. 寻址阶段（写模式）
	
	//清除AF应答失败标志位
	I2C_ClearFlag(I2C1, I2C_FLAG_AF);
	
	//从机地址为7位，确保最低位为0，写模式
	I2C_SendData(I2C1, 0x78);

	while(1)
	{
		//ADDR1寻址成功标志位
		if(I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR) == SET)
		{
			break;
		}
		if(I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == SET)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);
			return 3; // 寻址失败
		}
	}
	
	// 清除ADDR标志位，强制
	I2C_ReadRegister(I2C1, I2C_Register_SR1);
	I2C_ReadRegister(I2C1, I2C_Register_SR2);
			while(1)
		{
			if(I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == SET)
			{
				I2C_GenerateSTOP(I2C1, ENABLE);
				return 2; // 数据被拒收
			}
			
			//TXE标志位判断发送寄存器是否为空	，1为空
			if(I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE) == SET)
			{
				break;
			}
		}
		//发送控制字节 0x40
		while(1)
		{
			if(I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == SET)
			{
				I2C_GenerateSTOP(I2C1, ENABLE);
				return 2; // 数据被拒收
			}
			
			//TXE标志位判断发送寄存器是否为空	，1为空
			if(I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE) == SET)
			{
				break;
			}
		}
		I2C_SendData(I2C1, 0x40);
		
		//发送多个字节
		for(i = 0; i<Count; i++)
	{
		while(1)
		{
			if(I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == SET)
			{
					I2C_GenerateSTOP(I2C1, ENABLE);
					return 2; // 数据被拒收
				}
			
				//TXE标志位判断发送寄存器是否为空	，1为空
				if(I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE) == SET)
					{
					break;
				}
		}
		I2C_SendData(I2C1, Data[i]);
		
	}
	//判断数据是否发送成功
	while(1)
	{
		if(I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == SET)
		{
				I2C_GenerateSTOP(I2C1, ENABLE);
				return 1; // 数据被拒收			
		}
		
		// #5. 结束传输
		//BTF标志位判断发送和移位寄存器是否为空，1为空
		if(I2C_GetFlagStatus(I2C1, I2C_FLAG_BTF) == SET)
		{
			break;
		}
	}
	
	//5.1 发送完成，发送停止位
	I2C_GenerateSTOP(I2C1, ENABLE);
	return 0; // 成功
}

void OLED_WriteCommand(uint8_t Command){
	
	 uint8_t bytesToSend[] = {0x00, Command};
	 h_I2C_SendBytes(0x78, bytesToSend, 2);
}



















