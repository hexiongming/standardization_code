/**
 *****************************************************************************
 * @file    tds_calculation.c
 * @author  hexm
 * @version V1.0.0
 * @date    2026-04-5
 * @brief   TDS计算模块，该文件包含tds_calculation.h中的函数声明
 * @attention 健康生活事业部 - 软件及 IOT 组
 ****************************************************************************/

 
/* 头文件 */
#include "tds_calculation.h"



// ========================= 静态变量定义 =========================
static uint8_t s_TdsCnt = 0;				// TDS计数器(每1kHz计数一次)


// 通道配置：GPIO引脚、状态标志。每个通道需要在此处添加对应IO配置
// 注意：数组索引 0 对应 TDS_CH_1 (位移0), 索引 1 对应 TDS_CH_2 (位移1)
static TDS_Config_t s_TdsConfig[TDS_CHANNEL_COUNT] = 
{
	// 通道1
	{
		TDS_CH_1_P_PORT,
		TDS_CH_1_N_PORT,
		false,
		false,
	},
	// 通道2
	{
		TDS_CH_2_P_PORT,
		TDS_CH_2_N_PORT,
		false,
		false,
	},
/*新增通道示例：
	// 通道3
	{
		TDS_CH_3_P_PORT,
		TDS_CH_3_N_PORT,
		false,
		false,
	},
*/
};


// ========================= 静态函数实现 =========================
/*******************************************************************************
 * @brief  计算水电阻值
 * @details 根据电极电压和极性，计算水的电阻值(扣除电极电阻)
 * @param  electrodeVoltage: 电极电压(AD原始值)
 * @param  polarity: 极性标识
 *         @arg 1: 正半周采样
 *         @arg 0: 负半周采样
 * @return float: 水电阻值(欧姆)
 * @note   此处处理电压值防止除零错误，保证计算稳定
******************************************************************************/
static float Tds_CalcWaterResistance(float electrodeVoltage, uint8_t polarity) 
{
	float RRef = R_REF_POS;             // 参考电阻值(欧)
	float voltageDivRatio = 0.0f;  // 分压比(AD值/VCC)
	float totalRes = 0.0f;       // 总电阻值(欧)
	float preTotalRes = 0.0f;    // 预计算总电阻值(AD值相关)
	
	if(electrodeVoltage > VCC)
		electrodeVoltage = VCC;	
	// 根据极性处理电压值
	if (polarity)
	{
		preTotalRes = VCC - electrodeVoltage;
	}
	else
	{
		preTotalRes = electrodeVoltage;
	} 
	
	// 计算分压比和总电阻
	voltageDivRatio = preTotalRes / VCC;
	if(voltageDivRatio <= 0)
		return 0;
	totalRes = RRef / voltageDivRatio;

	// 计算水电阻值(总电阻减去电极电阻)
	return (totalRes - ELECTRODE_RESISTANCE);
}

/*******************************************************************************
 * @brief  计算TDS值
 * @details 根据正负AD值和温度值，计算TDS值(未进行限幅处理)
 * @param  posADvalue: 正极AD值
 * @param  negADvalue: 负极AD值
 * @param  temp: 温度值(摄氏度，用于温度补偿)
 * @return uint16_t: TDS值(未限幅)
 * @note   1. 返回值未限幅范围，需外部调用者处理范围限制
 *         2. 电导率 = 1000000 / 水电阻 (uS/cm)
 *         3. 温度补偿: 以25度为标准，每度补偿2%
 *         4. TDS = 电导率 * 0.5 (ppm)
******************************************************************************/
static uint16_t Tds_CalcValue(float posADvalue, float negADvalue, uint8_t temp) 
{
	float RPos = 0.0f;                  // 正半周水电阻(欧)
	float RNeg = 0.0f;                  // 负半周水电阻(欧)
	float RWater = 0.0f;                // 水电阻平均值(欧)
	float conductivity = 0.0f;           // 电导率(uS/cm)
	float compCoef = 0.0f;              // 温度补偿系数
	float compCond = 0.0f;      // 温度补偿后电导率(uS/cm)
	
	// 计算正负半周水电阻
	RPos = Tds_CalcWaterResistance(posADvalue, 1);
	RNeg = Tds_CalcWaterResistance(negADvalue, 0);

	// 计算水电阻几何平均值(取绝对值防止负数，保证平方根内有效)
	RWater = sqrtf(fabsf(RPos * RNeg));
	
	// 计算电导率(uS/cm)，防止除以零
	if (RWater > 0)
	{
		conductivity = 1000000.0f / RWater;
	}
	else
	{
		conductivity = 0.0f;
	}
	
	// 温度补偿(25度为标准温度)
	compCoef = 1.0f + TEMP_COEFFICIENT * ((float)temp - 25.0f);
	if(compCoef <= 0)
		return 0;	
	compCond = conductivity / compCoef;
	
	// 转换为TDS值(ppm)并取整
	return (uint16_t)(compCond * TDS_CONVERSION);
}


/*******************************************************************************
 * @brief  启动指定TDS通道方波
 * @details 该函数产生指定TDS通道的方波信号，需在125us定时中断中调用
 *          每次调用计数器累加，达到周期值时翻转电平产生1kHz方波信号
 * @param  ch: 要启动的TDS通道
 *         @arg TDS_CH_1: 启动通道1
 *         @arg TDS_CH_2: 启动通道2
 *         @arg TDS_CH_ALL: 启动所有通道
 * @return 无
 * @note   需在定时中断中每125us调用一次，否则方波频率会不准
 * @note   首次调用时初始化引脚电平，后续调用翻转电平以生成方波
******************************************************************************/
void Tds_Start(TDS_Channel_e ch)
{
	if(ch > TDS_CH_ALL || ch == TDS_CH_NULL)
	{
		return ;
	}
	// 计数器达到周期值时更新方波
	if (++s_TdsCnt >= TDS_PERIOD)
	{
		s_TdsCnt = 0;

		for (uint8_t i = 0; i < TDS_CHANNEL_COUNT; i++)
        {
            TDS_Channel_e e_CurrentMask = (TDS_Channel_e)(1U << i);

            if (ch & e_CurrentMask)
            {
                TDS_Config_t *p_Cfg = &s_TdsConfig[i]; 

                if (p_Cfg->b_StartFlag == 0)
                {
                    USER_GPIO_SET(p_Cfg->e_PortP);
                    USER_GPIO_CLR(p_Cfg->e_PortN);
                    p_Cfg->b_StartFlag = 1;
                    p_Cfg->b_SampleFlag = 1; 
                }
                else
                {
                    p_Cfg->b_SampleFlag = !p_Cfg->b_SampleFlag;
                    USER_GPIO_XOR(p_Cfg->e_PortP);
                    USER_GPIO_XOR(p_Cfg->e_PortN);
                }
            }
        }

	}
}

/*******************************************************************************
 * @brief  停止指定TDS通道方波
 * @details 该函数用于停止指定TDS通道的方波输出，由外部调用
 * @param  ch: 要停止的TDS通道
 *         @arg TDS_CH_1: 停止通道1
 *         @arg TDS_CH_2: 停止通道2
 *         @arg TDS_CH_ALL: 停止所有通道
 * @return 无
 * @note   停止后将清空通道启动标志和采样标志
 ******************************************************************************/
void Tds_Stop(TDS_Channel_e ch)
{
	if(ch > TDS_CH_ALL || ch == TDS_CH_NULL)
	{
		return ;
	}
	for (uint8_t i = 0; i < TDS_CHANNEL_COUNT; i++)
	{
		TDS_Channel_e e_CurrentMask = (TDS_Channel_e)(1U << i);

		if (ch & e_CurrentMask)
		{
			TDS_Config_t *p_Cfg = &s_TdsConfig[i]; 
			USER_GPIO_CLR(p_Cfg->e_PortP);
			USER_GPIO_CLR(p_Cfg->e_PortN);
			p_Cfg->b_StartFlag = 0;
			p_Cfg->b_SampleFlag = 0;
		}
	}
	// 重置计数器
	s_TdsCnt = 0;
}

/*******************************************************************************
 * @brief  判断指定TDS通道是否已启动
 * @details 返回所有已启动通道的掩码状态
 * @param  无
 * @return 返回已启动通道的掩码
 *			@retval 0: 无通道启动
 *			@retval 1: 通道1启动
 * 			@retval 2: 通道2启动
 * 			@retval 3: 通道1/2都启动
 * @note   使用时配合 TDS_Channel_e 枚举判断
 ******************************************************************************/
TDS_Channel_e Tds_IsStart(void)
{
	TDS_Channel_e ch = TDS_CH_NULL;
	for(uint8_t i = 0; i < TDS_CHANNEL_COUNT; i++)
	{
		if (s_TdsConfig[i].b_StartFlag == true)
        {
            ch |= (TDS_Channel_e)(1U << i);
        }
	}
	return ch;
}

/*******************************************************************************
 * @brief  判断是否为TDS采样点
 * @details 检测当前是否到达TDS方波周期的中间位置(信号稳定时)
 * @param  ch: 要判断的TDS通道
 *         @arg TDS_CH_1: 通道1
 *         @arg TDS_CH_2: 通道2
 * @return 返回值
 *         @retval AD_POSITIVE: 是采样点，可读取ADC值
 *         @retval AD_NEGATIVE: 是采样点，可读取ADC值
 *         @retval AD_DISABLE: 不是采样点
 * @note   采样点位于方波周期的中间位置，此时信号稳定，适合ADC采样
 ******************************************************************************/
AD_Sample_Point_e Tds_GetSamplePoint(TDS_Channel_e ch)
{
	if(ch >= TDS_CH_ALL || ch == TDS_CH_NULL)
	{
		return AD_DISEABLE;
	}
	// 在周期中点进行采样
	if(s_TdsCnt == (TDS_PERIOD >> 1))
	{
		uint8_t idx = 0;
		TDS_Channel_e temp = ch;
		while ((temp & 1U) == 0) 
		{
			temp >>= 1;
			idx++;
			// 防止移位溢出导致死循环
        	if (idx > TDS_CHANNEL_COUNT) 
				return AD_DISEABLE; 
		}
		const TDS_Config_t *p_Cfg = &s_TdsConfig[idx];
		return p_Cfg->b_SampleFlag? AD_POSITIVE : AD_NEGATIVE;	
	}
	else
	{
		return AD_DISEABLE;
	}	
}

/*******************************************************************************
 * @brief  获取指定TDS通道的测量值
 * @details 获取指定通道的ADC值，结合温度值计算并返回TDS值
 *          会自动限制结果在 TDS_VALUE_MIN~TDS_VALUE_MAX 范围内
 * @param  ch: 要读取的TDS通道 (仅支持单通道掩码)
 *         @arg TDS_CH_1: 读取通道1的TDS值
 *         @arg TDS_CH_2: 读取通道2的TDS值
 * @param  posADvalue: 正极AD值(经过TDS驱动)
 * @param  negADvalue: 负极AD值(经过TDS驱动)
 * @param  temperature: 温度补偿值(摄氏度)，用于TDS计算的温度校正
 * @return uint16_t: TDS测量值(范围：0~999 ppm)
 * @note   调用前请确保：
 *         1. 已正确ADC采样并传入 posADvalue/negADvalue
 *         2. 已传入有效的 temperature 值(推荐范围：0~80℃)
 ******************************************************************************/
uint16_t Tds_GetValue(TDS_Channel_e ch, float posADvalue, float negADvalue, uint8_t temperature)
{
	uint8_t idx = 0;
	TDS_Channel_e temp = ch;

	if(ch >= TDS_CH_ALL || ch == TDS_CH_NULL)
	{
		return TDS_VALUE_MIN;
	}
	
	while ((temp & 1U) == 0) 
	{
		temp >>= 1;
		idx++;
		// 防止移位溢出导致死循环
		if (idx > TDS_CHANNEL_COUNT) 
			return TDS_VALUE_MIN; 
	}
	const TDS_Config_t *p_Cfg = &s_TdsConfig[idx];

	if (!p_Cfg->b_StartFlag)
		return TDS_VALUE_MIN;

	uint16_t val = Tds_CalcValue(posADvalue, negADvalue, temperature);

	val = (val >= TDS_VALUE_MAX) ? TDS_VALUE_MAX : val;
	val = (val <= TDS_VALUE_MIN) ? TDS_VALUE_MIN : val;
	return val;

}

/**** (c) *************************** END OF FILE **/
