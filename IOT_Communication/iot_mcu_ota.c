/*******************************************************************************
 * @file    iot_mcu_ota.c
 * @author  hexm
 * @version V1.0.0
 * @date    2026-05-20
 * @brief   MCU OTA实现文件
 * @attention 健康生活事业部 - 软件及 IOT 组
 *
 * 职责：处理WiFi模组对MCU OTA过程中的指令响应以及固件数据接收
******************************************************************************/
#include "iot_mcu_ota.h"


static mcu_ota_data_t mcu_ota_data;

#if MCU_OTA_UPDATE_MODE
static uint32_t s_ota_write_offset = 0;

//=============================================================================
// 写flash函数实现，需要根据芯片差异进行修改
// 需要包含MCU底层flash硬件驱动头文件；
// 需修改写初始地址；
//=============================================================================
static Result_e Mcu_Ota_Flash_Write(uint32_t addr, uint8_t* data,uint32_t data_len)
{
	uint32_t word_len;
	uint8_t buff[OTA_YMODEM_VALID_SIZE]; 

	if ((data_len % 4 != 0) || (data_len == 0))
    {
        return RESULT_ERROR;
    }

    word_len = data_len / 4; 
	if (0 != EraseSector_Code(MCU_OTA_START_ADDR))
    { // 0为成功
        return RESULT_ERROR;
    }
	memcpy(buff, data, data_len);
    if (0 != FLASH_Code_Write((uint32_t)(addr + MCU_OTA_START_ADDR), word_len, (uint32_t *)buff))
    { // 0为成功
        return RESULT_ERROR;
    }
	return RESULT_SUCCESS;
}

//=============================================================================
// 内部CRC16-XMODEM 实现
//=============================================================================
static uint16_t Ymodem_Check_Crc16(const uint8_t *data, uint32_t len) 
{
    uint16_t crc = 0x0000;
    for (uint32_t i = 0; i < len; i++) 
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) 
        {
            if (crc & 0x8000)
            {
                crc = (crc << 1) ^ 0x1021;
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc;
}

//=============================================================================
// 发送单字节
//=============================================================================
static void Ota_Send_Byte(uint8_t c) 
{
	char send_data = c;
	IOT_UART_TxEnQueue(&send_data);
}


//=============================================================================
// 处理YMODEM传输带的头包固件信息
//=============================================================================
static Result_e Ymodem_Header_Msg_Deal(uint8_t* data,uint16_t data_len)
{

    uint16_t filename_len = 0;

	// 计算filename长度
	while (filename_len < data_len && data[filename_len] != 0x00) 
	{
		filename_len++;
	}
	
	// 计算filesize位置
	uint16_t filesize_start = filename_len + 1; // 跳过filename和0x00分隔符
	uint16_t filesize_len = 0;
	
	// 计算filesize长度
	while ((filesize_start + filesize_len) < data_len && data[filesize_start + filesize_len] != 0x00)
	{
		filesize_len++;
	}
	
	if (filesize_len > 0) 
	{
		// 将字符串形式的文件大小转换为整数值并存储
		uint32_t filesize = 0;
		for (uint16_t i = 0; i < filesize_len; i++) 
		{
			if (data[filesize_start + i] >= '0' && data[filesize_start + i] <= '9') 
			{
				filesize = filesize * 10 + (data[filesize_start + i] - '0');
			}
		}

		// 根据分包规则计算总包数
		if (filesize > 0) 
		{
			uint32_t total_packets = 0;
			mcu_ota_data.packet_size = filesize;
			
			total_packets = filesize / OTA_YMODEM_VALID_SIZE;
			if (filesize % OTA_YMODEM_VALID_SIZE != 0) 
			{
				total_packets++; // 最后一包不满 OTA_YMODEM_VALID_SIZE 字节也要算一包
			}

			mcu_ota_data.packet_sum = total_packets;
			mcu_ota_data.packet_num = 0;
		}
		else
		{
			return RESULT_ERROR;
		}
		
	}

    return RESULT_SUCCESS;
}
//=============================================================================
// 处理YMODEM传输的固件数据BIN文件数据并写flash
//=============================================================================
static Result_e Ymodem_Packet_Bin_Deal(uint8_t* data,uint32_t data_len)
{
	 if(mcu_ota_data.packet_serial == data[1]) //相同一帧数据已处理就不处理了；
    {
        return RESULT_ERROR;
    }

	if( 0 == Mcu_Ota_Flash_Write(s_ota_write_offset,data,data_len))	//写flash
	{
		mcu_ota_data.packet_num ++;
		mcu_ota_data.receive_byte += data_len;
		s_ota_write_offset += data_len;
	}
	else
	{
		return RESULT_ERROR;
	}

	if((mcu_ota_data.packet_num == mcu_ota_data.packet_sum) 
		&& (mcu_ota_data.receive_byte == mcu_ota_data.packet_size))
	{
		mcu_ota_data.e_ymodem_step = YMODEM_EOT_NAK;
	}
    mcu_ota_data.packet_serial = data[1];
    return RESULT_SUCCESS;
}

/********************************************************************************
 * @brief   Ymodem数据处理
 * @details 解析帧头、序号、数据、CRC校验
 * @param   data: 串口接收的数据，len 数据长度
 * @return  执行结果
 * @note    在UART接收中断或轮询中调用，实现Ymodem状态机
 *******************************************************************************/
static Result_e Mcu_Ota_Ymodem_Transmit(uint8_t *data ,uint16_t len) 
{
	uint16_t crc_recv;
	uint16_t crc_calc;
	uint32_t msg_len;

	//EOT单字节数据包处理（正常在固件传输结束后发送EOT）
	if(EOT == data[0] && mcu_ota_data.e_ymodem_step == YMODEM_EOT_NAK)
	{
		Ota_Send_Byte(NAK);
		mcu_ota_data.e_ymodem_step = YMODEM_EOT_ACK;
		return RESULT_SUCCESS;
	}
	else if(EOT == data[0] && mcu_ota_data.e_ymodem_step == YMODEM_EOT_ACK)
	{
		Ota_Send_Byte(ACK);
		Ota_Send_Byte(C);
		mcu_ota_data.e_ymodem_step = YMODEM_END;
		return RESULT_SUCCESS;
	}
	else
	{
		msg_len = len;
		if(msg_len < MCU_OTA_PACKET_BUF_SIZE) 
		{
			return RESULT_ERROR;
		}
		crc_recv = data[msg_len-1] + (uint16_t)((data[msg_len-2] & 0x00FF) << 8);
		crc_calc = Ymodem_Check_Crc16(data + 3, msg_len-5);
		if( crc_calc != crc_recv)
		{
			return RESULT_ERROR;
		}

		if(mcu_ota_data.e_ymodem_step == YMODEM_HEADER)
		{
			if(0x00 == data[1])
			{
				if(Ymodem_Header_Msg_Deal(data + 3,msg_len -5) == RESULT_SUCCESS)
				{
					Ota_Send_Byte(ACK);
					Ota_Send_Byte(C);
					mcu_ota_data.e_ymodem_step = YMODEM_PACKET;
					s_ota_write_offset = 0; //重头开始写flash
					return RESULT_SUCCESS;
				}
				else
				{
					Ota_Send_Byte(NAK);
					return RESULT_ERROR;
				}
			}

		}
		else if(mcu_ota_data.e_ymodem_step == YMODEM_PACKET)
		{
			if(Ymodem_Packet_Bin_Deal(data + 3,msg_len -5) == RESULT_SUCCESS)
			{
				Ota_Send_Byte(ACK);
				return RESULT_SUCCESS;
			}
			else
			{
				Ota_Send_Byte(NAK);
				return RESULT_ERROR;
			}
		}
		else if(mcu_ota_data.e_ymodem_step == YMODEM_END)
		{
			Ota_Send_Byte(ACK);
			mcu_ota_data.e_ymodem_step = YMODEM_NULL;
			mcu_ota_data.e_mcu_ota_state = MCU_OTA_STATE_TRANSMIT_DONE;
			return RESULT_SUCCESS;
		}
	}
	return RESULT_ERROR;
}

/********************************************************************************
 * @brief   Ymodem数据处理
 * @details 从RxQueueu获取数据包，并调用协议解析
 * @param   void
 * @return  Result_e
 * @note    在 Mcu_Ota_Get_State() == MCU_OTA_STATE_YMODEM_TRANSMIT 时调用
 *******************************************************************************/
Result_e Mcu_Ota_Ymodem_Handle(void) 
{
	uint16_t readLen = 0;
	uint16_t targetLen = 0;
	uint8_t packetBuf[MCU_OTA_PACKET_BUF_SIZE];

	uint16_t dataLen = IOT_RxQueue_DataLen();


	if(mcu_ota_data.e_ymodem_step == YMODEM_PACKET 
		|| mcu_ota_data.e_ymodem_step == YMODEM_END 
		|| mcu_ota_data.e_ymodem_step == YMODEM_HEADER)
	{
		targetLen = MCU_OTA_PACKET_BUF_SIZE;
	}
	else if(mcu_ota_data.e_ymodem_step == YMODEM_EOT_NAK 
		|| mcu_ota_data.e_ymodem_step == YMODEM_EOT_ACK)
	{
		targetLen = 1;
	}
	if(dataLen < targetLen)
	{
		return RESULT_ERROR;
	}
	readLen = IOT_RxQueue_ReadData(packetBuf, targetLen);
	if (readLen == targetLen)
	{
		// 数据完整，传递给协议解析
		return Mcu_Ota_Ymodem_Transmit(packetBuf, targetLen);
	}
	else
	{
		return RESULT_ERROR;
	}
}

/********************************************************************************
 * @brief   启动Ymodem传输
 * @details 发送C命令启动Ymodem协议传输固件数据
 * @param   /
 * @return  void
 * @note    调用后进入TRANSMIT状态，开始接收Ymodem数据帧
 *******************************************************************************/
void Mcu_Ota_Start_Transmit(void) 
{
	Ota_Send_Byte(C); // 启动Ymodem模式
    Mcu_Ota_Init();	//	初始化OTA相关变量
	IOT_RxQueue_Init(); //清空接收队列，避免未处理的数据影响
	mcu_ota_data.e_mcu_ota_state = MCU_OTA_STATE_YMODEM_TRANSMIT;
	mcu_ota_data.e_ymodem_step = YMODEM_HEADER;
    
}
#endif

/********************************************************************************
 * @brief   MCU OTA初始化
 * @details 保存底层硬件接口函数指针，初始化OTA状态机
 * @param   hdl: OTA句柄指针
 * @param   local_version: 本地版本号字符串
 * @param   flash_write: Flash写入函数指针
 * @return  void
 * @note    在OTA启动时调用一次
 *******************************************************************************/
void Mcu_Ota_Init(void)
{
    memset(&mcu_ota_data, 0, sizeof(mcu_ota_data_t));

    mcu_ota_data.e_mcu_ota_state = MCU_OTA_STATE_IDLE;

}

/********************************************************************************
 * @brief   重新开始Ymodem传输
 * @details 发送AT+MCUOTARESUME命令恢复中断的Ymodem传输
 * @param   size: 数据包大小，OTA_PKT_128或OTA_PKT_1024
 * @return  void
 * @note    在NEED_RESUME状态下调用，继续之前中断的传输
 *******************************************************************************/
void Mcu_Ota_Resume_Transmit(void) 
{
	/* 断电重传：发送 RESUME 指令 */
	char txBuf[32];
	snprintf(txBuf, sizeof(txBuf), "MCUOTARESUME=%d",OTA_YMODEM_VALID_SIZE);
	IOT_Lib_Send_ATCmd(txBuf);
	mcu_ota_data.e_mcu_ota_state = MCU_OTA_STATE_NEED_RESUME;
}

/********************************************************************************
 * @brief   获取OTA当前状态
 * @details 返回OTA状态机的当前状态值
 * @param   void
 * @return  mcu_ota_state_e: 当前OTA状态
 * @note    供上层应用查询OTA状态
 *******************************************************************************/
mcu_ota_state_e Mcu_Ota_Get_State(void)                
{ 
    return mcu_ota_data.e_mcu_ota_state; 
}

/********************************************************************************
 * @brief   设置OTA当前状态
 * @details 设置OTA状态值
 * @param   mcu_ota_state_e
 * @return  /
 * @note    供应用层设置OTA状态
 *******************************************************************************/
void Mcu_Ota_Set_State(mcu_ota_state_e state)                
{ 
    mcu_ota_data.e_mcu_ota_state = state; 
}

/**** (c) *************************** END OF FILE **/
