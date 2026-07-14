
#include "FinvalidMeter.h"

/* 要求：
1、脉冲数换算出流量值
*/

#define IN_FLOWMETER_IO		P01
// 流量计参数配置（需根据实际流量计手册修改）
#define PULSE_FACTOR        450.0   // 脉冲当量：每升多少脉冲（例如：450脉冲/升）
#define FLOW_UNIT           1000.0  // 单位换算：升转毫升（1L = 1000mL）

unsigned int g_FinvalidMeterValue = 0;	//全局变量，用来脉冲值计算



//获取IO口的状态
unsigned char FinvalidMete_Io_Back(void)
{
    unsigned char InputState = 0;

    InputState =  IN_FLOWMETER_IO;

    return InputState;
}

//该函数使用轮询方式，调用在125us中断函数，需要看调用时间能否覆盖脉冲时间
void FinvalidMeter_Calculate(void)
{
    static unsigned char New_State = 0;
    static unsigned char Old_State = 0;

    New_State = FinvalidMete_Io_Back();

    if(New_State != Old_State)
    {
        Old_State = New_State;
        g_FinvalidMeterValue++;		//注意：g_FlinvalideterValue需要清零
    }
}

// 计算总流量（升）
// 调用时机：需要读取流量时调用
float Calculate_TotalFinvalid(void)
{
    float TotalFinvalid = 0.0;	//全局变量，总流量值（单位：升）

    TotalFinvalid = (float)g_FinvalidMeterValue / PULSE_FACTOR;

    return TotalFinvalid;
}

// 计算总流量（毫升）
float Calculate_TotalFinvalid_ML(void)
{
    return Calculate_TotalFinvalid() * FLOW_UNIT;
}
