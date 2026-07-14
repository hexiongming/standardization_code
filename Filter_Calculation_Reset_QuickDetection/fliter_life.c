/*******************************************************************************
 * @file    fliter_life.c
 * @author  hexm
 * @version V1.0
 * @date    2026-03-20
 * @brief   滤芯寿命计算、复位、快检功能，本文件不允许随意改动，只能配置 fliter_life.h的宏定义
 * @attention 健康生活事业部 - 软件及 IOT 组 
 ******************************************************************************/

/* 包含头文件 */
#include "fliter_life.h"



/************************** 静态全局变量 **************************/
static FilterInfo_T s_filter_info_t[FILTER_TYPE_MAX];	// 滤芯信息数组
static FilterMode_E s_filter_mode;						// 滤芯工作模式
static uint32_t s_filter_test_sec;						// 滤芯快检时间
static bool_t s_filter_test_fast;						// 滤芯快检加速使能



/*******************************************************************************
 * @brief   获取滤芯设计流量（毫升）
 * @param   type: 滤芯类型
 * @return  滤芯容量（毫升）
 ******************************************************************************/
static uint32_t Get_Filter_Capacity_ML(FilterType_E type) 
{
	return FILTER_ML(type); 
}

/*******************************************************************************
 * @brief   获取滤芯设计时间（秒）
 * @param   type: 滤芯类型
 * @return  滤芯总时间容量（秒）
 ******************************************************************************/
static uint32_t Get_Filter_Capacity_Sec(FilterType_E type) 
{
	return FILTER_SEC(type);
}

/*******************************************************************************
 * @brief   更新滤芯状态（1秒调用一次）
 * @param   type: 滤芯类型
 * @return  /
 ******************************************************************************/
static void Update_Filter_Status(FilterType_E type) 
{
	uint32_t remain_sec = 0;
	uint32_t remain_ml = 0;

	remain_sec = s_filter_info_t[type].remain_sec;
	remain_ml = s_filter_info_t[type].remain_ml;
	
	// 状态判断：非出厂状态 或 快检模式
	if ((s_filter_info_t[type].status != FILTER_LIFE_FACTORY_NEW) ||
		(Is_Filter_Test_Mode()))
	{
		// 滤芯到期：时间或流量任一耗尽
		if ((remain_sec <= 0U) || (remain_ml <= 0U))
		{
			s_filter_info_t[type].status = FILTER_LIFE_EXPIRED;
		}
		// 滤芯即将到期：无效于告警阈值
		else if ((remain_sec <= FILTER_WARNING_THRESHOLD_SEC) || 
				 (remain_ml <= FILTER_WARNING_THRESHOLD_ML))
		{
			s_filter_info_t[type].status = FILTER_LIFE_WARNING;
		}
		// 滤芯正常
		else
		{
			s_filter_info_t[type].status = FILTER_LIFE_NORMAL;
		}
	}
}

/************************** 全局函数实现 **************************/

/*******************************************************************************
 * @brief   所有滤芯状态更新（1秒调用一次）
 * @param   type: 滤芯类型
 * @return  /
 ******************************************************************************/
void Update_Filter_Remain_Sec(FilterType_E type)
{
	uint32_t step_sec = 0;

	// 滤芯快检模式
	if (Is_Filter_Test_Mode())
	{
		#if TEST_MODE_MIX_MODE
		if(s_filter_test_fast == false)
		{
			step_sec = 1;
		}
		else
		{
			if (s_filter_info_t[type].status == FILTER_LIFE_NORMAL)
			{
				step_sec = TEST_TIME_SEC_SEC;
			}
			else if (s_filter_info_t[type].status == FILTER_LIFE_WARNING)
			{
				step_sec = TEST_TIME_SEC_SEC_WARNIG;
			}
			else
			{
				step_sec = TEST_TIME_SEC_SEC;
			}
		}
		
		// 正常状态下的阈值保护逻辑
		if (s_filter_info_t[type].status == FILTER_LIFE_NORMAL)
		{
			if (s_filter_info_t[type].remain_sec > step_sec)
			{
				if (s_filter_info_t[type].remain_sec - step_sec < FILTER_WARNING_THRESHOLD_SEC)
				{
					s_filter_info_t[type].remain_sec = FILTER_WARNING_THRESHOLD_SEC;
					return;
				}
			}
		}
		#else
		if (s_filter_test_fast == true)
		{
			step_sec = TEST_TIME_SEC_SEC;
		}
		else
		{
			step_sec = 1;
		}
		#endif
	}
	else
	{
		step_sec = 1;
	}

	// 时间递减处理
	if (s_filter_info_t[type].remain_sec > step_sec)
	{
		s_filter_info_t[type].remain_sec -= step_sec;
	}
	else
	{
		s_filter_info_t[type].remain_sec = 0;
	}
}

/*******************************************************************************
 * @brief   更新所有滤芯剩余时间和状态（1秒调用一次）
 * @param   /
 * @return  /
 ******************************************************************************/
void Update_All_Filter_Remain_Sec(void) 
{
	FilterType_E type;

	for (type = (FilterType_E)0; type < FILTER_TYPE_MAX; type++)
	{
		Update_Filter_Remain_Sec(type);
		Update_Filter_Status(type);
	}

	if (Is_Filter_Test_Mode())
	{
		s_filter_test_sec++;
	}
}

/*******************************************************************************
 * @brief   更新滤芯剩余流量（用水时调用）
 * @param   type: 滤芯类型
 * @param   single_ml: 单次使用水量（毫升）
 ******************************************************************************/
void Update_Filter_Remain_ML(FilterType_E type, uint32_t single_ml)
{
	uint32_t step_ml = 0;
	uint32_t deduct = 0;
	
	if(Is_Filter_Test_Mode())
	{
		#if TEST_MODE_MIX_MODE
		if(s_filter_test_fast == false)
		{
			step_ml = 1;
		}
		else
		{
			if(s_filter_info_t[type].status == FILTER_LIFE_NORMAL)
			{
				step_ml = TEST_FLOW_ML_WATER_VOLUME;
			}
			else if(s_filter_info_t[type].status == FILTER_LIFE_WARNING)
			{
				step_ml = TEST_FLOW_ML_WATER_VOLUME_WARNIG;
			}
			else
			{
				step_ml = TEST_FLOW_ML_WATER_VOLUME;
			}
		}
		
		if(s_filter_info_t[type].status == FILTER_LIFE_NORMAL)
		{
			deduct = step_ml * single_ml;
		
			if(s_filter_info_t[type].remain_ml > deduct)
			{
				if(s_filter_info_t[type].remain_ml - deduct < FILTER_WARNING_THRESHOLD_ML)
				{
					s_filter_info_t[type].remain_ml = FILTER_WARNING_THRESHOLD_ML;
					return;
				}
			}
		}
		#else
		if (s_filter_test_fast == true)
		{
			step_ml = TEST_FLOW_ML_WATER_VOLUME;
		}
		else
		{
			step_ml = 1;
		}
		#endif
	}
	else
	{
		step_ml = 1;
	}

	deduct = step_ml * single_ml;
	if(s_filter_info_t[type].remain_ml > deduct)
	{
		s_filter_info_t[type].remain_ml -= deduct;
	}
	else
	{
		s_filter_info_t[type].remain_ml = 0;
	}
}


/*******************************************************************************
 * @brief   获取滤芯状态
 * @param   type: 滤芯类型
 * @return  滤芯当前状态
 ******************************************************************************/
FilterStatus_E Get_Filter_Status(FilterType_E type)
{
	return s_filter_info_t[type].status;
}

/*******************************************************************************
 * @brief   设置滤芯状态
 * @param   type: 滤芯类型
 * @param   status: 要设置的状态
 ******************************************************************************/
void Set_Filter_Status(FilterType_E type, FilterStatus_E status)
{
	s_filter_info_t[type].status = status;
}

/*******************************************************************************
 * @brief   获取单个滤芯所有属性
 * @param   type: 滤芯类型
 * @return  滤芯属性结构体指针
 ******************************************************************************/
FilterInfo_T *Get_Filter_All_Properties(FilterType_E type)
{
	return (&s_filter_info_t[type]);
}

/*******************************************************************************
 * @brief   复位单个滤芯信息
 * @param   type: 滤芯类型
 * @param   status: 复位后的初始状态
 * @return  /
 ******************************************************************************/
void Reset_One_Filter_Info(FilterType_E type, FilterStatus_E status)
{
	s_filter_info_t[type].type = type;
	
	s_filter_info_t[type].total_ml = Get_Filter_Capacity_ML(type);
	s_filter_info_t[type].remain_ml = s_filter_info_t[type].total_ml;
	
	s_filter_info_t[type].total_sec = Get_Filter_Capacity_Sec(type);
	s_filter_info_t[type].remain_sec= s_filter_info_t[type].total_sec;

	s_filter_info_t[type].status = status;
}

/*******************************************************************************
 * @brief   复位所有滤芯信息
 * @param   status: 复位后的初始状态
 * @return  /
 ******************************************************************************/
void Reset_All_Filter_Info(FilterStatus_E status)
{
	FilterType_E type;

	for (type = (FilterType_E)0; type < FILTER_TYPE_MAX; type++)
	{
		Reset_One_Filter_Info(type, status);
	}
}

/*******************************************************************************
 * @brief   恢复单个滤芯信息
 * @param   type: 滤芯类型
 * @param   filter: 滤芯信息源指针
 ******************************************************************************/
void Restore_One_Filter_Info(FilterType_E type, FilterInfo_T *filter)
{
	if (filter != NULL)
	{
		s_filter_info_t[type].type = type;
		
		s_filter_info_t[type].total_ml = filter->total_ml;
		s_filter_info_t[type].remain_ml = filter->remain_ml;
		
		s_filter_info_t[type].total_sec = filter->total_sec;
		s_filter_info_t[type].remain_sec = filter->remain_sec;
		
		s_filter_info_t[type].status = filter->status;
	}
}


/*******************************************************************************
 * @brief   设置滤芯工作模式
 * @param   mode: 要设置的模式
 * @return  /
 ******************************************************************************/
void Set_Filter_Mode(FilterMode_E mode)
{
	if (mode < FILTER_MODE_MAX)
	{
		s_filter_mode = mode;
	}
}

/*******************************************************************************
 * @brief   获取滤芯工作模式
 * @param   /
 * @return  当前滤芯工作模式
 ******************************************************************************/
FilterMode_E Get_Filter_Mode(void)
{
	return s_filter_mode;
}

/*******************************************************************************
 * @brief   获取滤芯快检累计时间
 * @param   /
 * @return  /
 ******************************************************************************/
uint32_t Get_Filter_Test_Time(void)
{
	return s_filter_test_sec;
}

/*******************************************************************************
 * @brief   开始滤芯快检
 * @param   mode: 快检模式（时间/流量）
 * @return  /
 ******************************************************************************/
void Filter_Test_Start(FilterMode_E mode)
{

	s_filter_test_sec = 0;

	if (mode < FILTER_MODE_MAX)
	{
		s_filter_mode = mode;
		s_filter_test_fast = true;
	}
}

/*******************************************************************************
 * @brief   滤芯快检加速使能/禁止
 * @param   mode: 使能/禁止
 * @return  /
 ******************************************************************************/
void Filter_Test_Boost(bool_t mode)
{
	s_filter_test_fast = mode;
}

/*******************************************************************************
 * @brief   判断是否为滤芯快检模式
 * @param   /
 * @return  0-否 1-是 
 ******************************************************************************/
uint8_t Is_Filter_Test_Mode(void)
{
	if(s_filter_mode != FILTER_MODE_NORMAL)
	{
		return 1;
	}

	return 0;
}
/**** (c) *************************** END OF FILE **/
