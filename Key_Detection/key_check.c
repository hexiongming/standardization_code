/*******************************************************************************
 * @file    key_check.c
 * @author  hexm
 * @version V1.0
 * @date    2026-02-20
 * @brief   按键检测模块实现，不允许随意改动
 * @attention 健康生活事业部 - 软件及 IOT 组
 ******************************************************************************/

/* 包含头文件 */
#include "key_check.h"

static struct KeyInfoCDT s_Key_info;
static uint16_t (*readKeyFun)(void);
static void (*keyDealFun)(KeyInfo_t);
static uint16_t s_isPushingKeyCode;



/***************************************************************
 * @brief 按键检测模块初始化函数
 * @param readKeyFun  按键读取回调函数指针
 * @param keyDealFun  按键处理回调函数指针
 * @note 系统初始化时调用f_Key_Init(Key_Read_Value, Key_Action_Deal);
 **************************************************************/
void Key_Init(uint16_t (*_readKeyFun)(void), void (*_keyDealFun)(KeyInfo_t))
{
	s_Key_info.actionKind = KEY_ACTION_UNDEFINE;
	s_Key_info.id = KEY_NULL;
	s_isPushingKeyCode = KEY_NULL;
	readKeyFun = _readKeyFun;
	keyDealFun = _keyDealFun;
}

/**************************************************************
* @name    Key_Action_Detect
* @brief   按键动作检测与状态调度主函数
* @note    主循环20ms定时调用，实现短按、长按、释放等动作识别及处理
**************************************************************/
void Key_Action_Detect(void)
{
	uint16_t Key_codeBuf;
	static uint16_t Key_stateHoldCnt = 0;
	static uint8_t  Key_stateHoldCntExt = 0;
	static uint16_t Key_releaseCnt = 0;

	Key_codeBuf = readKeyFun();

	if (s_Key_info.id != Key_codeBuf)
	{
		if ((KEY_NULL == Key_codeBuf) && (Key_releaseCnt > KEY_RELEASE_MIN_TIME)
			&& (Key_releaseCnt < KEY_RELEASE_MAX_TIME))
		{
			s_Key_info.actionKind = KEY_ACTION_RELEASING;
			keyDealFun(&s_Key_info);
		}
		else
		{
			Key_releaseCnt = 0;
		}

		s_Key_info.id = Key_codeBuf;
		Key_stateHoldCnt = 0;
		Key_stateHoldCntExt = 0;
	}
	else
	{
		if (KEY_NULL == s_Key_info.id)
		{
			Key_stateHoldCnt = 0;
			Key_stateHoldCntExt = 0;
			Key_releaseCnt = 0;
			s_isPushingKeyCode = KEY_NULL;
			return;
		}

		s_Key_info.actionKind = KEY_ACTION_UNDEFINE;
		Key_stateHoldCnt++;
		Key_stateHoldCntExt++;
		Key_releaseCnt++;
		s_isPushingKeyCode = Key_codeBuf;

		// 间隔触发
		if (Key_stateHoldCntExt >= KEY_INTERVAL_VALID_TIME)
		{
			Key_stateHoldCntExt = KEY_INTERVAL_VALID_TIME - KEY_INTERVAL_RESTART_TIME;
			s_Key_info.actionKind = KEY_ACTION_INTERVAL;
			keyDealFun(&s_Key_info);
		}

		// 短按 / 长按判断
		if (Key_stateHoldCnt == KEY_SHORT_VALID_TIME)
		{
			s_Key_info.actionKind = KEY_ACTION_SHORT;
		}
		else if (Key_stateHoldCnt == KEY_LONG_VALID_TIME)
		{
			s_Key_info.actionKind = KEY_ACTION_LONG;
		}
		else if (Key_stateHoldCnt == KEY_USER_LONG_VALID_TIME)
		{
			s_Key_info.actionKind = KEY_ACTION_USER_LONG;
		}
		else
		{
			// 防溢出
			if (Key_stateHoldCnt > 0xFF00)
			{
				Key_stateHoldCnt = 0xFF00;
			}
			return;
		}

		keyDealFun(&s_Key_info);
	}
}

/***************************************************************
 * @brief   强制清空当前按键状态
 * @note    需要复位按键状态时调用，清除动作与 ID
 **************************************************************/
void Key_Kill(void)
{
	s_Key_info.id = KEY_NULL;
	s_Key_info.actionKind = KEY_ACTION_UNDEFINE;
	s_isPushingKeyCode = KEY_NULL;
}

/***************************************************************
 * @brief   判断指定按键是否处于按下状态
 * @param   keyCode  待判断的按键编码
 * @return  1: 处于按下状态  0: 未处于按下状态
 * @note    业务逻辑中查询按键状态使用
 **************************************************************/
bool_t Key_IsPushing(uint16_t keyCode)
{
	return (s_isPushingKeyCode == keyCode);
}


/**** (c) *************************** END OF FILE **/
