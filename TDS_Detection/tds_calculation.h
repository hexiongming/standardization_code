/**
 *****************************************************************************
 * @file    tds_calculation.h
 * @author  hexm
 * @version V1.0.0
 * @date    2026-04-5
 * @brief   TDS计算模块头文件
 * @attention 健康生活事业部 - 软件及 IOT 组
 *****************************************************************************/
/**
 * 需要适配修改点：（最大支持8个通道）
 * 1.新增通道要改tds_calculation.c tds_config 数组；
 * 2.新增通道要改tds_calculation.h TDS_Channel_e 枚举值；
 * 3.新增通道要改tds_calculation.h TDS_Channel_Port_e 枚举值；
 * 4.新增通道要改tds_calculation.h TDS_CHANNEL_COUNT 宏定义；
 * 5.MCU不同硬件平台要改tds_calculation.h GPIO操作函数及定义，并引入硬件操作的库文件,示例见 tds_gpio.c / tds_gpio.h；
 * 
 * 使用方式：
 * 1.在125us的中断中调用tds_start() 传入TDS的通道，即可使TDS探针处于工作模式，传入参数如TDS_CH_1 或 TDS_CH_ALL;
 * 2.在125us的中断中调用tds_ad_sample_point() 传入需要采样的通道，获取是否达到采样点，不允许同时传入两个或以上通道
 * 如返回 AD_POSITIVE 则调用AD_get_value() 获取对应通道的AD值，此时采样的是正极的电压
 * 如返回 AD_NEGATIVE 则调用AD_get_value() 获取对应通道的AD值，此时采样的是负极的电压
 * 如返回 AD_DISEABLE 则不进行AD采样
 * 进行AD采样还需满足 is_tds_start() == TDS_CH_x && USER_GPIO_READ(TDS_CH_x_P_PORT) == 1
 * 3.将需要计算的通道、采样滤波后的正极和负极的AD值、环境温度传入tds_get_value() 返回TDS值；
 */


#ifndef TDS_CALCULATION_H
#define TDS_CALCULATION_H

#include "stdint.h"
#include "stdbool.h"
#include "stdio.h"
#include "math.h"

//#include "tds_gpio.h"

#ifndef bool_t
typedef bool bool_t;
#endif


/** 
  * @addtogroup HAL
  * @{
  */

/** 
  * @addtogroup TDS检测
  * @{
  */


/**
 * @brief 根据通道进行GPIO操作（MCU	IO操作适配改这里）
 * @param ch传入的参数是 TDS_Channel_Port_e 的枚举值
 * @note  需要自己根据传入的参数实现对应TDS引脚的GPIO操作
 */
#define USER_GPIO_OUT(ch)		TDS_PORT_Init_OUT(ch)
#define USER_GPIO_IN(ch)		TDS_PORT_Init_IN(ch)
#define USER_GPIO_SET(ch)		TDS_PORT_GPIO_SET(ch)
#define USER_GPIO_CLR(ch)		TDS_PORT_GPIO_CLR(ch)
#define USER_GPIO_XOR(ch)		TDS_PORT_GPIO_XOR(ch)
#define USER_GPIO_READ(ch)		TDS_PORT_GPIO_READ(ch)


// ========================= 常量定义 =========================
/**
 * @brief TDS计算相关常量
 * @note  根据硬件电路参数调整，确保与实际电阻/电压匹配
 */
#define VCC                  4096.0f        // 参考电压(AD满量程值，对应实际电压如3.3V/5V)
#define R_REF_POS            1000.0f        // 正极参考电阻(Ω)
#define R_REF_NEG            100.0f         // 负极参考电阻(Ω)
#define ELECTRODE_RESISTANCE 1100.0f        // 电极内阻(Ω，需根据实际电极校准)
#define TEMP_COEFFICIENT     0.02f          // 温度补偿系数(每度变化，25°C为基准)
#define TDS_CONVERSION       0.65f          // TDS转换系数(电导率转TDS，通常为0.5~0.7)

#define TDS_PERIOD           4       // TDS驱动周期-125us (1kHz方波)
#define TDS_VALUE_MAX        999     // TDS最大值(ppm)
#define TDS_VALUE_MIN        0       // TDS最小值(ppm)

#define TDS_SHOW_MAX        199     // TDS显示最大值
#define TDS_SHOW_MIN        3       // TDS显示最小值


#define TDS_CHANNEL_COUNT	2		//定义的通道数
// ========================= 枚举定义 =========================
/**
 * @brief TDS通道枚举
 * @note  支持单独控制通道1/2，或同时控制所有通道
 */
typedef enum 							//增加通道改这里
{
	TDS_CH_NULL = 0,
	TDS_CH_1 = (1U << 0),				// TDS通道1
	TDS_CH_2 = (1U << 1),				// TDS通道2
	TDS_CH_ALL = TDS_CH_1 | TDS_CH_2	// 所有TDS通道
/*新增通道示例：
	TDS_CH_3,							// TDS通道3
	TDS_CH_ALL = TDS_CH_1 | TDS_CH_2 | TDS_CH_3	// 所有TDS通道
*/
} TDS_Channel_e;



typedef enum 							//增加通道改这里
{
	TDS_CH_NULL_PORT = 0,
	TDS_CH_1_P_PORT,				// TDS通道1正极IO口
	TDS_CH_1_N_PORT,				// TDS通道1负极IO口
	TDS_CH_2_P_PORT,				// TDS通道2正极IO口
	TDS_CH_2_N_PORT,				// TDS通道2负极IO口
/*新增通道示例：
	TDS_CH_3_P_PORT,				// TDS通道3正极IO口
	TDS_CH_3_N_PORT,				// TDS通道3负极IO口
*/
} TDS_Channel_Port_e;


/**
 * @brief 电压采样类型
 * @note  支持单独控制通道1/2，或同时控制所有通道
 */
typedef enum 
{
	AD_DISEABLE,
	AD_POSITIVE,		// 正极采样点
	AD_NEGATIVE,		// 负极采样点
} AD_Sample_Point_e;

// ========================= 结构体定义 =========================
typedef struct 
{
	TDS_Channel_Port_e	e_PortP;			// 正极采样通道
	TDS_Channel_Port_e	e_PortN;			// 负极采样通道
	bool_t    			b_StartFlag;  		// 指向静态启动标志
	bool_t    			b_SampleFlag; 		// 指向静态采样标志
} TDS_Config_t;

// ========================= 函数声明 =========================
/**
 * @brief  启动指定TDS通道驱动
 */
void Tds_Start(TDS_Channel_e ch);

/**
 * @brief  停止指定TDS通道驱动
 */
void Tds_Stop(TDS_Channel_e ch);

/**
 * @brief 返回已启动的TDS通道
 */
TDS_Channel_e Tds_IsStart(void);

/**
 * @brief  判断是否为TDS采样点
 */
AD_Sample_Point_e Tds_GetSamplePoint(TDS_Channel_e ch);

/**
 * @brief  获取指定TDS通道的测量值
 */
uint16_t Tds_GetValue(TDS_Channel_e ch, float posADvalue, float negADvalue, uint8_t temperature);


/**
  * @}
  */

/**
  * @}
  */
#endif

/**** (c) *************************** END OF FILE **/
