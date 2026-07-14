/**
 *****************************************************************************
 * @file    eeprom_wr.h
 * @author  hexm
 * @version V1.0.0
 * @date    2026-06-10
 * @brief   EEPROM写入和读取模块头文件
 * @attention 健康生活事业部 - 软件及 IOT 组
 *****************************************************************************/
/**
 * 需要注意的点：
 * 1、本文件已实现写入和读取的头校验和数据校验，用户只需针对实际要存储或读取的数据操作即可
 * 2、因实际硬件平台不一，有些硬件平台写前需要擦除数据区域，需要自己在调用 EEPROM_WriteUserData() 前擦除后再写入
 * 3、读取主存储区和备份区都失败时，需要自己实现恢复默认值的函数EEPROM_RESET()
 * 移植需要修改的点：
 * 1、 EEPROM_USER_DATA_MAX_LEN 根据实际需要存储的数据量更改，可以比实际的大，但不能小
 * 2、 EEPROM_SUPPORT_BACKUP 、 EEPROM_START_ADDR 、 EEPROM_BACKUP_ADDR 根据实际需要更改储存地址
 * 3、 N_BYTE_ALIGNMENT 数据写入时对齐长度，如为1则无对齐，为4则对齐4字节，有些硬件平台需要对齐4字节写入
 * 4、 EEPROM_RESET() 、EEPROM_WRITE() 、EEPROM_READ() 根据实际硬件接口修改
 * 5、 包含相关接口的头文件
 * 
 * 本文件提供以下接口供外部调用：
 * bool_t EEPROM_WriteUserData(uint8_t *pstData,uint8_t len);		//存储数据函数，参数为实际要写入的数据指针和长度
 * bool_t EEPROM_ReadUserData(uint8_t *pstData,uint8_t len);		//读取数据函数，参数为实际要读取的数据指针和长度
 */

#ifndef EEPROM_WR_H
#define EEPROM_WR_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stdio.h"

// #include "flash.h"
// #include "EEPROM.h" 


#ifndef bool_t
typedef bool bool_t;
#endif


void FLASH_Data_Reset(uint8_t *pData);

/* ===================== 全局配置宏（可直接修改适配硬件） ==================== */

// 存储帧协议配置（带帧头+校验）
#define EEPROM_HEAD     				0xAAU	//头码
#define EEPROM_HEAD_LEN   				1U		//帧头长度
#define EEPROM_CRC_LEN          		1U		//校验码长度
#define EEPROM_USER_DATA_MAX_LEN		32U		//用户数据最大长度
#define EEPROM_READ_RETRY_CNT   		3U		//读取失败最大重试次数


// 底层硬件读写接口宏，根据硬件平台替换不同驱动函数
#define EEPROM_START_ADDR 				EEPROM_ADDR0	//写入地址，根据实际替换
#define EEPROM_SUPPORT_BACKUP 			0U				//支持备份功能
#if(EEPROM_SUPPORT_BACKUP)
#define EEPROM_BACKUP_ADDR 				EEPROM_ADDR1	//写入备份地址，根据实际替换
#endif
#define N_BYTE_ALIGNMENT 				4U				//写入数据长度对齐

#define EEPROM_RESET(buf) 				FLASH_Data_Reset(buf)				//自己实现将默认数据恢复到buf
#define EEPROM_WRITE(addr, buf, len) 	FLASH_Data_Write((uint32_t)addr, (uint32_t)len, (uint32_t *)buf)
#define EEPROM_READ(addr, buf, len) 	FLASH_Read((uint8_t *)addr, (uint32_t)len, (uint8_t *)buf)


typedef struct
{
    uint8_t start_code;                 
    uint16_t flush_remain_time;         
    uint16_t flush_total_time;          
    uint8_t end_code;                   
} SysInfoBak_T;
extern SysInfoBak_T SysInfoBak;
/* ===================== EEPROM驱动模块函数声明 ==================== */
/********************************************************************************
 * @brief   写入带帧头+CRC校验的完整用户数据帧至EEPROM
 * @details 封装帧头、业务数据、校验码批量写入存储介质
 * @param   pstData: 待存储的用户参数结构体指针
 * @param   len: 待存储的用户参数结构体长度,不包含头码和校验码
 * @return  bool_t: true-写入完成 false-写入硬件失败
 * @note    存储帧格式：帧头1字节 + 用户数据 + CRC8校验1字节
 *******************************************************************************/
bool_t EEPROM_WriteUserData(uint8_t *pstData,uint8_t len);

/********************************************************************************
 * @brief   读取并校验EEPROM存储的用户数据帧
 * @param   pstData: 接收读取数据的结构体指针
 * @param   len: 业务数据长度,不包含头码和校验码
 * @return  bool_t: true-读取校验通过 false-多次失败并恢复默认值
 * @note    读取失败会恢复默认值并将默认值重新写入
 *******************************************************************************/
bool_t EEPROM_ReadUserData(uint8_t *pstData,uint8_t len);

#endif
/**** (c) *************************** END OF FILE **/
