/*******************************************************************************
 * @file    key_user.h
 * @author  hexm
 * @version V1.0
 * @date    2026-02-20
 * @brief   按键逻辑处理模块头文件
 * @attention 健康生活事业部 - 软件及 IOT 组
 ******************************************************************************/
/**
 * 使用方法：
 * 1、包含硬件底层获取按键头文件及其它处理按键时事件需要包含的头文件；
 * 2、在按键动作处理函数 Key_Action_Deal() 中实现各按键事件的处理；
 * 3、在 Key_Read_Value(); 实现按键键值的获取；
 * 4、实现各按键键值的宏定义;
 */


#ifndef KEY_USER_H
#define KEY_USER_H

#include "key_check.h"

#include "myheader.h"
#include "CheckTouchKey_Set.h"
#include "Buzzer.h"
#include "iot_user_app.h"
/** 
  * @addtogroup HAL
  * @{
  */

/** 
  * @addtogroup 按键处理
  * @{
  */

/* 按键键值宏定义 根据实际情况定义-------------------------------------------- */
#define KEY_BOILEDWATER			0x01		// 开水		0x01
#define KEY_INCREASE			0x02		// 增加		0x02
#define KEY_CONTINUOUS			0x04		// 连续		0x04
#define KEY_CHILDLOCKKEY		0x08		// 童锁		0x08
#define KEY_LARGETCUP			0x10		// 大杯		0x10
#define KEY_HOTWATER			0x20		// 热水		0x20
#define KEY_WARMWATER			0x40		// 温水 	0x40
#define KEY_MEDIUMCUP			0x80		// 中杯		0x80
#define KEY_WATERDISPENSE		0x100		// 取水		0x100
#define KEY_TEACOFFEE			0x200		// 茶咖		0x200
#define KEY_SMALLCUP			0x400		// 小杯 	0x400
#define KEY_ICEWATER			0x800		// 冰水		0x800 
#define KEY_DECREASE			0x1000		// 减少		0x1000
#define KEY_CLOD				0x2000		// 清洁		0x2000	
#define KEY_CLEAR				0x4000		// 制冷		0x4000


/**
 * @brief  按键处理函数
 * @param  info: 按键信息结构体
 *         自定义实现
 *         在 Key_Action_Detect() 中调用
 */
void Key_Action_Deal(KeyInfo_t info);

/**
 * @brief  按键键值获取函数
 * @param  void
 *         自定义实现
 *         在 Key_Action_Detect() 中调用
 */
uint16_t Key_Read_Value(void);

/**
  * @}
  */

/**	
  * @}
  */

#endif

/**** (c) *************************** END OF FILE **/
