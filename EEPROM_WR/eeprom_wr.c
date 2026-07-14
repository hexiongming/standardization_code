/**
 *****************************************************************************
 * @file    eeprom_wr..c
 * @author  hexm
 * @version V1.0.0
 * @date    2026-06-10
 * @brief   EEPROM写入和读取模块，该文件包含 eeprom_wr.h中的函数声明
 * @attention 健康生活事业部 - 软件及 IOT 组
 *****************************************************************************
 */

/* 头文件 */
#include "eeprom_wr.h"


//示例：
/********************************************************************************
 * @brief   读取失败后恢复默认值
 * @details 将默认值恢复到数据缓存中
 * @param   pData: 将默认值恢复到数据缓存指针
 * @return  void
 * @note    自己实现数据恢复默认值函数
 *******************************************************************************/
void FLASH_Data_Reset(uint8_t *pData)
{
	SysInfoBak_T *pstData = (SysInfoBak_T *)pData;
	pstData->start_code = 0x77;
	pstData->flush_remain_time = 0x8899;
	pstData->flush_total_time = 0xaabb;
	pstData->end_code = 0xcc;
}

/* ===================== 静态函数实现 ==================== */
/********************************************************************************
 * @brief   计算数据缓冲区CRC8校验值
 * @details 标准CRC8校验算法，用于存储数据完整性校验
 * @param   pData: 待计算校验的数据缓存指针
 * @param   Len: 参与校验的数据长度
 * @return  uint8_t: 计算得到的CRC8校验码
 * @note    校验多项式0x07，初始值0x00
 *******************************************************************************/
static uint8_t EEPROM_CalcCrc8(uint8_t *pData, uint8_t Len)
{
    uint8_t Crc = 0x00U;
    uint8_t i, j;

    for(i = 0; i < Len; i++)
    {
        Crc ^= pData[i];
        for(j = 0; j < 8; j++)
        {
            if(Crc & 0x80U)
            {
                Crc = (Crc << 1) ^ 0x07U;
            }
            else
            {
                Crc = Crc << 1;
            }
        }
    }
    return Crc;
}

/********************************************************************************
 * @brief   写入带帧头+CRC校验的完整用户数据帧至EEPROM
 * @details 封装帧头、业务数据、校验码批量写入存储介质
 * @param   pstData: 待存储的用户参数结构体指针
 * @return  bool_t: true-写入完成 false-写入硬件失败
 * @note    存储帧格式：帧头 + 用户数据 + CRC8校验1字节
 *******************************************************************************/
bool_t EEPROM_WriteUserData(uint8_t *pstData, uint8_t len)
{
    if(NULL == pstData)
    {
        return false;
    }
	if(len > EEPROM_USER_DATA_MAX_LEN  || len == 0)
	{
		return false;
	}

    uint8_t WriteBuf[EEPROM_HEAD_LEN + EEPROM_USER_DATA_MAX_LEN + EEPROM_CRC_LEN] = {0};
    uint8_t Offset = 0U;
    uint8_t CrcVal = 0U;

    WriteBuf[Offset++] = EEPROM_HEAD;

    memcpy(&WriteBuf[Offset], pstData, len);
    Offset += len;

    CrcVal = EEPROM_CalcCrc8((uint8_t *)pstData, len);
    WriteBuf[Offset++] = CrcVal;

	uint8_t write_len = Offset / N_BYTE_ALIGNMENT;
	if(0 != (Offset % N_BYTE_ALIGNMENT))
		write_len++;

    bool_t ret = EEPROM_WRITE(EEPROM_START_ADDR, WriteBuf, write_len);
	#if(EEPROM_SUPPORT_BACKUP)
	ret = EEPROM_WRITE(EEPROM_BACKUP_ADDR, WriteBuf, write_len);
	#endif
	
    return ret;
}

/********************************************************************************
 * @brief   读取并校验EEPROM存储的用户数据帧
 * @details 最多重试EEPROM_READ_RETRY_CNT次，全部失败则加载默认值
 * @param   addr: 读取的起始地址
 * @param   pstData: 接收读取数据的结构体指针
 * @param   len: 业务数据长度,不包含头码和校验码
 * @return  bool_t: true-读取校验通过 false-多次失败
 * @note    读取流程：读帧头校验 -> 读业务数据 -> CRC校验，失败自动重试
 *******************************************************************************/
static bool_t EEPROM_ReadData(uint8_t *addr, uint8_t *pstData, uint8_t len)
{
    if(NULL == pstData)
    {
        return false;
    }
	if(len > EEPROM_USER_DATA_MAX_LEN  || len == 0)
	{
		return false;
	}


    uint8_t ReadBuf[EEPROM_HEAD_LEN + EEPROM_USER_DATA_MAX_LEN + EEPROM_CRC_LEN] = {0};
    uint8_t RetryCnt = 0U;
    uint8_t Head;
	uint8_t Read_len = EEPROM_HEAD_LEN + len + EEPROM_CRC_LEN;
    uint8_t ReadCrc, CalcCrc;


    // 循环重试读取，最多EEPROM_READ_RETRY_CNT次
    for(RetryCnt = 0; RetryCnt < EEPROM_READ_RETRY_CNT; RetryCnt++)
    {
        // 读取帧缓存
        EEPROM_READ(addr,ReadBuf, Read_len);
   
        // 校验帧头
        Head = ReadBuf[0];
        if(EEPROM_HEAD != Head)
        {
            continue;
        }

        // 提取业务数据与存储校验码
        uint8_t *pDataBuf = &ReadBuf[EEPROM_HEAD_LEN];
        ReadCrc = ReadBuf[EEPROM_HEAD_LEN + len];

        // 本地重新计算CRC对比校验
        CalcCrc = EEPROM_CalcCrc8(pDataBuf, len);
        if(CalcCrc == ReadCrc)
        {
            memcpy(pstData, pDataBuf, len);
            return true;
        }
    }

    return false;
}

/********************************************************************************
 * @brief   读取并校验EEPROM存储的用户数据帧
 * @param   pstData: 接收读取数据的结构体指针
 * @param   len: 业务数据长度,不包含头码和校验码
 * @return  bool_t: true-读取校验通过 false-多次失败并恢复默认值
 *******************************************************************************/
bool_t EEPROM_ReadUserData(uint8_t *pstData, uint8_t len)
{
	#if(EEPROM_SUPPORT_BACKUP)
	if(false == EEPROM_ReadData((uint8_t *)EEPROM_START_ADDR, pstData, len))
	{
		if(true == EEPROM_ReadData((uint8_t *)EEPROM_BACKUP_ADDR, pstData, len))
		{
			EEPROM_WriteUserData(pstData, len); //从备份区读取成功后重新写入主存储区
			return true;
		}	
		else
		{
			EEPROM_RESET(pstData);			//两个区域都读取失败后，恢复默认值，并将默认值重新写入
			EEPROM_WriteUserData(pstData, len);
			return false;
		}

	}
	return true;
	#else
	if(false == EEPROM_ReadData((uint8_t *)EEPROM_START_ADDR, pstData, len))
	{
		EEPROM_RESET(pstData);			//读取失败后，恢复默认值，并将默认值重新写入
		EEPROM_WriteUserData(pstData, len);
		return false;
	}
	return true;
	#endif
}

/**** (c) *************************** END OF FILE **/
