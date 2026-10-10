/*******************************************************************************
 * @file    iot_user_app.c
 * @author  hexm
 * @version V1.0.0
 * @date    2026-04-20
 * @brief   IoT 业务应用层（应用层）
 * @attention 健康生活事业部 - 软件及 IOT 组
 *
 * 上报规则：
 *   1. 属性与上次不同时立即上报
 *   2. 全属性每 1 小时上报一次（连云成功后立即上报一次）
 *   3. OTA 下载期间停止上报属性
 *   4. 上电 3 秒后才开始上报（模组上电初始化需要 3 秒）
 *
 * 新增上报字段说明：
 *   1. . 在本文件 s_attrTable[] 数组末尾追加一行
 *      s_attrTable 添加的顺序要与 DeviceParam_e 的顺序一致
******************************************************************************/
#include "iot_user_app.h"



/* ========================= 私有变量 ========================= */
static uint32_t s_DiffReportCounter      = 0;   /* 差异属性上报间隔计数器*/
static uint32_t s_fullReportCounter  = 0;       /* 全属性上报间隔计数器*/
static uint16_t s_bootCounter        = 0;       /* 上电延迟计数器*/
static uint16_t s_CheckExistCounter  = 0;       /* 检测WiFi模块存在计数器*/
static uint8_t s_CheckExistNumber  = 0;	        /* 检测WiFi模块存在计数器*/

static uint16_t s_McuOtaCounter  = 0;           /* mcu ota时间计数器*/
static uint16_t s_AllReportFlag     = 0;        /* 全属性上报标志*/
static NetTime_t s_NetTime;                     /* 网络时间*/
static FactoryResult_e s_FactoryResult;         /* 产测结果*/
static NetWorkStatus_e s_NetStatus = NET_STATUS_OFFLINE;                 /* 网络状态*/
static NetWork_Config_Status_e s_NetWorkConfigStatus = NET_CONFIG_NULL;  /* 配网状态*/
static RouterStatus_t s_RouterStatus = ROUTER_STATUS_NULL;               /* 连接路由器状态*/
static char app_mcu_Ver[32];                                             /* MCU 在APP显示的版本信息*/	
/* -----------------------------------------------------------------------
 * 外部变量声明或定义，若已在外部文件定义和声明，则包含头文件
 * ----------------------------------------------------------------------- */
char g_mcu_Manufacturer[32]="Philips";
char g_device_Series[32] = "MargaritaIOT";
char g_device_ModelID[32] = "Margarita01"; 
char g_iot_ErrorCode[5] = "E00";

/* -----------------------------------------------------------------------
 * 属性描述符表（上报属性注册表）
 * 根据实际修改此注册表
 * 新增上报字段只需在此处追加一行，格式：
 *   { "JsonKey", param, IOT_ATTR_INT, false, true, NULL }  — 只读只上报
 *   { "JsonKey", param, IOT_ATTR_INT, true,  true,  NULL }  — 可读写
 *   { "JsonKey", param, IOT_ATTR_INT, true,  true, CallBack_xxx } — 可读写+回调
 * 只支持常量的字符串类型，如果需上报可变字符串，请单独实现上报函数;
 * ----------------------------------------------------------------------- */
static const IOT_AttrEntry_t s_attrTable[]=
{
	//jsonkey,               param,                   type,            write,    allReport   
	{"Ver",                  VER,                     IOT_ATTR_STR,    false,      true,      },
	{"McuManufacturer",      MCUMANUFACTURER,         IOT_ATTR_STR,    false,      true,      },
	{"Status",               STATUS,                  IOT_ATTR_INT,    false,      true,      },
	{"InputTemp",            INPUTTEMP,               IOT_ATTR_INT,    false,      true,      },
	{"OutputTemp",           OUTPUTTEMP,              IOT_ATTR_INT,    false,      true,      },
	{"SetTemp",              SETTEMP,                 IOT_ATTR_INT,    true,       true,      },
	{"SetOutlet",            SETOUTLET,               IOT_ATTR_INT,    true,       true,      },
	{"TotalOutlet",          TOTALOUTLET,             IOT_ATTR_INT,    false,      true,      },
	{"PureTDS",              PURETDS,                 IOT_ATTR_INT,    false,      true,      },
	{"RawTDS",               RAWTDS,                  IOT_ATTR_INT,    false,      true,      },
	{"WarmWaterSet",         WARMWATERSET,            IOT_ATTR_INT,    true,       true,      },
	{"HotWaterSet",          HOTWATERSET,             IOT_ATTR_INT,    true,       true,      },
	{"Outlet120Set",         OUTLET120SET,            IOT_ATTR_INT,    true,       true,      },
	{"Outlet350Set",         OUTLET350SET,            IOT_ATTR_INT,    true,       true,      },
	{"Outlet800Set",         OUTLET800SET,            IOT_ATTR_INT,    true,       true,      },
	{"SterilizerState",      STERILIZERSTATE,         IOT_ATTR_INT,    true,       true,      },
	{"RefrigerationMode",    REFRIGERATIONMODE,       IOT_ATTR_STR,    true,       true,      },
	{"MAXFilterReset",       MAXFILTERRESET,          IOT_ATTR_INT,    true,       false,     },
	{"PC5in1FilterReset",    PC5IN1FILTERRESET,       IOT_ATTR_INT,    true,       false,     },
	{"NightModeSet",         NIGHTMODE0,              IOT_ATTR_INT,    true,       false,     },
	{"NightModeRepeatWeek",  NIGHTMODE1,              IOT_ATTR_STR,    true,       false,     },
	{"NightModeStartHour",   NIGHTMODE2,              IOT_ATTR_INT,    true,       false,     },
	{"NightModeEndHour",     NIGHTMODE3,              IOT_ATTR_INT,    true,       false,     },
	{"NightModeStartMinute", NIGHTMODE4,              IOT_ATTR_INT,    true,       false,     },
	{"NightModeEndMinute",   NIGHTMODE5,              IOT_ATTR_INT,    true,       false,     },
	{"NightModeNoProduce",   NIGHTMODE6,              IOT_ATTR_INT,    true,       false,     },
	{"NightModeNoCooling",   NIGHTMODE7,              IOT_ATTR_INT,    true,       false,     },
	{"NightModeOpenLight",   NIGHTMODE8,              IOT_ATTR_INT,    true,       false,     },
	{"NightModeNoSound",     NIGHTMODE9,              IOT_ATTR_INT,    true,       false,     },

	{"aiPlan",               AIPLAN,                  IOT_ATTR_JSON,   true,       false,     },

	{"ErrorCode",            ERRORCODE,               IOT_ATTR_STR,    false,      false,     },
	{"Warning",              WARNING,                 IOT_ATTR_INT,    false,      false,     },

	{"IceCubeKanban",       ICECUBEKANBAN,            IOT_ATTR_ARRAY,  true,       false,     },

	
	/*END----新增字段在此行之前追加----*/
};
#define ATTR_TABLE_SIZE  (sizeof(s_attrTable) / sizeof(s_attrTable[0]))
static IOT_paramValue_u s_paramValue[ATTR_TABLE_SIZE] = {0};
/*
 * 下发指令的属性值为json格式的属性表
 */
typedef enum
{
	NONE,
    VERSION, 
	COUNT,   
	COMMANDS,
} Cmd_JsonParam_e;

static const IOT_AttrEntry_t s_AIplanTable[]=
{
	//jsonkey,        param,          type,              write,    allReport   
	{"version",      VERSION,         IOT_ATTR_STR,      true,      true,      },
	{"count",        COUNT,           IOT_ATTR_INT,      true,      true,      },
	{"commands",     COMMANDS,        IOT_ATTR_ARRAY,    true,      true,      },
};
#define JSON_ATTR_SIZE  (sizeof(s_AIplanTable) / sizeof(s_AIplanTable[0]))
static IOT_paramValue_u s_AIPlanParamValue[JSON_ATTR_SIZE] = {0};

static const  IOT_Json_Attr_t s_jsonTable[]=
{//如有其它JSON格式的属性值，在末尾继续追加
	{AIPLAN,     s_AIplanTable,    JSON_ATTR_SIZE,    s_AIPlanParamValue},
};
#define JSON_TABLE_SIZE  (sizeof(s_jsonTable) / sizeof(s_jsonTable[0]))

/********************************************************************************
 * @brief   APP下发的设备指令数据功能执行操作函数
 * @details 针对所有属性命令进行下发的命令执行
 * @param   param: 下发的设备属性枚举
 * @param   JsonParam: 下发的设备JSON格式属性的下层枚举
 * @param   int_value: 下发命令整型数据值
 * @param   point_value: 下发命令字符型/json数据值
 * @return  void
 * @brief   兼容整型和字符型命令的解析，根据实际数据类型进行应用和修改
 *******************************************************************************/
static void Device_Cmd_Process(uint8_t param, uint8_t JsonParam, int32_t int_value, const char *point_value)
{
	switch (param)
	{
		case SETTEMP://示例 根据实际需要更改
			if (int_value == 1)
			{
				//设置水温
			}
			else if (int_value == 0)
			{
				//关闭加热
			}
			break;

		case SETOUTLET:
			/* code */
			break;

		case WARMWATERSET:
			/* code */
			break;

		case ICECUBEKANBAN:
			IOT_Lib_Send_ATCmd("IceCubeKanban=%s", point_value);
			IOT_UserApp_ActiveReport(ICECUBEKANBAN);
			break;	
			
		case REFRIGERATIONMODE://示例 根据实际需要更改
			if (point_value != NULL)
			{
				IOT_Lib_Send_ATCmd("AT+MQTTSEND=%s", point_value);
			}
			break;
		case AIPLAN:			//属性值为JSON格式的命令往下层继续解析得到的属性值
			if (VERSION == JsonParam)
			{
				IOT_Lib_Send_ATCmd("version=%s", point_value);
			}
			else if (COUNT == JsonParam)
			{
				IOT_Lib_Send_ATCmd("count=%d", int_value);
			}
			else if (COMMANDS == JsonParam)
			{
				IOT_Lib_Send_ATCmd("commands=%s", point_value);
			}
			IOT_UserApp_ActiveReport(AIPLAN);
			break;

		case NIGHTMODE0:
			IOT_Lib_Send_ATCmd("NIGHTMODE0");
			break;
		case NIGHTMODE1:
			IOT_Lib_Send_ATCmd("NIGHTMODE1");
			break;
		case NIGHTMODE2:
			IOT_Lib_Send_ATCmd("NIGHTMODE2");
			break;
		case NIGHTMODE3:
			IOT_Lib_Send_ATCmd("NIGHTMODE3");
			break;
		case NIGHTMODE4:
			IOT_Lib_Send_ATCmd("NIGHTMODE4");
			break;
		case NIGHTMODE5:
			IOT_Lib_Send_ATCmd("NIGHTMODE5");
			break;
		case NIGHTMODE6:
			IOT_Lib_Send_ATCmd("NIGHTMODE6");
			break;
		case NIGHTMODE7:
			IOT_Lib_Send_ATCmd("NIGHTMODE7");
			break;
		case NIGHTMODE8:
			IOT_Lib_Send_ATCmd("NIGHTMODE8");
			break;	
		case NIGHTMODE9:
			IOT_Lib_Send_ATCmd("NIGHTMODE9");
			break;			
			

		default:
			break;
	}
}

/********************************************************************************
 * @brief   设备属性的参数值获取更新函数，由应用层变量赋值给属性描述符表的value
 * @details 针对所有属性命令进行下发的命令执行
 * @param   param: 设备属性枚举
 * @return  设备属性值
 * @brief   仅支持整型变量更新，字符型变量以变量地址传入，只要变量地址不变，就不需重新获取
 *          禁止传给value.strValue赋值局部变量的地址，否则将导致数据丢失
 *******************************************************************************/
static IOT_paramValue_u Device_ParamValue_Get(uint8_t param, uint8_t jsonParam)
{
	IOT_paramValue_u value = {0};
	static uint8_t test_value = 0;

	switch (param)
	{
		case VER://示例 根据实际需要更改
			value.strValue = app_mcu_Ver;
			break;

		case MCUMANUFACTURER://示例 根据实际需要更改
			value.strValue = g_mcu_Manufacturer;
			break;

		case SETTEMP://示例 根据实际需要更改
			value.intValue = test_value ++;
			break;

		case SETOUTLET:
			/* code */
			break;

		case WARMWATERSET:
			/* code */
			break;

		case HOTWATERSET:
			/* code */
			break;

		case ERRORCODE://示例 根据实际需要更改
			value.strValue = g_iot_ErrorCode;
			break;

		case ICECUBEKANBAN:		//示例 数组变量
			value.strValue = "[[0,0,0,1,3,7,0,0,60,1],[1,0,0,2,3,20,0,3,0,0],[1,1,0,2,2,20,30,0,0,1],[1,0,0,2,3,20,0,3,0,0]]";
			break;

		case AIPLAN:			//属性值为JSON格式的命令往下层继续解析得到的属性值
			if (VERSION == jsonParam)
			{
				value.strValue = "2025652256500025";
			}
			else if (COUNT == jsonParam)
			{
				value.intValue = 5;
			}
			else if (COMMANDS == jsonParam)
			{
				value.strValue = "[1,\"0000254568845\",0,2]";
			}

		default:
			break;
	}

	return value;
}


#if HAVE_FILTER
/* -----------------------------------------------------------------------
 * 滤芯寿命注册表
 * 根据实际修改此注册表
 * 新增上报字段只需在此处追加一行，格式：
 *  <JsonKey><X>,<FX>,<Y>,<FY>,<Z>,<FZ>  — 只上电时上报
 * 当该滤芯寿命到达X%时，上报告警代码FX
 * 当该滤芯寿命到达Y%时，上报的告警代码FY
 * 当该滤芯寿命到达Z%时，上报的告警代码FZ
 * ----------------------------------------------------------------------- */
static const FilterLifeConfig_t s_filterLifeConfig[] = {
    // JsonKey               X   FX		Y   FY   
    { "MAXFilterLife",       0, "F18",	5, "F19"},
    { "PC5in1FilterLife",    0, "F33",	5, "F34"},
    /* END---- 新增滤芯配置在此行之前追加 ---- */
};

#define FILTER_LIFE_CONFIG_COUNT  (sizeof(s_filterLifeConfig) / sizeof(s_filterLifeConfig[0]))

/* -----------------------------------------------------------------------
 * 滤芯复位名称映射表（数据驱动设计）
 * 根据实际修改此配置表
 * 新增复位字段只需在此处追加一行，格式：
 *   { "ResetKey", "DisplayName" }
 * 说明：
 *   - ResetKey: 云端下发的复位指令键名
 *   - DisplayName: 对应的滤芯显示名称（用于复位通知）
 * ----------------------------------------------------------------------- */
static const FilterResetConfig_t s_filterResetConfig[] = {
    // ResetKey              DisplayName
    { "MAXFilterReset",      "MAX" 		},
    { "PC5in1FilterReset",   "PC5in1"	},
    /* END---- 新增配置在此行之前追加 ---- */
};

#define FILTER_RESET_CONFIG_COUNT  (sizeof(s_filterResetConfig) / sizeof(s_filterResetConfig[0]))
#endif

/**************************************************** 以下无需/禁止修改 *******************************************************/

/* ========================= JSON 构造工具 ========================= */

/********************************************************************************
 * @brief   初始化 JSON 构建器，
 * @details buf 格式：MQTTSEND=N,{...}\r\n
 *          Begin 时先写入 "{" 
 *          End 时先填}再拼接MQTTSEND=N,
 *          jsonStart 记录 '{' 的位置，pos 从 '{' 后一位开始。
 * @param   JsonBuff: JSON 构建器指针
 * @return  void
 *******************************************************************************/
static void JsonBuff_Begin(JsonBuilder_t *JsonBuff)
{
	if (JsonBuff == NULL)
	{
		return;
	}
	const char *header = "{";
	uint16_t hlen = (uint16_t)strlen(header);
	memcpy(JsonBuff->buf, header, hlen);
	JsonBuff->jsonStart = hlen - 1;   /* '{' 的位置 */
	JsonBuff->pos       = hlen;       /* '{' 后一位，即第一个字段写入位置 */
	JsonBuff->hasField  = false;
	JsonBuff->buf[JsonBuff->pos] = '\0';
}

/********************************************************************************
 * @brief   闭合当前帧，回填 JSON 长度，入队后重置构建器
 * @details JSON 长度 = pos - jsonStart（含 '{' 和 '}'）
 *          回填到头部的3位长度字段，不足3位左补空格，超过3位截断（不应发生）。
 *          空包（hasField=false）直接重置，不入队。
 * @param   JsonBuff: JSON 构建器指针
 * @return  void
 *******************************************************************************/
static void JsonBuff_End(JsonBuilder_t *JsonBuff)
{
	if (JsonBuff == NULL)
	{
		return;
	}

	if (!JsonBuff->hasField)
	{
		JsonBuff_Begin(JsonBuff);
		return;
	}

	/* 追加 '}' 和 '\0' */
	JsonBuff->buf[JsonBuff->pos++] = '}';
	JsonBuff->buf[JsonBuff->pos]   = '\0';

	/* 计算 JSON 长度（从 '{' 到 '}' 的字节数） */
	uint16_t jsonLen = JsonBuff->pos - JsonBuff->jsonStart; 
    
	IOT_Lib_Send_ATCmd("MQTTSEND=%u,%s", jsonLen, JsonBuff->buf);

}

/********************************************************************************
 * @brief   向 JSON 构建器追加整型字段（超包时自动分包）
 * @details 追加前预估总帧长，若超过 IOT_PKG_MAX_SIZE 则先闭合当前帧入队，
 *          再在新帧中追加该字段
 * @param   JsonBuff: JSON 构建器指针
 * @param   key: JSON 字段名
 * @param   val: 整型字段值
 * @return  void
 *******************************************************************************/
static void JsonBuff_AddInt(JsonBuilder_t *JsonBuff, const char *key, int32_t val)
{
	char tmp[IOT_PKG_MAX_SIZE];

	if (JsonBuff == NULL || key == NULL)
	{
		return;
	}

	int n = snprintf(tmp, sizeof(tmp), "%s\"%s\":%ld",
					 JsonBuff->hasField ? "," : "", key, (long)val);

	memcpy(JsonBuff->buf + JsonBuff->pos, tmp, n);
	JsonBuff->pos += n;
	JsonBuff->hasField = true;
}

/********************************************************************************
 * @brief   向 JSON 构建器追加字符串字段（超过 IOT_PKG_MAX_SIZE 时自动分包）
 * @param   JsonBuff: JSON 构建器指针
 * @param   key: JSON 字段名
 * @param   val: 字符串字段值
 * @return  void
 *******************************************************************************/
static void JsonBuff_AddStr(JsonBuilder_t *JsonBuff, const char *key, const char *val)
{
	char tmp[IOT_PKG_MAX_SIZE];

	if (JsonBuff == NULL || key == NULL)
	{
		return;
	}

	int n = snprintf(tmp, sizeof(tmp), "%s\"%s\":\"%s\"",
					 JsonBuff->hasField ? "," : "", key, val);

	memcpy(JsonBuff->buf + JsonBuff->pos, tmp, n);
	JsonBuff->pos += n;
	JsonBuff->hasField = true;
}

/********************************************************************************
 * @brief   向 JSON 构建器追加数组型字段
 * @param   JsonBuff: JSON 构建器指针
 * @param   key: JSON 字段名
 * @param   val: 数组值,若是二维数组，需包含最外层的[],如[[1,0,"000221125000",0],[1,0,"000221125000",0]]
 * @return  void
 *******************************************************************************/
static void JsonBuff_AddArray(JsonBuilder_t *JsonBuff, const char *key, const char *val)
{
	char tmp[IOT_PKG_MAX_SIZE];

	if (JsonBuff == NULL || key == NULL)
	{
		return;
	}

	int n = snprintf(tmp, sizeof(tmp), "%s\"%s\":%s",
					 JsonBuff->hasField ? "," : "", key, val);

	memcpy(JsonBuff->buf + JsonBuff->pos, tmp, n);
	JsonBuff->pos += n;
	JsonBuff->hasField = true;
}
/* ========================= JSON 解析工具 ========================= */

/********************************************************************************
 * @brief   从 JSON 字符串中提取整型字段值
 * @details 搜索 "key": 模式，跳过空格后解析十进制整数，
 *          支持正负数，不依赖第三方 JSON 库
 * @param   p: 属性值起始地址
 * @param   e_param:  属性名称
 * @return  0:解析失败，1:解析成功
 * @note    解析成功后执行数据处理和上报执行结果
 *******************************************************************************/
static uint8_t Json_Int_Value_Deal(const char *p, uint8_t e_param, Cmd_JsonParam_e JsonParam)
{
	if (p == NULL )
        return 0;

	while (*p == ' ') 
		p++;  /* 跳过空格 */

	char *endPtr;
    int32_t val = strtol(p, &endPtr, 10);

    /* 检查是否解析到了任何数字 */
    if (endPtr == p) 
        return 0;

	Device_Cmd_Process(e_param, JsonParam, val, NULL);
	return 1;
}
/********************************************************************************
 * @brief   从 JSON 字符串中提取字符串字段值
 * @details 搜索 "key":"  模式，提取两个双引号之间的内容，
 *          自动追加 '\0' 终止符
 * @param   p:   属性值起始地址
 * @param   e_param:  属性名称
 * @return  0:解析失败，1:解析成功
 * @note    仅做简单线性搜索，不支持转义字符
 *******************************************************************************/
static uint8_t Json_Str_Value_Deal(const char *p, uint8_t e_param, Cmd_JsonParam_e JsonParam)
{
	char outBuf[IOT_RX_CMD_MAX_SIZE];
	uint16_t i = 0;

	if (p == NULL || outBuf == NULL)
        return 0;

	p = strchr(p, '"');
	if(p)
	{	
		p++; //跳过'"'，去获取实际参数
	}
	while (*p != '"' && *p != '\0' && (i < sizeof(outBuf) - 1))
	{
		outBuf[i++] = *p++;
	}
	outBuf[i] = '\0';

	Device_Cmd_Process(e_param, JsonParam, 0, outBuf);
	return 1;
}

/********************************************************************************
 * @brief   从 JSON 字符串中提取数组型字段值
 * @details 搜索 "key":"  模式，提取两个双引号之间的内容，
 *          自动追加 '\0' 终止符
 * @param   p:   属性值起始地址
 * @param   e_param:  属性名称
 * @return  0:解析失败，1:解析成功
 * @note    仅做简单线性搜索，不支持转义字符
 *******************************************************************************/
static uint8_t Json_Array_Value_Deal(const char *p, uint8_t e_param, Cmd_JsonParam_e JsonParam)
{
	char outBuf[IOT_RX_CMD_MAX_SIZE];
	uint16_t i = 0;

	if (p == NULL || outBuf == NULL)
        return 0;

	p = strchr(p, '[');

	while (*p != '}' && *p != '\0' && (i < sizeof(outBuf) - 1))
	{
		outBuf[i++] = *p++;
	}
	outBuf[i] = '\0';

	Device_Cmd_Process(e_param, JsonParam, 0, outBuf);
	return 1;
}

/********************************************************************************
 * @brief   从 JSON 字符串中提取字符串字段值
 * @details 搜索 "key":"  模式，提取两个双引号之间的内容，
 *          自动追加 '\0' 终止符
 * @param   p:   属性值起始地址
 * @param   e_param:  属性名称
 * @return  0:解析失败，1:解析成功
 * @note    仅做简单线性搜索，不支持转义字符
 *******************************************************************************/
static uint8_t Json_JSON_Value_Deal(const char *p, uint8_t e_param)
{
	const char *json = p;
	const char *jsonStart;

	if (json == NULL)
        return 0;

	p = strchr(json, '{');
	if(json)
	{	
		json++; //跳过'{'，去获取实际参数
	}

	jsonStart = json;
	for (uint8_t i = 0; i < JSON_TABLE_SIZE; i++)
	{
		const IOT_Json_Attr_t *json_attr = &s_jsonTable[i];

		if (e_param == json_attr->param)
		{
			const IOT_AttrEntry_t *attr_array = &json_attr->json_attr[i];
			for (uint8_t n = 0; n < json_attr->json_attr_count; n++)
			{
				char search[64];
				const IOT_AttrEntry_t *e = &attr_array[n];
				

				snprintf(search, sizeof(search), "\"%s\":", e->key);
				const char *p = strstr(jsonStart, search);					//下发指令与属性表key值比较，相同则继续，不同则比较下一个
				if (p == NULL) 
					continue;

				if (!e->write_enable) 
					continue;
				
				p += strlen(search);
				if(e->type == IOT_ATTR_INT)
				{
					Json_Int_Value_Deal(p, e_param, (Cmd_JsonParam_e)e->param);
				}
				else if(e->type == IOT_ATTR_STR)
				{
					Json_Str_Value_Deal(p, e_param, (Cmd_JsonParam_e)e->param);
				}
				else if(e->type == IOT_ATTR_ARRAY)
				{
					Json_Array_Value_Deal(p, e_param, (Cmd_JsonParam_e)e->param);
				}

				jsonStart = p;
			}
		}
	}
	return 1;
}

/* ========================= 云端指令处理 ========================= */

/********************************************************************************
 * @brief   解析并执行云端下发的 JSON 指令
 * @details 遍历 s_attrTable[]，对 writable=true 的字段尝试从 JSON 中提取值并写入，
 *          写入后若挂有 CallBack 回调则调用执行额外动作。
 *          新增可写字段只需在 s_attrTable[] 中将 writable 置 true，无需修改本函数。
 * @param   json: 云端下发的 JSON 字符串（来自 +MQTTRECV 帧）
 * @return  void
 *******************************************************************************/
static void IOT_MQTTRECV_Handle(const char *json)
{
	const char *jsonStart = json;

	if (json == NULL) 
		return;
	
	char *endPtr;
	// 尝试解析开头的数字
	uint16_t jsonLen = (uint16_t)strtoul(json, &endPtr, 10);//获取下发属性数据的字符串长度
	if(jsonLen < 3)
		return;
	// 如果解析成功且后面紧跟逗号，说明存在前缀 "NNN,"
	if (endPtr != json && *endPtr == ',') 
	{
		jsonStart = endPtr + 1; // 指向真正的 JSON 起始位置 '{'
	}

	int cmd_count = 0;			//计算下发的一条json中包含多少条指令
    const char *pt = jsonStart;
    while ((pt = strchr(pt, ':')) != NULL)
    {
        cmd_count++;
        pt++;    // 跳过当前找到的冒号，向后继续搜索
    }

	for (uint16_t i = 0; i < ATTR_TABLE_SIZE; i++)
	{
		char search[64];
		const IOT_AttrEntry_t *e = &s_attrTable[i];
		
		snprintf(search, sizeof(search), "\"%s\":", e->key);
		const char *p = strstr(jsonStart, search);					//下发指令与属性表key值比较，相同则继续，不同则比较下一个
		if (p == NULL) 
			continue;

		if (!e->write_enable) 
			continue;

		p += strlen(search);
		cmd_count--;		//命中后剩余命令数-1
		if(e->type == IOT_ATTR_INT)
		{
			Json_Int_Value_Deal(p, e->param, NONE);
			//IOT_UserApp_ActiveReport(e->param);             //主动上报该控制命令的执行结果；
		}
		else if(e->type == IOT_ATTR_STR)
		{
			Json_Str_Value_Deal(p, e->param, NONE);
			//IOT_UserApp_ActiveReport(e->param);             //主动上报该控制命令的执行结果；
		}
		else if(e->type == IOT_ATTR_JSON)
		{
			Json_JSON_Value_Deal(p, e->param);
		}
		else if(e->type == IOT_ATTR_ARRAY)
		{
			Json_Array_Value_Deal(p, e->param, (Cmd_JsonParam_e)e->param);
		}
		
		if(0 == cmd_count) //已解析完最后一条指令
			break;
		else
			jsonStart = p;
	}
}

/* ========================= 上报逻辑 ========================= */
#if HAVE_FILTER
/********************************************************************************
 * @brief   注册滤芯寿命推送阈值
 * @details 发送 AT+FLTPROCESS=<jsonKey>,<x>,<fx>,<y>,<fy>[,<z>,<fz>]\r\n，
 *          当设备上报的滤芯寿命字段到达指定百分比时，
 *          WiFi 模组会通过独立通道向云端推送对应告警码
 * @param   jsonKey: 滤芯寿命字段名，如 "MAXFilterLife"
 * @param   x:  第一阈值百分比（如 0 表示 0%）
 * @param   fx: 第一阈值告警码（如 "F0"）
 * @param   y:  第二阈值百分比
 * @param   fy: 第二阈值告警码
 * @param   z:  第三阈值百分比，传 -1 表示仅使用两个阈值点
 * @param   fz: 第三阈值告警码，z=-1 时忽略此参数
 * @return  void
 * @note    须在每次上电时调用，每个滤芯字段单独注册一次；
 *******************************************************************************/
static void FilterRegisrter(void)
{
	/* 遍历滤芯寿命配置表，自动注册所有滤芯的阈值 */
	for (uint8_t i = 0; i < FILTER_LIFE_CONFIG_COUNT; i++)
	{
		const FilterLifeConfig_t *cfg = &s_filterLifeConfig[i];

		IOT_Lib_Send_ATCmd("FLTPROCESS=%s,%d,%s,%d,%s",cfg->jsonKey, cfg->x, cfg->fx, cfg->y, cfg->fy);
	}
}
/********************************************************************************
 * @brief   注册滤芯寿命推送阈值
 * @details 发送 AT+FLTPROCESS=<jsonKey>,<x>,<fx>,<y>,<fy>[,<z>,<fz>]\r\n，
 *          当设备上报的滤芯寿命字段到达指定百分比时，
 *          WiFi 模组会通过独立通道向云端推送对应告警码
 * @param   jsonKey: 滤芯寿命字段名，如 "MAXFilterLife"
 * @param   x:  第一阈值百分比（如 0 表示 0%）
 * @param   fx: 第一阈值告警码（如 "F0"）
 * @return  void
 * @note    须在每次上电时调用，每个滤芯字段单独注册一次；
 *******************************************************************************/
static void FilterResetRegisrter(void)
{
	uint16_t offset = 0;
	int ret;
	char txBuf[IOT_UART_TX_BUF_SIZE];

	/* 遍历滤芯复位配置表，自动注册所有滤芯NAME */
	ret = snprintf(txBuf, sizeof(txBuf), "FLTRESET=");
	offset = (uint16_t)ret;
	for (uint8_t i = 0; i < FILTER_RESET_CONFIG_COUNT; i++)
	{
		const FilterResetConfig_t *cfg = &s_filterResetConfig[i];

		/* 计算剩余空间，防止溢出 */
		uint16_t remaining = sizeof(txBuf) - offset;
		if (remaining <= 1) 
			break; /* 空间不足 */
		if (i > 0)
		{
			// 检查是否有空间写逗号
			if (remaining < 2) 
				break; 
			txBuf[offset++] = ',';
			remaining--; // 更新剩余空间
		}
		ret = snprintf(txBuf + offset, remaining, "%s,%s",cfg->resetKey, cfg->displayName);
		if (ret < 0 || ret >= remaining)
			break; /* 追加失败或空间不足 */
			
		offset += (uint16_t)ret;
	}

	IOT_Lib_Send_ATCmd(txBuf);
}
#endif

/********************************************************************************
 * @brief   全属性上报
 * @details 遍历 s_attrTable[] 构建包含所有属性字段的 JSON 并通过 MQTT 发送，
 *          同时将每个字段的当前值写入 lastVal 快照。
 *          触发时机：连云成功后立即调用一次；此后每小时定时调用一次
 * @param   void
 * @return  void
 * @note   所有属性上报一次
 *******************************************************************************/
static void IOT_UserApp_ReportAll(void)
{
	static uint16_t s_reportIndex = 0;
	JsonBuilder_t JsonBuff;
	JsonBuff_Begin(&JsonBuff);

	for (uint16_t i = s_reportIndex; i < ATTR_TABLE_SIZE; i++)
	{
		if (IOT_TXQueue_Size_Return() == 0)
		{
			return;
		}
		const IOT_AttrEntry_t *e = &s_attrTable[i];
		if(e->allReport_enable)
		{
			if ((e->type == IOT_ATTR_STR) || (e->type == IOT_ATTR_ARRAY))
			{
				char *p_StrVal = Device_ParamValue_Get(e->param, NONE).strValue;
				if(p_StrVal == NULL)
				{
					s_reportIndex = i + 1;
					continue;
				}
				/* 16(AT+MQTTSEND=xxx,) + pos + e->key + e->p_StrVal + 7(":""}\0\r\n") 是否超过总帧上限，如果超过，则立即闭合当前帧入队，此次处理的数据放到下一包 */
				if ((16 + JsonBuff.pos + strlen(e->key) + strlen((char *)p_StrVal) + 7) > IOT_PKG_MAX_SIZE)
				{
					break;
				}
				JsonBuff_AddStr(&JsonBuff, e->key, (const char *)p_StrVal);
				//s_paramValue[i].strValue = p_StrVal;
			}
			else if (e->type == IOT_ATTR_INT)
			{
				int32_t cur = Device_ParamValue_Get(e->param, NONE).intValue;
				/* 16(AT+MQTTSEND=xxx,) + pos + e->key + cur + 5(":}\0\r\n") 是否超过总帧上限，如果超过，则立即闭合当前帧入队，此次处理的数据放到下一包 */
				if ((16 + JsonBuff.pos + strlen(e->key) + sizeof(cur) + 5) > IOT_PKG_MAX_SIZE)
				{
					break;
				}
				JsonBuff_AddInt(&JsonBuff, e->key, cur);
				s_paramValue[i].intValue = cur;
			}
			else
			{
				s_reportIndex = i + 1;
				continue;
			}
		}
		s_reportIndex = i + 1;
	}

    if (s_reportIndex >= ATTR_TABLE_SIZE)
    {
		s_AllReportFlag = 0; //清除上报标志
        s_reportIndex = 0;	//全部上报完毕，重置索引
    }

	JsonBuff_End(&JsonBuff);  /* 将最后一包（或唯一一包）闭合入队 */
}


/********************************************************************************
 * @brief   差异属性上报
 * @details 遍历 s_attrTable[]，将每个字段的当前值与 lastVal 快照比较，
 *          s_DiffReportCounter 达到IOT_REPORT_INTERVAL_MS时，
 *          仅上报发生变化的字段，减少不必要的网络流量
 * @param   void
 * @return  void
 * @note    无变化字段时不发送任何数据
 *******************************************************************************/
static void IOT_UserApp_ReportDiff(void)
{
	static uint16_t s_reportIndex = 0;
	JsonBuilder_t JsonBuff;
	JsonBuff_Begin(&JsonBuff);

	for (uint16_t i = s_reportIndex; i < ATTR_TABLE_SIZE; i++) 
	{
		if (IOT_TXQueue_Size_Return() == 0)
		{
			return;
		}
		const IOT_AttrEntry_t *e = &s_attrTable[i];
		if (e->type == IOT_ATTR_INT)
		{
			int32_t cur = Device_ParamValue_Get(e->param, NONE).intValue;
			if (cur != s_paramValue[i].intValue)
			{/* 16(AT+MQTTSEND=xxx,) + pos + e->key + cur + 5(":}\0\r\n") 是否超过总帧上限，如果超过，则立即闭合当前帧入队，此次处理的数据放到下一包 */
				if ((16 + JsonBuff.pos + strlen(e->key) + sizeof(cur) + 5) > IOT_PKG_MAX_SIZE)
				{
					break;
				}
				JsonBuff_AddInt(&JsonBuff, e->key, cur);
				s_paramValue[i].intValue = cur;
			}
		}
		else
		{
			//字符串不支持比较上报，如有需要单独上报
		}
		s_reportIndex = i + 1;
	}
	if (s_reportIndex >= ATTR_TABLE_SIZE)
    {
        s_reportIndex = 0;	//全部上报完毕，重置索引
    }

	if (JsonBuff.hasField) 
	{
		JsonBuff_End(&JsonBuff);  /* 将最后一包闭合入队 */
	}
}

/********************************************************************************
 * @brief   主动上报某些属性
 * @details  主动上报 s_attrTable[]属性表的值
 * @param   DeviceParam_e
 * @return  void
 * @note    需要主动上报某个属性的时候调用，每次只能上报一个属性
 *******************************************************************************/
void IOT_UserApp_ActiveReport(uint8_t param)
{
	JsonBuilder_t JsonBuff;
	JsonBuff_Begin(&JsonBuff);

	const IOT_AttrEntry_t *e = &s_attrTable[param];

	if (e->type == IOT_ATTR_STR || e->type == IOT_ATTR_ARRAY)
	{
		const char *p_StrVal = Device_ParamValue_Get(e->param, NONE).strValue;
		if(p_StrVal == NULL)
			return;
		/* 16(AT+MQTTSEND=xxx,) + pos + e->key + e->p_StrVal + 7(":""}\0\r\n") 是否超过总帧上限，如果超过，则立即闭合当前帧入队，此次处理的数据放到下一包 */
		if ((16 + JsonBuff.pos + strlen(e->key) + strlen((char *)p_StrVal) + 7) > IOT_PKG_MAX_SIZE)
		{
			return;
		}
		if(e->type == IOT_ATTR_STR)
		{	
			JsonBuff_AddStr(&JsonBuff, e->key, (const char *)p_StrVal);
		}
		else if(e->type == IOT_ATTR_ARRAY)
		{
			JsonBuff_AddArray(&JsonBuff, e->key, (const char *)p_StrVal);
		}
		
	}
	else if (e->type == IOT_ATTR_INT)
	{
		uint32_t cur = Device_ParamValue_Get(e->param, NONE).intValue;
		/* 16(AT+MQTTSEND=xxx,) + pos + e->key + cur + 5(":}\0\r\n") 是否超过总帧上限，如果超过，则立即闭合当前帧入队，此次处理的数据放到下一包 */
		if ((16 + JsonBuff.pos + strlen(e->key) + sizeof(cur) + 5) > IOT_PKG_MAX_SIZE)
		{
			return;
		}
		JsonBuff_AddInt(&JsonBuff, e->key, cur);
		s_paramValue[param].intValue = cur;
	}
	else if (e->type == IOT_ATTR_JSON)
	{
		char tmp[128];
		int n = snprintf(tmp, sizeof(tmp), "\"%s\":{", e->key);

		memcpy(JsonBuff.buf + JsonBuff.pos, tmp, n);
		JsonBuff.pos += n;

		for (uint8_t i = 0; i < JSON_TABLE_SIZE; i++)
		{
			const IOT_Json_Attr_t *json_attr = &s_jsonTable[i];
			if (param == json_attr->param)
			{
				const IOT_AttrEntry_t *attr_array = &json_attr->json_attr[i];
				for (uint8_t n = 0; n < json_attr->json_attr_count; n++)
				{
					const IOT_AttrEntry_t *e = &attr_array[n];
					if(e->allReport_enable)
					{
						if (e->type == IOT_ATTR_STR || e->type == IOT_ATTR_ARRAY)
						{
							char *p_StrVal = Device_ParamValue_Get(json_attr->param,e->param).strValue;
							if(p_StrVal == NULL)
								continue;
							/* 16(AT+MQTTSEND=xxx,) + pos + e->key + e->p_StrVal + 7(":""}\0\r\n") 是否超过总帧上限，如果超过，则立即闭合当前帧入队，此次处理的数据放到下一包 */
							if ((16 + JsonBuff.pos + strlen(e->key) + strlen((char *)p_StrVal) + 7) > IOT_PKG_MAX_SIZE)
							{
								break;
							}
							if(e->type == IOT_ATTR_STR)
							{	
								JsonBuff_AddStr(&JsonBuff, e->key, (const char *)p_StrVal);
							}
							else if(e->type == IOT_ATTR_ARRAY)
							{
								JsonBuff_AddArray(&JsonBuff, e->key, (const char *)p_StrVal);
							}
						}
						else if(e->type == IOT_ATTR_INT)
						{
							int32_t cur = Device_ParamValue_Get(json_attr->param,e->param).intValue;
							/* 16(AT+MQTTSEND=xxx,) + pos + e->key + cur + 5(":}\0\r\n") 是否超过总帧上限，如果超过，则立即闭合当前帧入队，此次处理的数据放到下一包 */
							if ((16 + JsonBuff.pos + strlen(e->key) + sizeof(cur) + 5) > IOT_PKG_MAX_SIZE)
							{
								break;
							}
							JsonBuff_AddInt(&JsonBuff, e->key, cur);
							s_paramValue[i].intValue = cur;
						}
					}
				}
			}
		}
	}
	else
	{
		return;
	}

	if (JsonBuff.hasField) 
	{
		JsonBuff_End(&JsonBuff);  /* 将最后一包闭合入队 */
	}
}

/********************************************************************************
 * @brief   MCU版本号上报
 * @details 上电连云成功后上报一次
 * @param   void
 * @return  void
 * @note    需要使用时调用
 *******************************************************************************/
static void IOT_UserApp_CheckMCUOTA(void)
{/* 单独上报 mcu version*/
	IOT_Lib_Send_ATCmd("MCUOTACHECK=%s",app_mcu_Ver);
}

/********************************************************************************
 * @brief   获取星期函数
 * @details 通过年、月、日获取星期
 * @param   年、月、日
 * @return  week 0-6
 * @note    在更新时间时调用
 *******************************************************************************/
static uint8_t IOT_GetWeek(uint16_t year, uint8_t month, uint8_t day)
{
    uint8_t  week;

    /*==================== 基姆拉尔森公式 ====================*/
    if ( month == 1 || month == 2 )
    {
        month += 12;
        year--;
    }

    // 公式输出：0-6 周日-周六
    week = (day + 2 * month + 3 * (month + 1)/5 + year + year/4 - year/100 + year/400 + 1) % 7;

    return (uint8_t)week;
}
/********************************************************************************
 * @brief   模组下发的网络时间解析
 * @details 在调用 IOT_GetNetTime()后，模组返回的结果
 * @param   +GETTIME:关键字后的属性参数
 * @return  void
 * @note    需要使用时调用
 *******************************************************************************/
static void IOT_UserApp_UpdateTime(const char* para_buf)
{
	if (para_buf == NULL) 
	{
		return;
	}

	// 1. 匹配网络错误响应：+GETTIME:NetworkError
	if (strstr(para_buf, "NetworkError") != NULL) 
	{
		s_NetTime.result = TIME_NETWORK_ERROR;
		return;
	}
	int year, month, day, hour, minute, second;
	int ret = sscanf(para_buf, "%4d-%2d-%2d-%2d:%2d:%2d",
					&year, &month, &day,
					&hour, &minute, &second);

	// sscanf成功解析6个参数，说明格式正确
	if (ret == 6) 
	{
		s_NetTime.result = TIME_PARSE_SUCCESS;
		s_NetTime.year   = (uint16_t)year;
		s_NetTime.month  = (uint8_t)month;
		s_NetTime.day    = (uint8_t)day;
		s_NetTime.hour   = (uint8_t)hour;
		s_NetTime.minute = (uint8_t)minute;
		s_NetTime.second = (uint8_t)second;
		s_NetTime.week  = IOT_GetWeek(s_NetTime.year, s_NetTime.month, s_NetTime.day);
		return;
	}

	//既不是错误也不是合法时间格式
	s_NetTime.result = TIME_FORMAT_ERROR;
	return;
}

/********************************************************************************
 * @brief   模组返回的产测结果解析
 * @details 在调用 IOT_FactorTest()后，模组返回的结果
 * @param   +GETTIME:关键字后的属性参数
 * @return  void
 * @note    需要使用时调用
 *******************************************************************************/
static void IOT_UserApp_UpdateFactoryResult(const char* para_buf)
{
	if (para_buf == NULL) 
	{
		return;
	}

	// 1. 正在产测中响应：+FACEVENT:TESTING
	if (strstr(para_buf, "TESTING") != NULL) 
	{
		s_FactoryResult = FACTORY_TESTING;
	}
	else if (strstr(para_buf, "SUCCESS") != NULL) 
	{
		s_FactoryResult = FACTORY_SUCCESS;
	}
	else if (strstr(para_buf, "FAIL") != NULL) 
	{
		s_FactoryResult = FACTORY_FAIL;
	}
}

void Mcu_OtaSetFlag_Reboot (void)
{
	//写flash/eeprom 的ota标志
	//重启进boot进行ymodem协议传输固件
	//在重启进入boot后将 mcu_ota_data.e_mcu_ota_state = MCU_OTA_STATE_DOWNLOAD_DONE; ,再调用 Mcu_Ota_Ymodem_Handle();
}

/********************************************************************************
 * @brief   IOT 事件统一处理回调
 * @details 由 iot_lib 层在解析到完整事件后调用，根据事件类型分发处理：
 *          - MQTT 连云成功：触发全属性上报
 *          - 云端下发：调用 IOT_MQTTRECV_Handle 解析执行
 *          - WiFi 状态变化：通知 UI 层更新指示灯（TODO）
 *          - OTA 下载：暂停上报，保护 Flash 操作
 *          - MCU OTA：版本比较、触发下载、断电重传处理
 * @param   evt: 解析完成的事件结构体指针，由 iot_lib 层传入
 * @return  void
 * @note    在主循环上下文中执行，禁止在此函数内做长时间阻塞操作
 *******************************************************************************/
static void IOT_Event_Handle(const IOT_Event_t *evt)
{
	switch (evt->e_type)
	{
		case IOT_EVT_AT_OK:
				
			break;

		case IOT_EVT_MQTT_CONNECT_OK:
			if (NET_CONFIG_ENTER == s_NetWorkConfigStatus)
			{
				s_NetWorkConfigStatus = NET_CONFIG_DONE;
			}
		
			if (s_NetStatus != NET_STATUS_ONLINE)
			{
				s_NetStatus =  NET_STATUS_ONLINE;

				/* 连云成功立即上报一次 ErrorCode + Warning + MCU版本号 */
				IOT_UserApp_ActiveReport(ERRORCODE);
				IOT_UserApp_ActiveReport(WARNING);
				/* 检查 MCU OTA 版本 */
				IOT_UserApp_CheckMCUOTA();
			}
		
			break;

		case IOT_EVT_WIFI_AP_UP:
			s_NetWorkConfigStatus = NET_CONFIG_ENTER;
			break;

		case IOT_EVT_WIFI_CONNECTING:
			s_NetStatus =  NET_STATUS_CONNECTING;
			break;

		case IOT_EVT_WIFI_CONNECTED:
			s_NetStatus =  NET_STATUS_CONNECTTED;
			break;

		case IOT_EVT_WIFI_TIMEOUT:
			s_NetWorkConfigStatus = NET_CONFIG_TIMEOUT;
			break;

		case IOT_EVT_WIFI_UNBIND_SUCCESS:
			s_NetStatus =  NET_STATUS_OFFLINE;

			break;
		case IOT_EVT_WIFI_STATION_UP:
			s_CheckExistNumber = 0;
			s_RouterStatus = ROUTER_STATUS_STATION_UP;
			break;

		case IOT_EVT_WIFI_STATION_DOWN:
			s_CheckExistNumber = 0;
			s_RouterStatus =  ROUTER_STATUS_STATION_DOWN;
			break;

		case IOT_EVT_MQTT_RECV:
			/* TODO: 属性下发解析 */
			s_NetStatus =  NET_STATUS_ONLINE;
			IOT_MQTTRECV_Handle(evt->para_buf);
			break;

		case IOT_EVT_GET_TIME:
			/* TODO: 网络时间解析 */
			IOT_UserApp_UpdateTime(evt->para_buf);
			break;	

		case IOT_EVT_FACTORYTEST:
			/* TODO: 产测结果解析 */
			IOT_UserApp_UpdateFactoryResult(evt->para_buf);
			break;	

		case IOT_EVT_OTA_PROGRESS:
			/* TODO: WiFi OTA 过程处理*/
			s_NetStatus =  NET_STATUS_OFFLINE;
			break;

		case IOT_EVT_OTA_SUCCESS:
			/* TODO: WiFi OTA 升级成功*/
			
			break;

		case IOT_EVT_MCUOTA_VERSION:
			/* 比较版本，决定是否下载 */
			if (strcmp(evt->para_buf, app_mcu_Ver) != 0)
			{
				Mcu_Ota_Set_State(MCU_OTA_STATE_WAIT_DOWNLOAD);
				IOT_Lib_Send_ATCmd("MCUOTADOWNLOAD");
			}
			break;

		case IOT_EVT_MCUOTA_DOWNLOAD_DONE:
			/* MCU OTA 固件下载完成 */
			Mcu_Ota_Set_State(MCU_OTA_STATE_DOWNLOAD_DONE);
			#if !MCU_OTA_UPDATE_MODE
				MCU_OTA_REBOOT();
				//写flash/eeprom 的ota标志
				//重启进boot进行ymodem协议传输固件
				//在boot启动后将 mcu_ota_data.e_mcu_ota_state = MCU_OTA_STATE_DOWNLOAD_DONE; ,再调用 Mcu_Ota_Ymodem_Handle();
			#endif
			break;

		case IOT_EVT_MCUOTA_TRANSPORT_REBOOT:
			/* 触发MCU OTA 异常断电重启重传机制 */
			Mcu_Ota_Resume_Transmit();
			break;

		default:
			break;
	}
}

/* ========================= 公共接口实现 ========================= */
/********************************************************************************
 * @brief   查询 WiFi 模组 连接路由状态
 * @details 拼装 AT+WJAPS\r\n 并发送
 *          WiFi 模组返回 STATION_UP 表示已连接，STATION_DOWN 表示未连接
 * @param   null
 * @return  void
 * @note    需要查询时调用；
 *******************************************************************************/
void IOT_GetRouter_Status(void)
{
	IOT_Lib_Send_ATCmd("WJAPS");
}

/********************************************************************************
 * @brief   返回 WiFi 模组 连接路由状态
 * @details 调用IOT_GetRouter_Status()	后查询该返回值，可以判断是否已连接路由以及是否有应答；
 * @param   void
 * @return  s_RouterStatus
 * @note    需要使用时调用
 *******************************************************************************/
RouterStatus_t IOT_ReturnRouter_Status(void)
{
	return s_RouterStatus;
}

/********************************************************************************
 * @brief   获取网络时间
 * @details 拼装 AT+GETTIME\r\n 发送，
 *          WiFi 模组返回当前时间
 * @param   none
 * @return  void
 * @note    使用时调用
 *******************************************************************************/
void IOT_GetNetTime(void)
{
	IOT_Lib_Send_ATCmd("GETTIME");
	memset(&s_NetTime, 0, sizeof(s_NetTime));
}
/********************************************************************************
 * @brief   返回网络时间
 * @details 获取后先通过 TimeParseResult_t 解析结果，如果成功，再使用获取到的时间；
 * @param   void
 * @return  s_NetTime
 * @note    需要使用时调用
 *******************************************************************************/
NetTime_t IOT_Return_NetTime(void)
{
	return s_NetTime;
}

/********************************************************************************
 * @brief   进入产测
 * @details 拼装 AT+FACTORYTEST\r\n 发送，
 *          WiFi 模组返回 
 * 			+FACEVENT:TESTING\r\n
 *			+FACEVENT:SUCCESS\r\n
 *			+FACEVENT:FAIL\r\n
 * @param   none
 * @return  void
 * @note    使用时调用
 *******************************************************************************/
void IOT_FactorTest(void)
{
	IOT_Lib_Send_ATCmd("FACTORYTEST");
	s_FactoryResult = FACTORY_NULL;
}

/********************************************************************************
 * @brief   产测结果获取
 * @details 获取后先通过 TimeParseResult_t 解析结果，如果成功，再使用获取到的时间；
 * @param   void
 * @return  s_NetTime
 * @note    需要使用时调用
 *******************************************************************************/
FactoryResult_e IOT_Return_FactoryResult(void)
{
	return s_FactoryResult;
}

/********************************************************************************
 * @brief   配网状态获取
 * @details 返回配网过程中的状态，在应用层做交互处理；
 * @param   void
 * @return  s_NetWorkConfigStatus
 * @note    需要使用时调用
 *******************************************************************************/
NetWork_Config_Status_e IOT_Return_Net_Config_Status(void)
{
	return s_NetWorkConfigStatus;
}

/********************************************************************************
 * @brief   网络状态获取
 * @details 模组主动下发的网络状态；
 * @param   void
 * @return  s_NetTime
 * @note    需要使用时调用
 *******************************************************************************/
NetWorkStatus_e IOT_Return_NetStatus(void)
{
	return s_NetStatus;
}
/********************************************************************************
 * @brief   应用层初始化
 * @details 完成以下初始化序列：
 *          1. 记录上电时间戳
 * 			2. 初始化串口
 *          3. 调用 IOT_Lib_Init 注册事件回调并初始化串口
 * @param   void
 * @return  void
 * @note    上电后调用一次，须在进入主循环之前完成
 *******************************************************************************/
void IOT_UserApp_Init(void)
{
	s_bootCounter = 0;

	//获取需要APP显示的版本号
	Return_APPShowVer(app_mcu_Ver,sizeof(app_mcu_Ver),APP_DISVER_MODE);

	/* 初始化 底层UART 根据实际情况增删改*/
	IOT_UART_Init();

	/* 初始化 AT 解析层，注册事件回调 */
	IOT_Lib_Init(IOT_Event_Handle);

	/* 初始化 OTA相关变量 */
	 Mcu_Ota_Init();
}

/********************************************************************************
 * @brief   应用层主循环轮询
 * @details 须以固定周期 IOT_POLL_INTERVAL_MS（10ms）调用，内部按调用次数计数：
 *          1. 串口驱动发送间隔，保证数据不粘连
 *          2. 上电延迟期间仅轮询接收，不发送任何指令
 *          3. 上电延迟结束后发送上电必要指令（ModelId、滤芯注册）
 *          4. 轮询 AT 解析层处理接收和事件分发
 *          5. 递增差异上报计数器和全属性上报计数器，到期则触发上报
 * @param   void
 * @return  void
 * @note    调用周期必须与 IOT_POLL_INTERVAL_MS 保持一致，否则时间换算不准确
 *******************************************************************************/
void IOT_UserApp_Handle(void)
{
	#if MCU_OTA_UPDATE_MODE
	if(Mcu_Ota_Get_State().e_mcu_ota_state != MCU_OTA_STATE_IDLE && 
		Mcu_Ota_Get_State().e_mcu_ota_state != MCU_OTA_STATE_WAIT_DOWNLOAD )
	{/* Ymodem协议数据处理和解析 */
		Mcu_Ota_Ymodem_Handle();
	}
	else
	{/* 轮询 AT 解析层 */
		IOT_RX_Handle();
	}
	#else
		/* 轮询 AT 解析层 */
		IOT_RX_Handle();
	#endif
	

	/* 上电延迟，等待WiFi模组初始化完成，计数器未到阈值时只接收，不发送 */
	if (s_bootCounter < IOT_BOOT_DELAY_MS) 
	{
		s_bootCounter++;
		return;
	}

	/* 上电延迟结束后发送一次上电必要指令 */
	static bool s_bootCmdSent = false;
	if (!s_bootCmdSent) 
	{
		s_bootCmdSent = true;
		IOT_Lib_ReportModelId(g_device_Series, g_device_ModelID);
	#if HAVE_FILTER
		FilterRegisrter();
		FilterResetRegisrter();
	#endif
	}

	/* 串口发送间隔*/
	IOT_TX_Handle();

	/* MCU OTA 升级超时检测 */
	if(Mcu_Ota_Get_State().e_mcu_ota_state != MCU_OTA_STATE_IDLE)
	{	
		if(s_McuOtaCounter < IOT_MCU_OTA_TIME_MS)
			s_McuOtaCounter ++;
		else
		{
			Mcu_Ota_Init();		//退出MCU  OTA ，回到正常工作流程
			s_McuOtaCounter = 0;
		}
		return;	//升级中，不处理其他任务
	}
	else
	{ 
		s_McuOtaCounter = 0;
	}

	/* WiFi模块存在状态检测 */
	if (s_CheckExistCounter < IOT_CHECK_EXIST_INTERVAL_MS)
	{
		if(s_CheckExistCounter == 0)
		{
			IOT_GetRouter_Status();
		}
		s_CheckExistCounter ++;
		
	}
	else
	{ 
		s_CheckExistCounter = 0;
		s_CheckExistNumber ++;
		if(s_CheckExistNumber > IOT_CHECK_EXIST_NUMBER)
		{
			s_CheckExistNumber = 0;
			s_RouterStatus = ROUTER_STATUS_NULL; //连续发送3次 WiFi模块没回复
		}	
	}
	/* 未连云直接返回，不上报状态 */
	if (s_NetStatus != NET_STATUS_ONLINE)
	{
		s_DiffReportCounter = 0;
		s_fullReportCounter = 0;
		return;
	}
		
	/* 差异上报检测 */
	if(s_DiffReportCounter < IOT_REPORT_INTERVAL_MS)
		s_DiffReportCounter ++;
	else
	{
		IOT_UserApp_ReportDiff();
		s_DiffReportCounter = 0;
	}
	/* 全属性定时上报*/
	if(s_fullReportCounter == 0)	//确保首次联网后也能上报全属性
	{
		s_DiffReportCounter = 0;
		s_AllReportFlag = 1;
		IOT_UserApp_ReportAll();
	}
	if(s_fullReportCounter < IOT_FULL_REPORT_INTERVAL_MS && !s_AllReportFlag)
	{
		s_fullReportCounter ++;
	}
	else
	{
		s_fullReportCounter = 0;
	}
		
	
}

/**** (c) *************************** END OF FILE **/
