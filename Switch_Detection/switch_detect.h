/*******************************************************************************
 * @file    switch_detect.h
 * @author  hexm
 * @version V1.0
 * @date    2026-02-20
 * @brief   开关量的检测模块函数头文件
 * @attention 健康生活事业部 - 软件及 IOT 组
 ******************************************************************************/
/**
 * 作用：
 * 1、兼容多种信号采集方式，如： 1、通过IO口直接读取  2、通过通信收到其它模块发过来的信号  3、通过其它函数处理后的信号量状态
 * 2、有些检测信号是低电平有效，有些是高电平有效，这里集中通过 检测状态采集转换函数，转换成有效信号，而不用每次使用都去判断高低电平；
 * 3、可以自定义信号有效/无效时间，如浮子到达高水位后稳定1秒才算有效（停止制水），当脱离高水位后，稳定3秒才开始启动制水，
 *    相当于可以自定义延时一定时间来执行对应的动作，避免信号波动负载频繁动作；
 * 
 * 使用方法：
 * 1、上电后调用 SignalDetect_init(); 进行初始化；
 * 2、在20ms周期调用 SignalDetect_Update();
 * 3、需要使用判断信号量状态时，调用 DETECTOR_STABLE_STATUS(sigState),传入需要判断的信号量检测状态结构体，如：
 * 	if(DETECTOR_STABLE_STATUS(PureWaterLevel_Low) == SIGNAL_VALID)
 * 
 * 本文件提供以下接口供外部调用：
 * void SignalDetect_init(void);			信号量检测模块初始化函数，初始化时调用一次
 * void SignalDetect_Update(void);			信号检测模块更新函数，每20ms调用一次
 * DETECTOR_CURRENT_STATUS(sigState)		访问信号量的当前状态(实时状态)
 * DETECTOR_STABLE_STATUS(sigState)			访问信号量的稳定状态(延时后的稳定状态)
 */
#ifndef SWITCH_DETECT_H
#define SWITCH_DETECT_H

/** 
  * @addtogroup HAL
  * @{
  */

/** 
  * @addtogroup 数字信号检测
  * @{
  */

/* 包含头文件 ---------------------------------------------------------------- */
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

//用户包含需要用到的外部函数接口
#include "myheader.h"
#include "key_user.h"



#ifndef bool_t
typedef bool bool_t;
#endif


/**
  * @brief 信号状态枚举定义
  */
enum 
{
    VOLTAGE_LEVEL_INVALID,          /*!< 无效电平状态 */
    VOLTAGE_LEVEL_VALID             /*!< 有效电平状态 */
};

typedef enum
{
    SIGNAL_UNSTABLE,               /*!< 信号未稳定状态 */
    SIGNAL_VALID,                  /*!< 信号有效状态 */
    SIGNAL_INVALID                 /*!< 信号无效状态 */
}SignalSta_t;

/**
  * @brief 信号检测状态结构体
  */
typedef struct  
{
    uint16_t _validHoldCnt;        /*!< 有效电平计时 */
    uint16_t _invalidHoldCnt;      /*!< 无效电平计时 */
    SignalSta_t currentSta;        /*!< 当前状态(不稳定) */
    SignalSta_t stableSta;         /*!< 稳定状态 */
}SigState_t;



/*---------------------------------用户需要改动的地方----------------------------------*/
//根据实际情况声明信号量检测状态结构体，供外部函数调用;
extern SigState_t PureWaterLevel_High;
extern SigState_t PureWaterLevel_Low;
extern SigState_t iceFull;	
/*------------------------------------------end--------------------------------------*/



typedef struct  
{
	SigState_t *_sigDetect;						/*!< 信号状态句柄 */	
    bool_t(*sigIsValid)(void);					/*!< 引脚电平信号检测函数句柄 */
    uint16_t validHoldTime;						/*!< 判断有效电平持续的时间 即有效电平保持多长时间算有效信号*/
    uint16_t invalidHoldTime;					/*!< 判断无效电平持续时的间 即无效电平保持多长时间算无效信号*/
}SigDetect_t;


/**
  * @brief 信号检测器初始化函数
  * @param void
  * @note 初始化调用一次；
  */
void SignalDetect_init(void);

/**
  * @brief 更新所有信号检测实例的状态
  * @param void
  * @note 主循环20ms周期调用
  */
void SignalDetect_Update(void);

#define DETECTOR_CURRENT_STATUS(sigState)   (sigState.currentSta)  /*!< 访问当前状态 */
#define DETECTOR_STABLE_STATUS(sigState)   (sigState.stableSta)    /*!< 访问稳定状态 */

/**
  * @}
  */

/**
  * @}
  */

#endif
