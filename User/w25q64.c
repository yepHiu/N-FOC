//
// Created by yephi on 12/5/2024.
//

#include "w25q64.h"

/****
	***********************************************************************************************************************************************************************************
	*	@version V1.0
	*  @date    2023-2-1
	*	@author  反客科技
   ************************************************************************************************************************************************************************************
   *  @description
	*
	*	实验平台：反客STM32H723ZGT6核心板 （型号：FK723M1-ZGT6）
	*	淘宝地址：https://shop212360197.taobao.com
	*	QQ交流群：536665479
	*
>>>>> 文件说明：
	*
	*  1.例程参考于官方评估板 STM32H7B3I_EVAL board(MB1331) 的驱动
	*	2.例程使用的是 OSPI1
	*	3.提供的读写函数均使用HAL库函数直接操作，没有用到DMA和中断
	*	4.默认配置OSPI驱动时钟为内核时钟的2分频
	*
>>>>> 重要说明：
	*
	*	1.W25QXX的擦除时间是限定的!!! 手册给出的典型参考值为: 4K-45ms, 32K-120ms ,64K-150ms,整片擦除20S
	*
	*	2.W25QXX的写入时间是限定的!!! 手册给出的典型参考值为: 256字节-0.4ms，也就是 1M字节/s （实测大概在600K字节/s左右）
	*
	*	3.如果使用库函数直接读取，那么是否使用DMA、是否开启Cache、编译器的优化等级以及数据存储区的位置(内部 TCM SRAM 或者 AXI SRAM)都会影响读取的速度
	*
	*	4.如果使用内存映射模式，则读取性能只与OSPI的驱动时钟以及是否开启Cache有关
	*
	*	5.实际使用中，当数据比较大时，建议使用64K或者32K擦除，擦除时间比4K擦除块
	*
	**************************************************************************************************************************************************************************************FANKE*****
***/

#include "w25q64.h"
#include "stdio.h"
#include "stm32h7xx.h"
#include "octospi.h"

#define W25Qxx_NumByteToTest   	32*1024					// 测试数据的长度，32K

int32_t OSPI_Status ; 		 //检测标志位

uint8_t  W25Qxx_WriteBuffer[W25Qxx_NumByteToTest];		//	写数据数组
uint8_t  W25Qxx_ReadBuffer[W25Qxx_NumByteToTest];		//	读数据数组

//OSPI_HandleTypeDef hospi1;


void HAL_OSPI_MspInit_(OSPI_HandleTypeDef* hospi)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if(hospi->Instance==OCTOSPI1)
    {
        __HAL_RCC_OCTOSPIM_CLK_ENABLE();		// 使能OSPI时钟
        __HAL_RCC_OSPI1_CLK_ENABLE();

        __HAL_RCC_OSPI1_FORCE_RESET();		// 复位QSPI
        __HAL_RCC_OSPI1_RELEASE_RESET();

        __HAL_RCC_GPIOF_CLK_ENABLE();		// 使能 对应的 IO口时钟
        __HAL_RCC_GPIOG_CLK_ENABLE();

        /****引脚定义********************************************

           PF6     ------> OCTOSPIM_P1_IO3
           PF7     ------> OCTOSPIM_P1_IO2
           PF8     ------> OCTOSPIM_P1_IO0
           PF9     ------> OCTOSPIM_P1_IO1
           PF10    ------> OCTOSPIM_P1_CLK
           PG6     ------> OCTOSPIM_P1_NCS
        *******************************************************/

        GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OCTOSPIM_P1;
        HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF9_OCTOSPIM_P1;
        HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_6;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OCTOSPIM_P1;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    }
}

void MX_OCTOSPI1_Init_(void)
{
    OSPIM_CfgTypeDef sOspiManagerCfg = {0};

    uint32_t i;
    char *p;

    /* 此处参考了安富莱的代码，大家可以去学习安富莱的教程，非常专业 */
    p = (char *)&hospi1;
    for (i = 0; i < sizeof(hospi1); i++)
    {
        *p++ = 0;
    }
    hospi1.Instance  = OCTOSPI1;
    HAL_OSPI_DeInit(&hospi1);           // 复位OSPI

    hospi1.Instance 							= OCTOSPI1;								// OSPI外设

    hospi1.Init.ClockPrescaler 			= 2;                                      // 时钟分频值，将OSPI内核时钟进行 2 分频得到OSPI通信驱动时钟
    hospi1.Init.FifoThreshold 				= 8;                                     	// FIFO阈值
    hospi1.Init.DualQuad 					= HAL_OSPI_DUALQUAD_DISABLE;              // 禁止双OSPI模式
    hospi1.Init.MemoryType 					= HAL_OSPI_MEMTYPE_MICRON;                // 存储器模式，只有8线模式才会用到
    hospi1.Init.DeviceSize 					= 23;					                        // flash大小，核心板采用是8M字节的W25Q64，这里设置为23，即2^23
    hospi1.Init.ChipSelectHighTime 		= 1;                                      // 片选保持高电平的时间
    hospi1.Init.FreeRunningClock 			= HAL_OSPI_FREERUNCLK_DISABLE;            // 禁止自由时钟模式
    hospi1.Init.ClockMode 					= HAL_OSPI_CLOCK_MODE_3;                  // 模式3
    hospi1.Init.WrapSize						= HAL_OSPI_WRAP_NOT_SUPPORTED;            // 不使用 wrap-size
    hospi1.Init.SampleShifting 			= HAL_OSPI_SAMPLE_SHIFTING_HALFCYCLE;     // 半个CLK周期之后进行采样
    hospi1.Init.DelayHoldQuarterCycle	= HAL_OSPI_DHQC_DISABLE;                  // 不使用数据保持功能
    hospi1.Init.ChipSelectBoundary 		= 0;											      // 禁止片选边界功能
    hospi1.Init.DelayBlockBypass 			= HAL_OSPI_DELAY_BLOCK_BYPASSED;          // 延时模块旁路
    hospi1.Init.MaxTran                 = 0;                                      // 禁止通信管理功能
    hospi1.Init.Refresh                 = 0;                                      // 禁止刷新功能

    HAL_OSPI_Init(&hospi1);    // 初始化 OSPI1 设置

    sOspiManagerCfg.ClkPort = 1;     // OSPI引脚分配管理器设置，使用 Port1 的 CLK
    sOspiManagerCfg.NCSPort = 1;     // OSPI引脚分配管理器设置，使用 Port1 的 NCS
    sOspiManagerCfg.IOLowPort = HAL_OSPIM_IOPORT_1_LOW;   // OSPI引脚分配管理器设置，使用 Port1 的低4位引脚，IO[3:0]

    HAL_OSPIM_Config(&hospi1, &sOspiManagerCfg, HAL_OSPI_TIMEOUT_DEFAULT_VALUE);  // 初始化OSPI引脚分配管理器设置
}


/**
 * @brief W25Q64 Init
 * @return
 */
int8_t OSPI_W25Qxx_Init(void)
{
    uint32_t	Device_ID;	// 器件ID

    //MX_OCTOSPI1_Init();							// 初始化 OSPI 配置

    Device_ID = OSPI_W25Qxx_ReadID(); 		// 读取器件ID

    if( Device_ID == W25Qxx_FLASH_ID )		// 进行匹配
    {
        printf ("W25Q64 OK,flash ID:%X\r\n",Device_ID);		// 初始化成功
        return OSPI_W25Qxx_OK;			// 返回成功标志
    }
    else
    {
        printf ("W25Q64 ERROR!!!!!  ID:%X\r\n",Device_ID);	// 初始化失败
        return W25Qxx_ERROR_INIT;		// 返回错误标志
    }
}

/*************************************************************************************************
*	函 数 名: OSPI_W25Qxx_AutoPollingMemReady
*	入口参数: 无
*	返 回 值: OSPI_W25Qxx_OK - 通信正常结束，W25Qxx_ERROR_AUTOPOLLING - 轮询等待无响应
*	函数功能: 使用自动轮询标志查询，等待通信结束
*	说    明: 每一次通信都应该调用此函数，等待通信结束，避免错误的操作
******************************************************************************************FANKE*****/

int8_t OSPI_W25Qxx_AutoPollingMemReady(void)
{
    OSPI_RegularCmdTypeDef  sCommand;		// OSPI传输配置
    OSPI_AutoPollingTypeDef sConfig;			// 轮询比较相关配置参数

    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;         // 通用配置
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;                // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;        // 1线指令模式
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;        // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;   // 禁止指令DTR模式
    sCommand.Address            = 0x0;                                // 地址0
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;              // 无地址模式
    sCommand.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;           // 地址长度24位
    sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;       // 禁止地址DTR模式
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;      //	无交替字节
    sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;               // 1线数据模式
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;          // 禁止数据DTR模式
    sCommand.NbData             = 1;                                  // 通信数据长度
    sCommand.DummyCycles        = 0;                                  // 空周期个数
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;               // 不使用DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;       // 每次传输数据都发送指令

    sCommand.Instruction        = W25Qxx_CMD_ReadStatus_REG1;         // 读状态信息寄存器

    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // 轮询等待无响应
    }

// 不停的查询 W25Qxx_CMD_ReadStatus_REG1 寄存器，将读取到的状态字节中的 W25Qxx_Status_REG1_BUSY 不停的与0作比较
// 读状态寄存器1的第0位（只读），Busy标志位，当正在擦除/写入数据/写命令时会被置1，空闲或通信结束为0
// FANKE
    sConfig.Match         = 0;											//	匹配值
    sConfig.MatchMode     = HAL_OSPI_MATCH_MODE_AND;			//	与运算
    sConfig.Interval      = 0x10;										//	轮询间隔
    sConfig.AutomaticStop = HAL_OSPI_AUTOMATIC_STOP_ENABLE;	// 自动停止模式
    sConfig.Mask          = W25Qxx_Status_REG1_BUSY; 			// 对在轮询模式下接收的状态字节进行屏蔽，只比较需要用到的位

    // 发送轮询等待命令
    if (HAL_OSPI_AutoPolling(&hospi1, &sConfig,HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // 轮询等待无响应
    }
    return OSPI_W25Qxx_OK; // 通信正常结束
}

/*************************************************************************************************
*	函 数 名: OSPI_W25Qxx_ReadID
*	入口参数: 无
*	返 回 值: W25Qxx_ID - 读取到的器件ID，W25Qxx_ERROR_INIT - 通信、初始化错误
*	函数功能: 初始化 OSPI 配置，读取器件ID
*	说    明: 无
**************************************************************************************************/

uint32_t OSPI_W25Qxx_ReadID(void)
{
    OSPI_RegularCmdTypeDef  sCommand;		// OSPI传输配置

    uint8_t	OSPI_ReceiveBuff[3];		      // 存储OSPI读到的数据
    uint32_t	W25Qxx_ID;					      // 器件的ID

    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;         // 通用配置
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;                // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;        // 1线指令模式
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;        // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;   // 禁止指令DTR模式
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;              // 无地址模式
    sCommand.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;           // 地址长度24位
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;      //	无交替字节
    sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;               // 1线数据模式
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;          // 禁止数据DTR模式
    sCommand.NbData             = 3;                                  // 传输数据的长度
    sCommand.DummyCycles        = 0;                                  // 空周期个数
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;               // 不使用DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;       // 每次传输数据都发送指令

    sCommand.Instruction        = W25Qxx_CMD_JedecID;                 // 执行读器件ID命令


    HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE);	// 发送指令

    HAL_OSPI_Receive (&hospi1, OSPI_ReceiveBuff, HAL_OSPI_TIMEOUT_DEFAULT_VALUE);	// 接收数据

    W25Qxx_ID = (OSPI_ReceiveBuff[0] << 16) | (OSPI_ReceiveBuff[1] << 8 ) | OSPI_ReceiveBuff[2];	// 将得到的数据组合成ID

    return W25Qxx_ID; // 返回ID
}


/*************************************************************************************************
*	函 数 名: OSPI_W25Qxx_MemoryMappedMode
*	入口参数: 无
*	返 回 值: OSPI_W25Qxx_OK - 写使能成功，W25Qxx_ERROR_WriteEnable - 写使能失败
*	函数功能: 将OSPI设置为内存映射模式
*	说    明: 无
**************************************************************************************************/

int8_t OSPI_W25Qxx_MemoryMappedMode(void)
{
    OSPI_RegularCmdTypeDef     sCommand;         // QSPI传输配置
    OSPI_MemoryMappedTypeDef   sMemMappedCfg;    // 内存映射访问参数

    sCommand.OperationType           = HAL_OSPI_OPTYPE_COMMON_CFG;             // 通用配置
    sCommand.FlashId                 = HAL_OSPI_FLASH_ID_1;                    // flash ID

    sCommand.Instruction             = W25Qxx_CMD_FastReadQuad_IO;             // 1-4-4模式下(1线指令4线地址4线数据)，快速读取指令
    sCommand.InstructionMode         = HAL_OSPI_INSTRUCTION_1_LINE;            // 1线指令模式
    sCommand.InstructionSize         = HAL_OSPI_INSTRUCTION_8_BITS;            // 指令长度8位
    sCommand.InstructionDtrMode      = HAL_OSPI_INSTRUCTION_DTR_DISABLE;       // 禁止指令DTR模式

    sCommand.AddressMode             = HAL_OSPI_ADDRESS_4_LINES;               // 4线地址模式
    sCommand.AddressSize             = HAL_OSPI_ADDRESS_24_BITS;               // 地址长度24位
    sCommand.AddressDtrMode          = HAL_OSPI_ADDRESS_DTR_DISABLE;           // 禁止地址DTR模式

    sCommand.AlternateBytesMode      = HAL_OSPI_ALTERNATE_BYTES_NONE;          // 无交替字节
    sCommand.AlternateBytesDtrMode   = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;   // 禁止替字节DTR模式

    sCommand.DataMode                = HAL_OSPI_DATA_4_LINES;                  // 4线数据模式
    sCommand.DataDtrMode             = HAL_OSPI_DATA_DTR_DISABLE;              // 禁止数据DTR模式

    sCommand.DummyCycles             = 6;                                      // 空周期个数
    sCommand.DQSMode                 = HAL_OSPI_DQS_DISABLE;                   // 不使用DQS
    sCommand.SIOOMode                = HAL_OSPI_SIOO_INST_EVERY_CMD;           // 每次传输数据都发送指令

    // 写配置
    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_TRANSMIT;		// 传输数据错误
    }

    sMemMappedCfg.TimeOutActivation  = HAL_OSPI_TIMEOUT_COUNTER_DISABLE;          // 禁用超时计数器, nCS 保持激活状态
    sMemMappedCfg.TimeOutPeriod      = 0;                                         // 超时判断周期
    // 开启内存映射模式
    if (HAL_OSPI_MemoryMapped(&hospi1,  &sMemMappedCfg) != HAL_OK)	// 进行配置
    {
        return W25Qxx_ERROR_MemoryMapped; 	// 设置内存映射模式错误
    }
    return OSPI_W25Qxx_OK; // 配置成功
}

/*************************************************************************************************
*	函 数 名: OSPI_W25Qxx_WriteEnable
*	入口参数: 无
*	返 回 值: OSPI_W25Qxx_OK - 写使能成功，W25Qxx_ERROR_WriteEnable - 写使能失败
*	函数功能: 发送写使能命令
*	说    明: 无
**************************************************************************************************/

int8_t OSPI_W25Qxx_WriteEnable(void)
{
    OSPI_RegularCmdTypeDef  sCommand;// OSPI传输配置
    OSPI_AutoPollingTypeDef sConfig;// 轮询比较相关配置参数

    sCommand.OperationType           = HAL_OSPI_OPTYPE_COMMON_CFG;             // 通用配置
    sCommand.FlashId                 = HAL_OSPI_FLASH_ID_1;                    // flash ID
    sCommand.InstructionMode         = HAL_OSPI_INSTRUCTION_1_LINE;            // 1线指令模式
    sCommand.InstructionSize         = HAL_OSPI_INSTRUCTION_8_BITS;            // 指令长度8位
    sCommand.InstructionDtrMode      = HAL_OSPI_INSTRUCTION_DTR_DISABLE;       // 禁止指令DTR模式
    sCommand.Address                 = 0;                                      // 地址0
    sCommand.AddressMode             = HAL_OSPI_ADDRESS_NONE;                  // 无地址模式
    sCommand.AddressSize             = HAL_OSPI_ADDRESS_24_BITS;               // 地址长度24位
    sCommand.AddressDtrMode          = HAL_OSPI_ADDRESS_DTR_DISABLE;           // 禁止地址DTR模式
    sCommand.AlternateBytesDtrMode   = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;   //	禁止替字节DTR模式
    sCommand.AlternateBytesMode      = HAL_OSPI_ALTERNATE_BYTES_NONE;          //	无交替字节
    sCommand.DataMode                = HAL_OSPI_DATA_NONE;                     // 无数据模式
    sCommand.DataDtrMode             = HAL_OSPI_DATA_DTR_DISABLE;              // 禁止数据DTR模式
    sCommand.DummyCycles             = 0;                                      // 空周期个数
    sCommand.DQSMode                 = HAL_OSPI_DQS_DISABLE;                   // 不使用DQS
    sCommand.SIOOMode                = HAL_OSPI_SIOO_INST_EVERY_CMD;           // 每次传输数据都发送指令

    sCommand.Instruction             = W25Qxx_CMD_WriteEnable;                 // 写使能命令

    // 发送写使能命令
    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_WriteEnable;
    }
    // 发送查询状态寄存器命令
    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;         // 通用配置
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;                // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;        // 1线指令模式
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;        // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;   // 禁止指令DTR模式
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;              // 无地址模式
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;      //	无交替字节
    sCommand.DummyCycles        = 0;                                  // 空周期个数
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;               // 不使用DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;       // 每次传输数据都发送指令
    sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;               // 1线数据模式
    sCommand.NbData             = 1;                                  // 通信数据长度

    sCommand.Instruction        = W25Qxx_CMD_ReadStatus_REG1;         // 查询状态寄存器命令

    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_WriteEnable;
    }

// 不停的查询 W25Qxx_CMD_ReadStatus_REG1 寄存器，将读取到的状态字节中的 W25Qxx_Status_REG1_WEL 不停的与 0x02 作比较
// 读状态寄存器1的第1位（只读），WEL写使能标志位，该标志位为1时，代表可以进行写操作
// FANKE	7B0
    sConfig.Match         = 0x02;										//	匹配值
    sConfig.MatchMode     = HAL_OSPI_MATCH_MODE_AND;			//	与运算
    sConfig.Interval      = 0x10;										//	轮询间隔
    sConfig.AutomaticStop = HAL_OSPI_AUTOMATIC_STOP_ENABLE;	// 自动停止模式
    sConfig.Mask          = W25Qxx_Status_REG1_WEL; 			// 对在轮询模式下接收的状态字节进行屏蔽，只比较需要用到的位

    if (HAL_OSPI_AutoPolling(&hospi1, &sConfig,HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // 轮询等待无响应
    }
    return OSPI_W25Qxx_OK;  // 通信正常结束
}

/*************************************************************************************************
*
*	函 数 名: OSPI_W25Qxx_SectorErase
*
*	入口参数: SectorAddress - 要擦除的地址
*
*	返 回 值: OSPI_W25Qxx_OK - 擦除成功
*			    W25Qxx_ERROR_Erase - 擦除失败
*				 W25Qxx_ERROR_AUTOPOLLING - 轮询等待无响应
*
*	函数功能: 进行扇区擦除操作，每次擦除4K字节
*
*	说    明: 1.按照 W25Q64JV 数据手册给出的擦除参考时间，典型值为 45ms，最大值为400ms
*				 2.实际的擦除速度可能大于45ms，也可能小于45ms
*				 3.flash使用的时间越长，擦除所需时间也会越长
*
**************************************************************************************************/

int8_t OSPI_W25Qxx_SectorErase(uint32_t SectorAddress)
{
    OSPI_RegularCmdTypeDef  sCommand;		// OSPI传输配置

    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;         // 通用配置
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;                // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;        // 1线指令模式
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;        // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;   // 禁止指令DTR模式
    sCommand.Address            = SectorAddress;                      // 地址
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_1_LINE;            // 1线地址模式
    sCommand.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;           // 地址长度24位
    sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;       // 禁止地址DTR模式
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;      //	无交替字节
    sCommand.DataMode           = HAL_OSPI_DATA_NONE;                 // 无数据模式
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;          // 禁止数据DTR模式
    sCommand.DummyCycles        = 0;                                  // 空周期个数
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;               // 不使用DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;       // 每次传输数据都发送指令

    sCommand.Instruction        = W25Qxx_CMD_SectorErase;             // 扇区擦除指令，每次擦除4K字节

    // 发送写使能
    if (OSPI_W25Qxx_WriteEnable() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_WriteEnable;		// 写使能失败
    }
    // 发送擦除指令
    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // 轮询等待无响应
    }
    // 使用自动轮询标志位，等待擦除的结束
    if (OSPI_W25Qxx_AutoPollingMemReady() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING;		// 轮询等待无响应
    }
    return OSPI_W25Qxx_OK; // 擦除成功
}

/*************************************************************************************************
*
*	函 数 名: OSPI_W25Qxx_BlockErase_32K
*
*	入口参数: SectorAddress - 要擦除的地址
*
*	返 回 值: OSPI_W25Qxx_OK - 擦除成功
*			    W25Qxx_ERROR_Erase - 擦除失败
*				 W25Qxx_ERROR_AUTOPOLLING - 轮询等待无响应
*
*	函数功能: 进行块擦除操作，每次擦除32K字节
*
*	说    明: 1.按照 W25Q64JV 数据手册给出的擦除参考时间，典型值为 120ms，最大值为1600ms
*				 2.实际的擦除速度可能大于120ms，也可能小于120ms
*				 3.flash使用的时间越长，擦除所需时间也会越长
*
*************************************************************************************************/

int8_t OSPI_W25Qxx_BlockErase_32K (uint32_t SectorAddress)
{
    OSPI_RegularCmdTypeDef  sCommand;		// OSPI传输配置

    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;         // 通用配置
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;                // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;        // 1线指令模式
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;        // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;   // 禁止指令DTR模式
    sCommand.Address            = SectorAddress;                      // 地址
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_1_LINE;            // 1线地址模式
    sCommand.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;           // 地址长度24位
    sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;       // 禁止地址DTR模式
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;      //	无交替字节
    sCommand.DataMode           = HAL_OSPI_DATA_NONE;                 // 无数据模式
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;          // 禁止数据DTR模式
    sCommand.DummyCycles        = 0;                                  // 空周期个数
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;               // 不使用DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;       // 每次传输数据都发送指令

    sCommand.Instruction        = W25Qxx_CMD_BlockErase_32K;          // 块擦除指令，每次擦除32K字节

    // 发送写使能
    if (OSPI_W25Qxx_WriteEnable() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_WriteEnable;		// 写使能失败
    }
    // 发送擦除指令
    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // 轮询等待无响应
    }
    // 使用自动轮询标志位，等待擦除的结束
    if (OSPI_W25Qxx_AutoPollingMemReady() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING;		// 轮询等待无响应
    }
    return OSPI_W25Qxx_OK; // 擦除成功
}

/*************************************************************************************************
*
*	函 数 名: OSPI_W25Qxx_BlockErase_64K
*
*	入口参数: SectorAddress - 要擦除的地址
*
*	返 回 值: OSPI_W25Qxx_OK - 擦除成功
*			    W25Qxx_ERROR_Erase - 擦除失败
*				 W25Qxx_ERROR_AUTOPOLLING - 轮询等待无响应
*
*	函数功能: 进行块擦除操作，每次擦除64K字节
*
*	说    明: 1.按照 W25Q64JV 数据手册给出的擦除参考时间，典型值为 150ms，最大值为2000ms
*				 2.实际的擦除速度可能大于150ms，也可能小于150ms
*				 3.flash使用的时间越长，擦除所需时间也会越长
*				 4.实际使用建议使用64K擦除，擦除的时间最快
*
**************************************************************************************************/
int8_t OSPI_W25Qxx_BlockErase_64K (uint32_t SectorAddress)
{
    OSPI_RegularCmdTypeDef  sCommand;		// OSPI传输配置

    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;         // 通用配置
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;                // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;        // 1线指令模式
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;        // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;   // 禁止指令DTR模式
    sCommand.Address            = SectorAddress;                      // 地址
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_1_LINE;            // 1线地址模式
    sCommand.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;           // 地址长度24位
    sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;       // 禁止地址DTR模式
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;      //	无交替字节
    sCommand.DataMode           = HAL_OSPI_DATA_NONE;                 // 无数据模式
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;          // 禁止数据DTR模式
    sCommand.DummyCycles        = 0;                                  // 空周期个数
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;               // 不使用DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;       // 每次传输数据都发送指令

    sCommand.Instruction        = W25Qxx_CMD_BlockErase_64K;          // 扇区擦除指令，每次擦除64K字节

    // 发送写使能
    if (OSPI_W25Qxx_WriteEnable() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_WriteEnable;		// 写使能失败
    }
    // 发送擦除指令
    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // 轮询等待无响应
    }
    // 使用自动轮询标志位，等待擦除的结束
    if (OSPI_W25Qxx_AutoPollingMemReady() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING;		// 轮询等待无响应
    }
    return OSPI_W25Qxx_OK; // 擦除成功
}

/*************************************************************************************************
*
*	函 数 名: OSPI_W25Qxx_ChipErase
*
*	入口参数: 无
*
*	返 回 值: OSPI_W25Qxx_OK - 擦除成功
*			    W25Qxx_ERROR_Erase - 擦除失败
*				 W25Qxx_ERROR_AUTOPOLLING - 轮询等待无响应
*
*	函数功能: 进行整片擦除操作
*
*	说    明: 1.按照 W25Q64JV 数据手册给出的擦除参考时间，典型值为 20s，最大值为100s
*				 2.实际的擦除速度可能大于20s，也可能小于20s
*				 3.flash使用的时间越长，擦除所需时间也会越长
*
*************************************************************************************************/
int8_t OSPI_W25Qxx_ChipErase (void)
{
    OSPI_RegularCmdTypeDef  sCommand;		// OSPI传输配置
    OSPI_AutoPollingTypeDef sConfig;       // 轮询比较相关配置参数

    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;         // 通用配置
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;                // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;        // 1线指令模式
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;        // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;   // 禁止指令DTR模式
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;              // 无地址模式
    sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;       // 禁止地址DTR模式
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;      //	无交替字节
    sCommand.DataMode           = HAL_OSPI_DATA_NONE;                 // 无数据模式
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;          // 禁止数据DTR模式
    sCommand.DummyCycles        = 0;                                  // 空周期个数
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;               // 不使用DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;       // 每次传输数据都发送指令

    sCommand.Instruction        = W25Qxx_CMD_ChipErase;               // 全片擦除指令

    // 发送写使能
    if (OSPI_W25Qxx_WriteEnable() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_WriteEnable;		// 写使能失败
    }
    // 发送擦除指令
    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // 轮询等待无响应
    }

    // 发送查询状态寄存器命令
    sCommand.DataMode       = HAL_OSPI_DATA_1_LINE;          // 一线数据模式
    sCommand.NbData         = 1;                             // 数据长度1
    sCommand.Instruction    = W25Qxx_CMD_ReadStatus_REG1;    // 状态寄存器命令

    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING;
    }

// 不停的查询 W25Qxx_CMD_ReadStatus_REG1 寄存器，将读取到的状态字节中的 W25Qxx_Status_REG1_BUSY 不停的与0作比较
// 读状态寄存器1的第0位（只读），Busy标志位，当正在擦除/写入数据/写命令时会被置1，空闲或通信结束为0

    sConfig.Match         = 0;											//	匹配值
    sConfig.MatchMode     = HAL_OSPI_MATCH_MODE_AND;			//	与运算
    sConfig.Interval      = 0x10;										//	轮询间隔
    sConfig.AutomaticStop = HAL_OSPI_AUTOMATIC_STOP_ENABLE;	// 自动停止模式
    sConfig.Mask          = W25Qxx_Status_REG1_BUSY; 			// 对在轮询模式下接收的状态字节进行屏蔽，只比较需要用到的位

    // W25Q64整片擦除的典型参考时间为20s，最大时间为100s，这里的超时等待值 W25Qxx_ChipErase_TIMEOUT_MAX 为 100S
    if (HAL_OSPI_AutoPolling(&hospi1, &sConfig,W25Qxx_ChipErase_TIMEOUT_MAX) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // 轮询等待无响应
    }
    return OSPI_W25Qxx_OK; // 擦除成功
}

/**********************************************************************************************************
*
*	函 数 名: OSPI_W25Qxx_WritePage
*
*	入口参数: pBuffer 		 - 要写入的数据
*				 WriteAddr 		 - 要写入 W25Qxx 的地址
*				 NumByteToWrite - 数据长度，最大只能256字节
*
*	返 回 值: OSPI_W25Qxx_OK 		     - 写数据成功
*			    W25Qxx_ERROR_WriteEnable - 写使能失败
*				 W25Qxx_ERROR_TRANSMIT	  - 传输失败
*				 W25Qxx_ERROR_AUTOPOLLING - 轮询等待无响应
*
*	函数功能: 按页写入，最大只能256字节，在数据写入之前，请务必完成擦除操作
*
*	说    明: 1.Flash的写入时间和擦除时间一样，是限定的，并不是说OSPI驱动时钟133M就可以以这个速度进行写入
*				 2.按照 W25Q64JV 数据手册给出的 页(256字节) 写入参考时间，典型值为 0.4ms，最大值为3ms
*				 3.实际的写入速度可能大于0.4ms，也可能小于0.4ms
*				 4.Flash使用的时间越长，写入所需时间也会越长
*				 5.在数据写入之前，请务必完成擦除操作
*
***********************************************************************************************************/
int8_t OSPI_W25Qxx_WritePage(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
    OSPI_RegularCmdTypeDef  sCommand;// OSPI传输配置

    sCommand.OperationType           = HAL_OSPI_OPTYPE_COMMON_CFG;             // 通用配置
    sCommand.FlashId                 = HAL_OSPI_FLASH_ID_1;                    // flash ID

    sCommand.Instruction             = W25Qxx_CMD_QuadInputPageProgram;        // 1-1-4模式下(1线指令1线地址4线数据)，页编程指令
    sCommand.InstructionMode         = HAL_OSPI_INSTRUCTION_1_LINE;            // 1线指令模式
    sCommand.InstructionSize         = HAL_OSPI_INSTRUCTION_8_BITS;            // 指令长度8位
    sCommand.InstructionDtrMode      = HAL_OSPI_INSTRUCTION_DTR_DISABLE;       // 禁止指令DTR模式

    sCommand.Address                 = WriteAddr;                              // 地址
    sCommand.AddressMode             = HAL_OSPI_ADDRESS_1_LINE;                // 1线地址模式
    sCommand.AddressSize             = HAL_OSPI_ADDRESS_24_BITS;               // 地址长度24位
    sCommand.AddressDtrMode          = HAL_OSPI_ADDRESS_DTR_DISABLE;           // 禁止地址DTR模式

    sCommand.AlternateBytesMode      = HAL_OSPI_ALTERNATE_BYTES_NONE;          // 无交替字节
    sCommand.AlternateBytesDtrMode   = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;   // 禁止替字节DTR模式

    sCommand.DataMode                = HAL_OSPI_DATA_4_LINES;                  // 4线数据模式
    sCommand.DataDtrMode             = HAL_OSPI_DATA_DTR_DISABLE;              // 禁止数据DTR模式
    sCommand.NbData                  = NumByteToWrite;                         // 数据长度

    sCommand.DummyCycles             = 0;                                      // 空周期个数
    sCommand.DQSMode                 = HAL_OSPI_DQS_DISABLE;                   // 不使用DQS
    sCommand.SIOOMode                = HAL_OSPI_SIOO_INST_EVERY_CMD;           // 每次传输数据都发送指令

    // 写使能
    if (OSPI_W25Qxx_WriteEnable() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_WriteEnable;	// 写使能失败
    }
    // 写命令
    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_TRANSMIT;		// 传输数据错误
    }
    // 开始传输数据
    if (HAL_OSPI_Transmit(&hospi1, pBuffer, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_TRANSMIT;		// 传输数据错误
    }
    // 使用自动轮询标志位，等待写入的结束
    if (OSPI_W25Qxx_AutoPollingMemReady() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING;		// 轮询等待无响应
    }
    return OSPI_W25Qxx_OK; // 写数据成功
}

/**********************************************************************************************************
*
*	函 数 名: OSPI_W25Qxx_WriteBuffer
*
*	入口参数: pBuffer 		 - 要写入的数据
*				 WriteAddr 		 - 要写入 W25Qxx 的地址
*				 NumByteToWrite - 数据长度，最大不能超过flash芯片的大小
*
*	返 回 值: OSPI_W25Qxx_OK 		     - 写数据成功
*			    W25Qxx_ERROR_WriteEnable - 写使能失败
*				 W25Qxx_ERROR_TRANSMIT	  - 传输失败
*				 W25Qxx_ERROR_AUTOPOLLING - 轮询等待无响应
*
*	函数功能: 写入数据，最大不能超过flash芯片的大小，请务必完成擦除操作
*
*	说    明: 1.Flash的写入时间和擦除时间一样，是有限定的，并不是说OSPI驱动时钟133M就可以以这个速度进行写入
*				 2.按照 W25Q64JV 数据手册给出的 页 写入参考时间，典型值为 0.4ms，最大值为3ms
*				 3.实际的写入速度可能大于0.4ms，也可能小于0.4ms
*				 4.Flash使用的时间越长，写入所需时间也会越长
*				 5.在数据写入之前，请务必完成擦除操作
*				 6.该函数移植于 stm32h743i_eval_qspi.c
*
**********************************************************************************************************/

int8_t OSPI_W25Qxx_WriteBuffer(uint8_t* pBuffer, uint32_t WriteAddr, uint32_t Size)
{
    uint32_t end_addr, current_size, current_addr;
    uint8_t *write_data;  // 要写入的数据

    current_size = W25Qxx_PageSize - (WriteAddr % W25Qxx_PageSize); // 计算当前页还剩余的空间

    if (current_size > Size)	// 判断当前页剩余的空间是否足够写入所有数据
    {
        current_size = Size;		// 如果足够，则直接获取当前长度
    }

    current_addr = WriteAddr;		// 获取要写入的地址
    end_addr = WriteAddr + Size;	// 计算结束地址
    write_data = pBuffer;			// 获取要写入的数据

    do
    {
        // 按页写入数据
        if(OSPI_W25Qxx_WritePage(write_data, current_addr, current_size) != OSPI_W25Qxx_OK)
        {
            return W25Qxx_ERROR_TRANSMIT;
        }

        else // 按页写入数据成功，进行下一次写数据的准备工作
        {
            current_addr += current_size;	// 计算下一次要写入的地址
            write_data += current_size;	// 获取下一次要写入的数据存储区地址
            // 计算下一次写数据的长度
            current_size = ((current_addr + W25Qxx_PageSize) > end_addr) ? (end_addr - current_addr) : W25Qxx_PageSize;
        }
    }
    while (current_addr < end_addr) ; // 判断数据是否全部写入完毕

    return OSPI_W25Qxx_OK;	// 写入数据成功
}

/**********************************************************************************************************************************
*
*	函 数 名: OSPI_W25Qxx_ReadBuffer
*
*	入口参数: pBuffer 		 - 要读取的数据
*				 ReadAddr 		 - 要读取 W25Qxx 的地址
*				 NumByteToRead  - 数据长度，最大不能超过flash芯片的大小
*
*	返 回 值: OSPI_W25Qxx_OK 		     - 读数据成功
*				 W25Qxx_ERROR_TRANSMIT	  - 传输失败
*				 W25Qxx_ERROR_AUTOPOLLING - 轮询等待无响应
*
*	函数功能: 读取数据，最大不能超过flash芯片的大小
*
*	说    明: 1.Flash的读取速度取决于OSPI的通信时钟，最大不能超过133M
*				 2.这里使用的是1-4-4模式下(1线指令4线地址4线数据)，快速读取指令 Fast Read Quad I/O
*				 3.使用快速读取指令是有空周期的，具体参考W25Q64JV的手册  Fast Read Quad I/O  （0xEB）指令
*				 4.实际使用中，是否使用DMA、编译器的优化等级以及数据存储区的位置(内部 TCM SRAM 或者 AXI SRAM)都会影响读取的速度
*  FANKE
*****************************************************************************************************************FANKE************/

int8_t OSPI_W25Qxx_ReadBuffer(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t NumByteToRead)
{
    OSPI_RegularCmdTypeDef  sCommand;// OSPI传输配置

    sCommand.OperationType           = HAL_OSPI_OPTYPE_COMMON_CFG;             // 通用配置
    sCommand.FlashId                 = HAL_OSPI_FLASH_ID_1;                    // flash ID

    sCommand.Instruction             = W25Qxx_CMD_FastReadQuad_IO;             // 1-4-4模式下(1线指令4线地址4线数据)，快速读取指令
    sCommand.InstructionMode         = HAL_OSPI_INSTRUCTION_1_LINE;            // 1线指令模式
    sCommand.InstructionSize         = HAL_OSPI_INSTRUCTION_8_BITS;            // 指令长度8位
    sCommand.InstructionDtrMode      = HAL_OSPI_INSTRUCTION_DTR_DISABLE;       // 禁止指令DTR模式

    sCommand.Address                 = ReadAddr;                               // 地址
    sCommand.AddressMode             = HAL_OSPI_ADDRESS_4_LINES;               // 4线地址模式
    sCommand.AddressSize             = HAL_OSPI_ADDRESS_24_BITS;               // 地址长度24位
    sCommand.AddressDtrMode          = HAL_OSPI_ADDRESS_DTR_DISABLE;           // 禁止地址DTR模式

    sCommand.AlternateBytesMode      = HAL_OSPI_ALTERNATE_BYTES_NONE;          // 无交替字节
    sCommand.AlternateBytesDtrMode   = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;   // 禁止替字节DTR模式

    sCommand.DataMode                = HAL_OSPI_DATA_4_LINES;                  // 4线数据模式
    sCommand.DataDtrMode             = HAL_OSPI_DATA_DTR_DISABLE;              // 禁止数据DTR模式
    sCommand.NbData                  = NumByteToRead;                          // 数据长度

    sCommand.DummyCycles             = 6;                                      // 空周期个数
    sCommand.DQSMode                 = HAL_OSPI_DQS_DISABLE;                   // 不使用DQS
    sCommand.SIOOMode                = HAL_OSPI_SIOO_INST_EVERY_CMD;           // 每次传输数据都发送指令

    // 写命令
    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_TRANSMIT;		// 传输数据错误
    }
    //	接收数据
    if (HAL_OSPI_Receive(&hospi1, pBuffer, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_TRANSMIT;		// 传输数据错误
    }
    // 使用自动轮询标志位，等待接收的结束
    if (OSPI_W25Qxx_AutoPollingMemReady() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING;		// 轮询等待无响应
    }
    return OSPI_W25Qxx_OK;	// 读取数据成功
}

/**
 * @brief Flash 读写测试
 * @return
 */
int8_t OSPI_W25Qxx_Test(void)		//Flash读写测试
{
    uint32_t i = 0X8000;	// 计数变量
    uint32_t W25Qxx_TestAddr  =	0	;							// 测试地址
    uint32_t ExecutionTime_Begin;		// 开始时间
    uint32_t ExecutionTime_End;		// 结束时间
    uint32_t ExecutionTime;				// 执行时间
    float    ExecutionSpeed;			// 执行速度

// 擦除 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>


    ExecutionTime_Begin 	= HAL_GetTick();	// 获取 systick 当前时间，单位ms
    OSPI_Status 			= OSPI_W25Qxx_BlockErase_32K(W25Qxx_TestAddr);	// 擦除32K字节
    ExecutionTime_End		= HAL_GetTick();	// 获取 systick 当前时间，单位ms

    ExecutionTime = ExecutionTime_End - ExecutionTime_Begin; // 计算擦除时间，单位ms

    if( OSPI_Status == OSPI_W25Qxx_OK )
    {
        printf ("\r\nW25Q64 擦除成功, 擦除32K字节所需时间: %d ms\r\n",ExecutionTime);
    }
    else
    {
        printf ("\r\n 擦除失败!!!!!  错误代码:%d\r\n",OSPI_Status);
        while (1);
    }

// 写入 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

    for(i=0;i<W25Qxx_NumByteToTest;i++)  //先将数据写入数组
    {
        W25Qxx_WriteBuffer[i] = i;
    }
    ExecutionTime_Begin 	= HAL_GetTick();	// 获取 systick 当前时间，单位ms
    OSPI_Status				= OSPI_W25Qxx_WriteBuffer(W25Qxx_WriteBuffer,W25Qxx_TestAddr,W25Qxx_NumByteToTest); // 写入数据
    ExecutionTime_End		= HAL_GetTick();	// 获取 systick 当前时间，单位ms

    ExecutionTime  = ExecutionTime_End - ExecutionTime_Begin; 		// 计算擦除时间，单位ms
    ExecutionSpeed = (float)W25Qxx_NumByteToTest / ExecutionTime ; // 计算写入速度，单位 KB/S
    if( OSPI_Status == OSPI_W25Qxx_OK )
    {
        printf ("\r\n写入成功,数据大小：%d KB, 耗时: %d ms, 写入速度：%.2f KB/s\r\n",W25Qxx_NumByteToTest/1024,ExecutionTime,ExecutionSpeed);
    }
    else
    {
        printf ("\r\n写入错误!!!!!  错误代码:%d\r\n",OSPI_Status);
        while (1);
    }

// 读取	>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>


    ExecutionTime_Begin 	= HAL_GetTick();	// 获取 systick 当前时间，单位ms
    OSPI_Status				= OSPI_W25Qxx_ReadBuffer(W25Qxx_ReadBuffer,W25Qxx_TestAddr,W25Qxx_NumByteToTest);	// 读取数据
    ExecutionTime_End		= HAL_GetTick();	// 获取 systick 当前时间，单位ms

    ExecutionTime  = ExecutionTime_End - ExecutionTime_Begin; 					// 计算擦除时间，单位ms
    ExecutionSpeed = (float)W25Qxx_NumByteToTest/1024/1024 / ExecutionTime*1000 ; 	// 计算读取速度，单位 MB/S

    if( OSPI_Status == OSPI_W25Qxx_OK )
    {
        printf ("\r\n读取成功,数据大小：%d KB, 耗时: %d ms, 读取速度：%.2f MB/s \r\n",W25Qxx_NumByteToTest/1024,ExecutionTime,ExecutionSpeed);
    }
    else
    {
        printf ("\r\n读取错误!!!!!  错误代码:%d\r\n",OSPI_Status);
        while (1);
    }
// 数据校验 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

    for(i=0;i<W25Qxx_NumByteToTest;i++)	//验证读出的数据是否等于写入的数据
    {
        if( W25Qxx_WriteBuffer[i] != W25Qxx_ReadBuffer[i] )	//如果数据不相等，则返回0
        {
            printf ("\r\n数据校验失败!!!!!出错位置：%d\r\n",i);
            while(1);
        }
    }
    printf ("\r\n校验通过!!!!! QSPI驱动W25Q64测试正常\r\n");

// 读取整片Flash的数据，用以测试速度 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

    printf ("\r\n*****************************************************************************************************\r\n");
    printf ("\r\n上面的测试中，读取的数据比较小，耗时很短，加之测量的最小单位为ms，计算出的读取速度误差较大\r\n");
    printf ("\r\n接下来读取整片flash的数据用以测试速度，这样得出的速度误差比较小\r\n");
    printf ("\r\n开始读取>>>>\r\n");
    ExecutionTime_Begin 	= HAL_GetTick();	// 获取 systick 当前时间，单位ms

    for(i=0;i<W25Qxx_FlashSize/(W25Qxx_NumByteToTest);i++)	// 每次读取 W25Qxx_NumByteToTest 字节的数据
    {
        OSPI_Status				= OSPI_W25Qxx_ReadBuffer(W25Qxx_ReadBuffer,W25Qxx_TestAddr,W25Qxx_NumByteToTest);	// 读取数据
        W25Qxx_TestAddr = W25Qxx_TestAddr + W25Qxx_NumByteToTest;
    }
    ExecutionTime_End		= HAL_GetTick();	// 获取 systick 当前时间，单位ms

    ExecutionTime  = ExecutionTime_End - ExecutionTime_Begin; 								// 计算擦除时间，单位ms
    ExecutionSpeed = (float)W25Qxx_FlashSize/1024/1024 / ExecutionTime*1000  ; 	// 计算读取速度，单位 MB/S

    if( OSPI_Status == OSPI_W25Qxx_OK )
    {
        printf ("\r\n读取成功,数据大小：%d MB, 耗时: %d ms, 读取速度：%.2f MB/s \r\n",W25Qxx_FlashSize/1024/1024,ExecutionTime,ExecutionSpeed);
    }
    else
    {
        printf ("\r\n读取错误!!!!!  错误代码:%d\r\n",OSPI_Status);
        while (1);
    }

    return OSPI_W25Qxx_OK ;  // 测试通过

}