/*******************************************************************************
 * @file    key_user.c
 * @author  hexm
 * @version V1.0
 * @date    2026-02-20
 * @brief   按键逻辑处理模块实现，按键执行程序在这里实现
 * @attention 健康生活事业部 - 软件及 IOT 组
 ******************************************************************************/

/* 包含头文件 */
#include "key_user.h"



/***************************************************************
 * @brief 按键动作处理函数
 * @param info 按键信息结构体指针
 * @note 在 Key_Action_Detect() 中由 keyDealFun(&Key_info) 自动调用，按实际应用逻辑处理
 **************************************************************/
void Key_Action_Deal(KeyInfo_t info)
{
	if(KEY_ACTION_SHORT == info->actionKind)
	{
		if(KEY_BOILEDWATER == info->id)
		{
			//处理函数
			TempNum=100;
			BuzzerSet(SingleNum, SoundShortSingle);
		}
		else if(KEY_HOTWATER == info->id)
		{
			TempNum=85;
			//处理函数
		}
		else if(KEY_CHILDLOCKKEY == info->id)
		{
			if(Mode==SleepMode)	
				ShiftMode_Handle(StandbyMode);
			if(ChildLock_Flag)
				ChildLock_Flag = 0;
			else
				ChildLock_Flag = 1;
			BuzzerSet(SingleNum, SoundShortSingle);
			//处理函数	
		}

	}
	else if(KEY_ACTION_RELEASING == info->actionKind)
	{
		if(KEY_WARMWATER == info->id)
		{
			TempNum=45;
			//处理函数
		}
		else if(KEY_ICEWATER == info->id)
		{
			TempNum=3;
			//处理函数
		}

		else if((KEY_INCREASE) == info->id)
		{
			TempNum+=5;
			//处理函数	
		}
		else if((KEY_DECREASE) == info->id)
		{
			TempNum-=5;
			//处理函数
		}
	}
	else if(KEY_ACTION_LONG == info->actionKind)
	{
		if((KEY_ICEWATER | KEY_WARMWATER) == info->id)
		{
			IOT_Lib_NewBleSet();
			BuzzerSet(SingleNum, SoundSupLongSingle);
			//处理函数	
		}
		else if(KEY_WATERDISPENSE == info->id)
		{
			BuzzerSet(PowerNum, SoundShortSingle);
			//处理函数
		}
	}
	else if(KEY_ACTION_INTERVAL == info->actionKind)
	{
		if((KEY_INCREASE) == info->id)
		{
			TempNum++;
			BuzzerSet(SingleNum, SoundSupLongSingle);
			//处理函数	
		}
		else if((KEY_DECREASE) == info->id)
		{
			TempNum--;
			BuzzerSet(SingleNum, SoundSupLongSingle);
			//处理函数
		}

	}
}

/***************************************************************
 * @brief 读取按键键值函数
 * @return 按键键值
 * @note 由 Key_Action_Detect()自动调用，根据实际情况实现
 **************************************************************/
uint16_t Key_Read_Value(void)
{
	uint32_t keyCode = 0;
	//获取键值函数 例如：
	//keyCode = f_YS806A_ReadKeyDat(&IICBus_SDA);

	keyCode = g_KeyFlag;
	return keyCode;
}

/**** (c) *************************** END OF FILE **/
