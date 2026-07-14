/*******************************************************************************
 * @file    key_check.h
 * @author  hexm
 * @version V1.0
 * @date    2026-02-20
 * @brief   按键检测模块头文件
 * @attention 健康生活事业部 - 软件及 IOT 组
 ******************************************************************************/
/**
 * 使用方法：
 * 1、上电后调用 Key_Init(Key_Read_Value,Key_Action_Deal); 进行按键初始化；
 * 2、在 KEY_ACTION_TIME （默认20ms）按键检测周期调用 Key_Action_Detect();
 * 3、在按键动作处理函数 Key_Action_Deal() 中实现各按键事件的处理；
 * 4、在 Key_Read_Value(); 实现按键键值的获取；
 * 5、需要判断不在标准定义的长按时间时，可以在 Key_Action_Deal();(每20ms调用)中，通过 Key_IsPushing(keyCode) + 计数器 进行判断; 
 * 6、KEY_USER_LONG_VALID_TIME 可设置自定义长按时间
 * 
 * 本文件提供以下接口供外部调用：
 * Key_Init(Key_Read_Value,Key_Action_Deal);	按键检测模块初始化函数
 * Key_Action_Detect();						按键动作检测函数(周期调用)
 * Key_IsPushing(uint16_t keyCode); 			检测传入的按键键值是否处于按下状态，返回1:处于按下状态 0:不处于按下状态
 * Key_Kill(); 								清除按键信息
 */



#ifndef KEY_CHECK_H
#define KEY_CHECK_H

#include "stdint.h"
#include "stdbool.h"



#ifndef bool_t
typedef bool bool_t;
#endif
/** 
  * @addtogroup HAL
  * @{
  */

/** 
  * @addtogroup 按键检测
  * @{
  */

#define KEY_ACTION_TIME        		20       					/*!< f_Key_ActionDetect调用时间 ms*/
/* 短按释放触发有效时间最短100ms，最长2.8s，超过2.8S释放不判断为短按*/
#define KEY_RELEASE_MIN_TIME        (100/KEY_ACTION_TIME)		/*!< 短按释放动作最小触发时间 */
#define KEY_RELEASE_MAX_TIME        (2800/KEY_ACTION_TIME)		/*!< 短按释放动作最大触发时间 */

/* 间歇触发按下1s后有效，每间隔0.5s触发*/
#define KEY_INTERVAL_VALID_TIME     (1000/KEY_ACTION_TIME)		/*!< 重复触发按键动作的最小时间 */
#define KEY_INTERVAL_RESTART_TIME   (500/KEY_ACTION_TIME)		/*!< 重复触发按键动作的间隔时间 */

/* 短按按下立即触发时间100ms*/
#define KEY_SHORT_VALID_TIME        (100/KEY_ACTION_TIME)		/*!< 短按按下立即触发时间 */

/* 长按按下立即触发时间3s*/
#define KEY_LONG_VALID_TIME         (3000/KEY_ACTION_TIME)		/*!< 标准长按3秒按键触发时间 */

/* 自定义长按下立即触发时间10s*/
#define KEY_USER_LONG_VALID_TIME    (10000/KEY_ACTION_TIME)		/*!< 长按自定义按键触发时间 */


#define KEY_NULL        0x0000              /*!< 无按键按下时的键值 */

/**
  * @brief 按键动作类型枚举定义
  */
typedef enum
{
	KEY_ACTION_SHORT,           /*!< 短按按下触发（按下立即触发，不等待释放） */    
	KEY_ACTION_RELEASING,       /*!< 短按释放（释放后触发） */    
	KEY_ACTION_LONG,            /*!< 标准长按（长按时间满足立即触发，不等待释放） */  
	KEY_ACTION_USER_LONG,       /*!< 自定义长按（长按时间满足立即触发，不等待释放） */   
	KEY_ACTION_INTERVAL,        /*!< 间歇（长按重复触发） */    
	KEY_ACTION_UNDEFINE         /*!< 未定义 */    
}KeyAction_t;

/**
  * @brief 按键信息数据结构
  */
struct KeyInfoCDT
{
	KeyAction_t actionKind;     /*!< 按键动作信息 */
	uint16_t id;                /*!< 按键键值信息 */
};

typedef struct KeyInfoCDT *KeyInfo_t;
/**
  * @brief 按键检测模块初始化函数
  * @param _readKeyFun:按键键值读取函数句柄.
  * @param _keyDealFun:按键动作处理函数句柄
  * @retval 无
  */
void Key_Init(uint16_t(*_readKeyFun)(void), void(*_keyDealFun)(KeyInfo_t));
/**
 * @brief 按键动作检测函数(周期调用)
 * @retval 无
 */
void Key_Action_Detect(void);
/**
 * @brief 清除按键信息
 * @retval 无
 */
void Key_Kill(void);
/**
 * @brief 检测指定按键是否处于按下状态
 * @param keyCode:要检测的按键键值
 * @retval 1:处于按下状态 0:不处于按下状态
 */
bool_t Key_IsPushing(uint16_t keyCode);

/**
  * @}
  */

/**
  * @}
  */

#endif
/**** (c) *************************** END OF FILE **/
