/*******************************************************************************
 * @file    switch_detect.c
 * @author  hexm
 * @version V1.0
 * @date    2026-06-20
 * @brief   开关量的检测模块函数
 * @attention 健康生活事业部 - 软件及 IOT 组
 ******************************************************************************/

/* 包含头文件 */
#include "switch_detect.h"



/* 定义需要采集的信号量检测状态结构体，并在switch_detect.h中声明，方便在其它函数中使用 */
SigState_t PureWaterLevel_High;	//示例 纯水壶高水位检测信号量
SigState_t PureWaterLevel_Low;	//示例 纯水壶低水位检测信号量
SigState_t iceFull;				//示例 冰水壶满水检测信号量

/* 定义需要信号量检测状态采集函数，将高低电平或其它状态转化成开关量的有效状态 */
/***************************************************************
 * @brief 信号量检测状态采集转换函数
 * @param void
 * @return void
 * @note 将高/低电平或其它状态转化成开关量的有效状态
 * @attention 兼容多种信号采集方式，如：
 * 1、通过IO口直接读取
 * 2、通过通信收到其它模块发过来的信号 
 * 3、通过其它函数处理后的信号量状态
 **************************************************************/
static bool_t Switch_PureWaterLevel_High(void)	//示例：通过IO口直接读取，低电平有效
{
    return (PORT_GetBit(PORTD, PIN0) == 0) ? VOLTAGE_LEVEL_VALID : VOLTAGE_LEVEL_INVALID;			
}
static bool_t Switch_PureWaterLevel_Low(void)	//示例：通过通信收到其它模块发过来的信号
{
	return (FlagReg.bits.ColdH == 1) ? VOLTAGE_LEVEL_VALID : VOLTAGE_LEVEL_INVALID;
}
static bool_t Switch_IceFull(void)	//示例：通过其它函数处理后的信号量状态
{
    return (Key_IsPushing(KEY_ICEWATER)) ? VOLTAGE_LEVEL_VALID : VOLTAGE_LEVEL_INVALID;			
}

/* 定义需要采集的信号量检测参数结构体 */
static SigDetect_t s_SigConfig[] = 
{	//检测状态结构体		//信号检测函数句柄				//信号有效时间	//信号无效时间 单位20ms
	{&PureWaterLevel_High,	Switch_PureWaterLevel_High,		150,		25},
	{&PureWaterLevel_Low,	Switch_PureWaterLevel_Low,		50,			25},
	/*新增示例：*/
	{&iceFull,				Switch_IceFull,					250,		250},
	//{&PureWaterLevel_Low,	Switch_PureWaterLevel_Low,		50,			25},
};
#define DETECTOR_MAX_NUM		(sizeof(s_SigConfig) / sizeof(SigDetect_t))

/* 以上需要用户自定义实现，下面无需改动*/
/*-----------------------------------------------------------------------------------------------------------------*/


/***************************************************************
 * @brief 信号检测器初始化函数
 * @param void
 * @return void
 * @note 初始化调用一次；
 **************************************************************/
void SignalDetect_init(void)
{
    SigDetect_t sigD;

    for (uint8_t i = 0; i < DETECTOR_MAX_NUM; i++)
    {
        sigD = s_SigConfig[i];
        sigD._sigDetect->_validHoldCnt = 0;
        sigD._sigDetect->_invalidHoldCnt = 0;
        sigD._sigDetect->currentSta = SIGNAL_UNSTABLE;
        sigD._sigDetect->stableSta = SIGNAL_UNSTABLE;
    }
}
/***************************************************************
 * @brief 更新所有信号检测实例的状态
 * @param void
 * @return void
 * @note 主循环20ms周期调用；
 **************************************************************/
void SignalDetect_Update(void)
{
    bool_t _isValid;
    uint8_t i;

    for (i = 0; i < DETECTOR_MAX_NUM;i++)
    {
        _isValid = s_SigConfig[i].sigIsValid();

        if (_isValid)
        {
            s_SigConfig[i]._sigDetect->_invalidHoldCnt = 0;
            s_SigConfig[i]._sigDetect->currentSta = SIGNAL_VALID;

            if (s_SigConfig[i]._sigDetect->_validHoldCnt > s_SigConfig[i].validHoldTime)
            {
                s_SigConfig[i]._sigDetect->stableSta = SIGNAL_VALID;
            }
			else
			{
				s_SigConfig[i]._sigDetect->_validHoldCnt ++;
			}
        }
        else
        {
            s_SigConfig[i]._sigDetect->_validHoldCnt = 0;
            s_SigConfig[i]._sigDetect->currentSta = SIGNAL_INVALID;

            if (s_SigConfig[i]._sigDetect->_invalidHoldCnt >= s_SigConfig[i].invalidHoldTime)
            {
                s_SigConfig[i]._sigDetect->stableSta = SIGNAL_INVALID;
            }
			else
			{
				s_SigConfig[i]._sigDetect->_invalidHoldCnt ++;
			}
        }
    }
}
