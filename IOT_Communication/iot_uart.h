/*******************************************************************************
 * @file    iot_uart.h
 * @author  hexm
 * @version V1.0
 * @date    2026-04-20
 * @brief   串口收发模块头文件（底层）
 * @attention 健康生活事业部 - 软件及 IOT 组
 *
 * 职责：仅负责字节级别的串口收发与环形缓冲区管理，
 *       不做任何协议解析，保持与上层完全解耦。
******************************************************************************/
/**
 * 移植时需改动点：
 * 1、 IOT_UART_BAUDRATE					串口波特率，根据需要修改（一般情况下默认9600）
 * 2、 OTA_YMODEM_VALID_SIZE  				Ymodem模式下传输的有效数据大小，根据需要修改128/1024
 * 3、 IOT_UART_PLATFORM_SEND(); 			串口发送函数根据硬件平台适配
 * 4、 IOT_UART_PLATFORM_INIT(); 			根据硬件平台传入串口初始化函数
 * 5、 IOT_UART_RxISR(); 					在UART中断接收函数中调用
 * 6、 IOT_TXSetIdle();						串口发送完成调用设置发送设为空闲
 * 7、 需要包含uart硬件底层驱动头文件
 * 8、 调用WiFi串口发送数据时，必须在队列里面发送，首先调用 IOT_UART_TxEnQueue() 入列，严禁直接调用 IOT_UART_Send() 直接发送；
 */

#ifndef IOT_UART_H
#define IOT_UART_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "uart_demo.h"				//硬件底层驱动头文件，按需求修改


#ifdef __cplusplus
extern "C" {
#endif


#ifndef bool_t
typedef bool bool_t;
#endif


/* ========================= 配置宏 ========================= */
#define IOT_UART_BAUDRATE           9600U   /* 串口波特率*/

#define OTA_YMODEM_VALID_SIZE 		128		/* YMODEM 有效数据大小 */
#if (OTA_YMODEM_VALID_SIZE  == 1024)
#define IOT_UART_RX_QUEUE_SIZE		((OTA_YMODEM_VALID_SIZE + 5U) * 2U)   /* 接收环形缓冲区大小（字节）*/
#else
#define IOT_UART_RX_QUEUE_SIZE		512U    /* 接收环形缓冲区大小（字节）*/
#endif	

#define IOT_RX_CMD_MAX_SIZE			128U	/* 接收WiFi下发的单条指令数据最大长度（字节）*/

#define IOT_UART_TX_BUF_SIZE        128U    /* 发送缓冲区大小（字节）*/
#define IOT_UART_TX_BUF_NUM			5U		/* 发送队列最大缓存条目数,确保 ≥ 5 需预留足够缓冲，否则有可能会导致需要上报的数据被覆盖*/


/* ========================= 平台适配层（移植时修改此处） ========================= */
/* 示例：STM32 HAL，替换为实际平台 API */
#ifndef IOT_UART_PLATFORM_SEND
#define IOT_UART_PLATFORM_SEND(data, len)   Uart2_IntSend(data, len)	//宏定义内容根据实际替换
#endif
#ifndef IOT_UART_PLATFORM_INIT
#define IOT_UART_PLATFORM_INIT(baudrate)   Uart2_Init(baudrate)	//宏定义内容根据实际替换
#endif

/* ========================= 发送队列（内部） ========================= */
/*
 * 环形发送队列：所有待发送的 AT 帧字符串统一入队，
 * IOT_TX_Handle 每次调用时尝试取队头一条发送，
 * 间隔未到时保留队头，下次 Poll 再试，不丢失数据。
 */
typedef struct
{
	char    txbuf[IOT_UART_TX_BUF_NUM][IOT_UART_TX_BUF_SIZE];
	uint8_t head;
	uint8_t tail;
	uint8_t count;
} IOT_TxQueue_t;

/* ========================= 接收环形缓冲区（内部） ========================= */
typedef struct
{
	uint8_t  buf[IOT_UART_RX_QUEUE_SIZE];
	uint16_t head;
	uint16_t tail;
} IOT_RxQueue_t;


/* ========================= 函数声明 ========================= */
/********************************************************************************
 * @brief   接收队列初始化
 * @details 清空接收环形缓冲区
 * @param   void
 * @return  void
 * @note    需要使用时调用一次
 *******************************************************************************/
void IOT_RxQueue_Init(void);

/*******************************************************************************
 * @brief   串口模块初始化
 * @details 初始化环形缓冲区，配置串口硬件(9600,8N1)
 * @param   void
 * @return  无
 * @note    上电仅调用一次
 ******************************************************************************/
void IOT_UART_Init(void);

/*******************************************************************************
 * @brief   将字符串压入发送队列
 * @details 拷贝字符串到队列槽位，队列满时静默丢弃
 * @param   str: 待入队的完整 AT 帧字符串，如 "AT+SOFTSTART=2\r\n"
 * @return  bool: true=入队成功  false=队列已满
 ******************************************************************************/
bool IOT_UART_TxEnQueue(const char *str);

/*******************************************************************************
 * @brief   发送队列轮询
 * @details 尝试从队列取出队头一条数据发送；间隔未到时保留队头等待下次调用。
 *          在 IOT_RX_Handle 中每次调用
 * @param   void
 * @return  void
 ******************************************************************************/
void IOT_TX_Handle(void);

/********************************************************************************
 * @brief   设置tx忙/闲状态
 * @details 供外部调用，设置 tx 忙/闲状态
 * @param   value
 * @return  void
 * @note    在 硬件串口中断处理函数发送完成后调用
 *******************************************************************************/
void IOT_TXSetBusy(void);

/********************************************************************************
 * @brief   设置tx闲状态
 * @details 供外部调用，设置 tx 闲状态
 * @param   void
 * @return  void
 * @note    在 硬件串口中断处理函数发送完成后调用
 *******************************************************************************/
void IOT_TXSetIdle(void);

/********************************************************************************
 * @brief   查询tx忙/闲状态
 * @details 供外部调用，查询 tx 忙/闲状态
 * @param   value
 * @return  void
 * @note    需要使用时调用，一般在要发数据前调用
 *******************************************************************************/
uint8_t IOT_TXBusy_Return(void);

/********************************************************************************
 * @brief   查询TxQueue 空间
 * @details 供外部调用,查询TXQueue 剩余空间
 * @param   value
 * @return  void
 * @note    在TxQueue入队前调用查询剩余空间
 *******************************************************************************/
uint8_t IOT_TXQueue_Size_Return(void);

/*******************************************************************************
 * @brief   读取接收队列中存在的数据长度
 * @details 从接收的环形队列读取有效数据字节数大小
 * @param   void
 * @return  已缓存的数据长度
 ******************************************************************************/
uint16_t IOT_RxQueue_DataLen(void);

/*******************************************************************************
 * @brief   从接收缓冲区读取n个字节的数据
 * @details 尝试从接收的环形队列读取n个字节有效数据。
 * @param   buf: 输出缓冲区指针
 * @param   len: 读取字节数
 * @return  成功读取的字节数，0表示无数据
 ******************************************************************************/
uint16_t IOT_RxQueue_ReadData(uint8_t *buf, uint16_t Len);

/*******************************************************************************
 * @brief   读取一行数据
 * @details 以\r\n为结束符，读取完整AT指令行
 * @param   buf: 输出数据缓冲区
 * @param   maxLen: 缓冲区最大长度
 * @return  实际读取长度，0表示无完整数据
 ******************************************************************************/
uint16_t IOT_UART_ReadLine(uint8_t *buf, uint16_t maxLen);

/*******************************************************************************
 * @brief   串口接收中断回调
 * @details 在UART RX中断中调用，将字节存入环形缓冲区
 * @param   byte: 中断接收到的字节
 * @return  无
 * @note    中断上下文执行，禁止阻塞
 ******************************************************************************/
void IOT_UART_RxISR(uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif


/**** (c) *************************** END OF FILE **/
