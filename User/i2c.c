#include "i2c.h"
#include "delay.h"    
#include "usart.h" 
#include "stdio.h"
/**
  * 函    数：I2C写SCL引脚电平
  * 参    数：BitValue 协议层传入的当前需要写入SCL的电平，范围0~1
  * 返 回 值：无
  * 注意事项：此函数需要用户实现内容，当BitValue为0时，需要置SCL为低电平，当BitValue为1时，需要置SCL为高电平
  */
void MyI2C_W_SCL(uint8_t BitValue)//参数BitValue值0或1
{
    GPIO_WriteBit(GPIOB, IIC_SCL_Pin, (BitAction)BitValue);		//根据BitValue，设置SCL引脚的电平	，
	/* 如果 BitValue 是 0，强制转换为 BitAction 后就是 Bit_RESET（0），正确设置低电平。
		如果 BitValue 是 1，强制转换为 BitAction 后就是 Bit_SET（1），正确设置高电平。	*/								
}

/**
  * 函    数：I2C写SDA引脚电平
  * 参    数：BitValue 协议层传入的当前需要写入SDA的电平，范围0~0xFF
  * 返 回 值：无
  * 注意事项：此函数需要用户实现内容，当BitValue为0时，需要置SDA为低电平，当BitValue非0时，需要置SDA为高电平
  */
void MyI2C_W_SDA(uint8_t BitValue)
{
    GPIO_WriteBit(GPIOB, IIC_SDA_Pin, (BitAction)BitValue);		//根据BitValue，设置SDA引脚的电平，BitValue要实现非0即1的特性											//延时10us，防止时序频率超过要求
}

/**
  * 函    数：I2C读SDA引脚电平
  * 参    数：无
  * 返 回 值：协议层需要得到的当前SDA的电平，范围0~1
  * 注意事项：此函数需要用户实现内容，当前SDA为低电平时，返回0，当前SDA为高电平时，返回1
  */
uint8_t MyI2C_R_SDA(void)
{
   GPIO_InitTypeDef GPIO_InitStructure;
    uint8_t BitValue;
    
    // 临时将SDA配置为输入模式
    GPIO_InitStructure.GPIO_Pin = IIC_SDA_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; // 上拉输入
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    Delay_us(2); // 短暂延时稳定电平
    BitValue = GPIO_ReadInputDataBit(GPIOB, IIC_SDA_Pin);
    
    // 恢复为输出开漏模式
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    return BitValue;
	
}

/**
  * 函    数：I2C初始化
  * 参    数：无
  * 返 回 值：无
  * 注意事项：此函数需要用户实现内容，实现SCL和SDA引脚的初始化
  */
void MyI2C_Init(void)
{
    /*开启时钟*/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);	//开启GPIOB的时钟

    /*GPIO初始化*/
    GPIO_InitTypeDef GPIO_InitStructure;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
		GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;

    GPIO_InitStructure.GPIO_Pin = IIC_SDA_Pin | IIC_SCL_Pin;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);					

    /*设置默认高电平（空闲状态）*/
    GPIO_SetBits(GPIOB, IIC_SDA_Pin | IIC_SCL_Pin);		


}

/*协议层*/

/**
  * 函    数：I2C起始
  * 参    数：无
  * 返 回 值：无
  */
void MyI2C_Start(void)
{
    MyI2C_W_SDA(1);							//释放SDA，确保SDA为高电平
    MyI2C_W_SCL(1);							//释放SCL，确保SCL为高电平
    Delay_us(4);
    MyI2C_W_SDA(0);							//在SCL高电平期间，拉低SDA，产生起始信号
    Delay_us(4);
    MyI2C_W_SCL(0);							//起始后把SCL也拉低，即为了占用总线，也为了方便总线时序的拼接
    Delay_us(4);
}

/**
  * 函    数：I2C终止
  * 参    数：无
  * 返 回 值：无
  */
void MyI2C_Stop(void)
{
    MyI2C_W_SCL(0);
    MyI2C_W_SDA(0);							//拉低SDA，确保SDA为低电平
		Delay_us(4);
    MyI2C_W_SCL(1);							//释放SCL，使SCL呈现高电平
    Delay_us(4);
    MyI2C_W_SDA(1);							//在SCL高电平期间，释放SDA，产生终止信号
    Delay_us(4);
}

/**
  * 函    数：I2C发送一个字节
  * 参    数：Byte 要发送的一个字节数据，范围：0x00~0xFF
  * 返 回 值：无
  */
void MyI2C_SendByte(uint8_t Byte)
{		
		MyI2C_W_SCL(0);
    uint8_t i;
    for (i = 0; i < 8; i ++)				//循环8次，主机依次发送数据的每一位  //1111000(0x78) & 10000000(0x80)
    {
        MyI2C_W_SDA(Byte & (0x80 >> i));	//使用掩码的方式取出Byte的指定一位数据并写入到SDA线
        MyI2C_W_SCL(1);						//释放SCL，从机在SCL高电平期间读取SDA
        Delay_us(4);
        MyI2C_W_SCL(0);						//拉低SCL，主机开始发送下一位数据
    }
}

/**
  * 函    数：I2C接收一个字节
  * 参    数：无
  * 返 回 值：接收到的一个字节数据，范围：0x00~0xFF
  */
uint8_t MyI2C_ReceiveByte(uint8_t Ack)
{
    uint8_t i, Byte = 0x00;					//初始化定义接收的数据，并赋初值0x00，此处必须赋初值0x00，后面会用到

    for (i = 0; i < 8; i ++)				//循环8次，主机依次接收数据的每一位
    {		
				MyI2C_W_SCL(0);
				MyI2C_W_SDA(1);							//接收前，主机先确保释放SDA，避免干扰从机的数据发送
				Delay_us(2);
			
        MyI2C_W_SCL(1);						//释放SCL，主机机在SCL高电平期间读取SDA
        Delay_us(2);
        if (MyI2C_R_SDA() == 1) 
					{
            Byte |= (0x80 >> i);   //读取SDA数据，并存储到Byte变量
					}

    }
		MyI2C_W_SCL(0);
		MyI2C_W_SDA(!Ack);  // 如果Ack=1，发送0（ACK）；如果Ack=0，发送1（NACK）
		Delay_us(2);
		MyI2C_W_SCL(1);
		Delay_us(2);
		MyI2C_W_SCL(0); // SCL拉低，结束ACK
    return Byte;							//返回接收到的一个字节数据
}

//接收多个字节



/**  ！！！！！没有用到！！！！！！！！！
  * 函    数：I2C发送应答位
  * 参    数：Byte 要发送的应答位，范围：0~1，0表示应答，1表示非应答
  * 返 回 值：无
  */
void MyI2C_SendAck(uint8_t AckBit)
{		
		MyI2C_W_SCL(0);
		Delay_us(2);
    MyI2C_W_SDA(AckBit);				//主机把应答位数据放到SDA线
    MyI2C_W_SCL(1);							//释放SCL，从机在SCL高电平期间，读取应答位
		Delay_us(2);
    MyI2C_W_SCL(0);							//拉低SCL，开始下一个时序模块
}

/**
  * 函    数：I2C接收应答位
  * 参    数：无
  * 返 回 值：接收到的应答位，范围：0~1，0表示应答，1表示非应答
  */
uint8_t MyI2C_ReceiveAck(void)
{
    uint8_t AckBit;							//定义应答位变量
    MyI2C_W_SDA(1);							//接收前，主机先确保释放SDA，避免干扰从机的数据发送
    Delay_us(2);
		MyI2C_W_SCL(1);							//释放SCL，主机机在SCL高电平期间读取SDA
		Delay_us(2);  
		AckBit = MyI2C_R_SDA();			//将应答位存储到变量里
    MyI2C_W_SCL(0);							//拉低SCL，开始下一个时序模块
    return AckBit;							//返回定义应答位变量
}

// 发送多个字节到指定设备地址
// 参数：Addr - 设备地址（7位地址，左对齐，最低位为0表示写）
//        pData - 数据指针
//        Size - 数据大小
// 返回0成功，负数失败（-1: 地址ACK错误, -2: 数据ACK错误）
int My_SI2C_SendBytes(uint8_t Addr, uint8_t *pData, uint16_t Size)
{
    MyI2C_Start(); // 发送开始条件
	
    // 发送设备地址（写模式：Addr & 0xFE 清除最低位，表示写）
		MyI2C_SendByte(Addr & 0xfe) ;
    
	
    for(uint32_t i=0; i<Size; i++) // 循环发送每个字节
    {
        MyI2C_SendByte(pData[i]) ;
        
    }
	
    MyI2C_Stop(); // 发送停止条件
    return 0; // 成功
}

// 从指定设备地址接收多个字节
// 参数：Addr - 设备地址（7位地址，左对齐，最低位为1表示读）
//        pBuffer - 接收缓冲区指针
//        Size - 要接收的字节数
// 返回0成功，-1失败（地址ACK错误）
int My_II2C_ReceiveBytes(uint8_t Addr, uint8_t *pBuffer, uint16_t Size)
{
    MyI2C_Start(); // 发送开始条件
	
    // 发送设备地址（读模式：Addr | 0x01 设置最低位，表示读）
		MyI2C_SendByte(Addr | 0x01) ;
    
    // 接收数据：前Size-1个字节发送ACK（表示继续接收），最后一个字节发送NACK（表示停止）
    for(uint32_t i=0; i<Size-1; i++)
    {
        pBuffer[i] = MyI2C_ReceiveByte(1); // 接收字节并发送ACK
    }
    pBuffer[Size-1] = MyI2C_ReceiveByte(0); // 接收最后一个字节并发送NACK
	
    MyI2C_Stop();  // 发送停止条件
    return 0;
}


