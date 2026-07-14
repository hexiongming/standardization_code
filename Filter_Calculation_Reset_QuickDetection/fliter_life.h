/*******************************************************************************
 * @file    fliter_life.h
 * @author  hexm
 * @version V1.0
 * @date    2026-02-20
 * @brief    滤芯寿命计算、复位、快检功能头文件
 * @attention 健康生活事业部 - 软件及 IOT 组
 ******************************************************************************/
/**
 * 移植需要修改的点：
 * 1、FilterType_E / FILTER_YEARS / FILTER_LITERS 滤芯类型和时长/流量配置，根据实际产品修改
 * 2、FILTER_WARNING_THRESHOLD_DAYS / FILTER_WARNING_THRESHOLD_DAYS 滤芯时长/流量预警阈值，按实际修改
 * 3、TEST_TIME_SEC_DAY / TEST_FLOW_ML_WATER_VOLUME 快检模式下的加速时间和流量，按实际调整
 * 
 * 
 * 本文件提供以下接口供外部调用：
 * 主用接口：
 * Update_All_Filter_Remain_Sec(void);									// 更新所有滤芯剩余时间及状态（1秒调用一次）
 * Update_Filter_Remain_ML(FilterType_E type, uint32_t single_ml);		// 更新单个滤芯剩余流量（用水时调用，传入单次出水量毫升）
 * Reset_One_Filter_Info(FilterType_E type, FilterStatus_E status);		// 复位单个滤芯信息（初始值，滤芯复位时调用）
 * Restore_One_Filter_Info(FilterType_E type, FilterInfo_T *filter);	// 恢复单个滤芯信息（上电从eeprom读取滤芯信息，或出厂复位后，恢复滤芯信息）
 * 辅助接口：
 * Reset_All_Filter_Info(FilterStatus_E status);						// 复位所有滤芯信息（初始值，首次上电或存储数据丢失时调用）
 * Update_Filter_Remain_Sec(FilterType_E type);							// 更新单个滤芯剩余时间（一般不用调用，由更新所有滤芯剩余时间调用）
 * Get_Filter_Status(FilterType_E type);								// 获取单个滤芯状态 // 新装未使用 正常 即将到期（提醒） 过期 过度使用（预留状态）
 * Set_Filter_Status(FilterType_E type, FilterStatus_E status);			// 设置单个滤芯状态 // 新装未使用 正常 即将到期（提醒） 过期 过度使用（预留状态）
 * Get_Filter_All_Properties(FilterType_E type);						// 获取单个滤芯所有属性（数据具体看业务需求）
 * Set_Filter_Mode(FilterMode_E mode);									// 设置滤芯工作模式（一般不常用，开机默认模式和关机模式一样都是断电，再上电默认就是正常模式）
 * Get_Filter_Mode(void);												// 获取滤芯工作模式（数据具体看业务需求）
 * 快检相关接口：
 * Filter_Test_Start(FilterMode_E mode);								// 开始滤芯快检（指定快检模式）
 * Get_Filter_Test_Time(void);											// 获取滤芯快检模式累计时间（秒）
 * Is_Filter_Test_Mode(void);											// 判断是否为滤芯快检模式
 * Filter_Test_Boost(bool_t mode);										// 滤芯快检加速使能/禁止
 */
#ifndef FLITER_LIFE_H
#define FLITER_LIFE_H

#include "stdint.h"
#include "stdbool.h"
#include "stdio.h"


#ifndef bool_t
typedef bool bool_t;
#endif


/** 
  * @addtogroup HAL
  * @{
  */

/** 
  * @addtogroup 滤芯寿命管理
  * @{
  */


// 时间单位转换
#define YEAR_TO_SEC(years)        ((uint32_t)(years) * 365U * 24U * 60U * 60U)
#define DAY_TO_SEC(years)         ((uint32_t)(years) * 24U * 60U * 60U)
#define LITER_TO_ML(liters)       ((uint32_t)(liters) * 1000U)

// 滤芯类型枚举
typedef enum {
	FILTER_TYPE_AQP,             // AQP滤芯
	FILTER_TYPE_3IN1,            // 3in1复合滤芯
	FILTER_TYPE_MNR,             // MNR滤芯
	FILTER_TYPE_MAX              // 滤芯类型数量（用于边界检查）
} FilterType_E;

// 不同滤芯类型的标准使用年限（单位：年）
#define FILTER_YEARS(type)        \
	((type) == FILTER_TYPE_AQP ? 10 : \
	 (type) == FILTER_TYPE_3IN1 ? 1 : \
	 (type) == FILTER_TYPE_MNR ? 2 : 1)

// 不同滤芯类型的标准使用流量（单位：升）
#define FILTER_LITERS(type)       \
	((type) == FILTER_TYPE_AQP ? 36500U : \
	 (type) == FILTER_TYPE_3IN1 ? 9000U : \
	 (type) == FILTER_TYPE_MNR ? 3650U : 4000U)

// 滤芯年限转换为秒（无法保证整型数值安全）
#define FILTER_SEC(type)          YEAR_TO_SEC(FILTER_YEARS(type))
// 滤芯流量L转换为ml（适合精确计算）
#define FILTER_ML(type)           LITER_TO_ML(FILTER_LITERS(type))

/************************** 预警阈值定义 **************************/
#define FILTER_WARNING_THRESHOLD_DAYS     14U     // 剩余14天时预警
#define FILTER_WARNING_THRESHOLD_LITERS   140U    // 剩余140L时预警

// 滤芯预警时间转换为秒
#define FILTER_WARNING_THRESHOLD_SEC     DAY_TO_SEC(FILTER_WARNING_THRESHOLD_DAYS)     
// 滤芯预警流量L转换为ml
#define FILTER_WARNING_THRESHOLD_ML      LITER_TO_ML(FILTER_WARNING_THRESHOLD_LITERS)    

/************************** 快检模式参数定义 **************************/
// 时间快检换算（1秒代表N天）
#define TEST_TIME_SEC_DAY               5U                                      // 1秒代表5天
#define TEST_TIME_SEC_SEC               DAY_TO_SEC(TEST_TIME_SEC_DAY)                // 1秒对应的实际秒数

#define TEST_MODE_MIX_MODE         0
#if TEST_MODE_MIX_MODE
#define TEST_TIME_SEC_DAY_WARNIG        1U                                      // 1秒代表1天-预警提醒状态
#define TEST_TIME_SEC_SEC_WARNIG        DAY_TO_SEC(TEST_TIME_SEC_DAY_WARNIG)         // 1秒对应的实际秒数-预警提醒状态
#endif
// 流量快检换算
#define TEST_FLOW_ML_WATER_VOLUME            3500U                                // 1ml代表3500ml
#if TEST_MODE_MIX_MODE
#define TEST_FLOW_ML_WATER_VOLUME_WARNIG     400U                                 // 1ml代表400ml-预警提醒状态
#endif



/************************** 枚举类型定义 **************************/
// 滤芯工作模式枚举
typedef enum {
	FILTER_MODE_NORMAL,           // 正常模式
	FILTER_MODE_TIME_TEST,        // 快检模式-时间
	FILTER_MODE_MINERAL_TEST,     // 快检模式-矿物水
	FILTER_MODE_PURE_TEST,        // 快检模式-纯水
	FILTER_MODE_LIVING_TEST,      // 快检模式-生活水
	FILTER_MODE_MAX               // 模式数量（用于边界检查）
} FilterMode_E;


// 滤芯寿命状态枚举
typedef enum {
	FILTER_LIFE_FACTORY_NEW,   // 新装未使用
	FILTER_LIFE_NORMAL,        // 正常
	FILTER_LIFE_WARNING,       // 即将到期（提醒）
	FILTER_LIFE_EXPIRED,       // 过期
	FILTER_LIFE_OVER_USED      // 过度使用（预留状态）
} FilterStatus_E;


/************************** 结构体定义 **************************/
// 滤芯完整信息结构体
typedef struct 
{
	FilterType_E  type;            // 滤芯类型
	uint32_t      total_ml;        // 总容量（毫升）
	uint32_t      remain_ml;       // 剩余使用量（毫升）    
	uint32_t      total_sec;       // 总时长（秒）
	uint32_t      remain_sec;      // 剩余使用时间（秒）        
	FilterStatus_E status;         // 当前状态
} FilterInfo_T;


/************************** 函数声明 **************************/

// 更新所有滤芯剩余时间（1秒调用一次）
void Update_All_Filter_Remain_Sec(void);

// 更新单个滤芯剩余时间
void Update_Filter_Remain_Sec(FilterType_E type);

// 更新单个滤芯剩余流量
void Update_Filter_Remain_ML(FilterType_E type, uint32_t single_ml);

// 获取滤芯状态
FilterStatus_E Get_Filter_Status(FilterType_E type);

// 设置滤芯状态
void Set_Filter_Status(FilterType_E type, FilterStatus_E status);

// 获取单个滤芯所有属性
FilterInfo_T *Get_Filter_All_Properties(FilterType_E type);

// 复位所有滤芯信息
void Reset_All_Filter_Info(FilterStatus_E status);

// 复位单个滤芯信息
void Reset_One_Filter_Info(FilterType_E type, FilterStatus_E status);

// 恢复单个滤芯信息
void Restore_One_Filter_Info(FilterType_E type, FilterInfo_T *filter);

// 设置滤芯工作模式
void Set_Filter_Mode(FilterMode_E mode);

// 获取滤芯工作模式
FilterMode_E Get_Filter_Mode(void);

// 开始滤芯快检
void Filter_Test_Start(FilterMode_E mode);

// 获取滤芯快检时间（秒）
uint32_t Get_Filter_Test_Time(void);

// 判断是否为滤芯快检模式
uint8_t Is_Filter_Test_Mode(void);

// 滤芯快检加速使能/禁止
void Filter_Test_Boost(bool_t mode);

/**
  * @}
  */

/**
  * @}
  */

#endif
/**** (c) *************************** END OF FILE **/

