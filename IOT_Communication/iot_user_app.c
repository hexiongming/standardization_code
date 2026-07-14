/*******************************************************************************
 * @file    iot_user_app.c
 * @author  hexm
 * @version V1.0.0
 * @date    2026-04-20
 * @brief   IoT 业务应用层（应用层）
 * @attention 健康生活事业部 - 软件及 IOT 组
 *
 * 上报规则（协议 V2.0）：
 *   1. 属性字段 1 分钟内上报不超过 1 次
 *   2. 属性与上次不同时立即上报
 *   3. 全属性每 1 小时上报一次（连云成功后立即上报一次）
 *   4. OTA 下载期间停止上报
 *   5. 上电 3 秒后才开始上报（模组指令注册需要 3 秒）
 *
 * 新增上报字段说明：
 *   1. 在业务模块（如 App_Key.c）中定义 int32_t 变量
 *   2. 在本文件 s_attrTable[] 数组末尾追加一行：
 *      { "JsonKey", &your_var, IOT_ATTR_INT }
 *   3. 无需修改任何函数
******************************************************************************/
#include "iot_user_app.h"



/* ========================= 私有变量 ========================= */
static uint32_t s_DiffReportCounter      = 0;	/* 差异属性上报间隔计数器*/
static uint32_t s_fullReportCounter  = 0;		/* 全属性上报间隔计数器*/
static uint16_t s_bootCounter        = 0;		/* 上电延迟计数器*/
static uint16_t s_txIntervalCounter  = 0;		/* 发送间隔计数器*/
static uint16_t s_McuOtaCounter  = 0;			/* mcu ota时间计数器*/
static uint16_t s_AllReportFlag     = 0;		/* 全属性上报标志*/
static NetTime_t s_NetTime;						/* 网络时间*/
static FactoryResult_t s_FactoryResult;			/* 产测结果*/
static NetWorkStatus_t s_NetStatus = NET_STATUS_OFFLINE;	/* 网络状态*/

/* -----------------------------------------------------------------------
 * 外部变量声明或定义，若已在外部文件定义和声明，则包含头文件
 * ----------------------------------------------------------------------- */
int32_t g_iot_Status;            /* 设备状态							*/
int32_t g_iot_Flush;             /* 一键冲洗							*/
int32_t g_iot_InputTemp;         /* 进水温度							*/
int32_t g_iot_OutputTemp;        /* 出水温度                          */
int32_t g_iot_SetTemp;           /* 设定温度                          */
int32_t g_iot_SetOutlet;         /* 设定水量(mL)                      */
int32_t g_iot_TotalOutlet;       /* 总纯水量(mL)                      */
int32_t g_iot_PureTDS;           /* 纯水 TDS                          */
int32_t g_iot_RawTDS;            /* 原水 TDS                          */
int32_t g_iot_MAXFilterLife;     /* MAX 滤芯剩余百分比                */
int32_t g_iot_PC5in1FilterLife;  /* 5in1 滤芯剩余百分比               */
int32_t g_iot_SmartMode;         /* 智能记忆模式                      */
int32_t g_iot_WinterMode;        /* 冬季模式                          */
int32_t g_iot_NightModeSet;      /* 夜间模式                          */
int32_t g_iot_SterilizerState;   /* 杀菌状态                          */
int32_t g_iot_FamilyMode;        /* 家庭模式                          */

char g_mcu_Ver[16] = MCU_VERSION;
char g_mcu_Manufacturer[32] = MCU_MANUFACTURER; 
char	*g_iot_ErrorCode = "E00";		/* 错误码 */
int32_t g_iot_WaringCode;				/* 告警码 */
int32_t g_maxFilterResetVal;			/* MAX滤芯复位状态 */
int32_t g_pc5in1FilterResetVal;			/* PC5in1滤芯复位状态 */
int32_t g_RefrigerationMode;			/* 制冷状态 */

/* -----------------------------------------------------------------------
 * 云端可写字段的 CallBack 回调
 * 仅需要额外动作的字段才定义回调，普通赋值字段传 NULL 即可
 * ----------------------------------------------------------------------- */

/********************************************************************************
 * @brief   滤芯复位写入回调
 * @details val==3 时执行 MAX 滤芯复位动作
 * @param   val: 云端下发值
 *******************************************************************************/
static void CallBack_MAXFilterReset(void *pData)
{
	int32_t val = *(int32_t *)pData; // 强制转换回 int32_t*
	if (val == 3)
	{
		/* TODO: 执行 MAX 滤芯复位 */
	}
}

/********************************************************************************
 * @brief   5in1 滤芯复位写入回调
 * @param   val: 云端下发值
 *******************************************************************************/
static void CallBack_PC5in1FilterReset(void *pData)
{
	int32_t val = *(int32_t *)pData; // 强制转换回 int32_t* 再取值
	if (val == 3)
	{
		/* TODO: 执行 5in1 滤芯复位 */
	}
	else if (val == 4)
	{
		/* TODO: 执行 5in1 滤芯撤销复位 */
	}
}
static void CallBack_RefrigerationMode(void *pData)
{
	int32_t val = *(int32_t *)pData; // 强制转换回 int32_t* 再取值
	if (val == 1)
	{
		char txBuf[IOT_UART_TX_BUF_SIZE];
		snprintf(txBuf, sizeof(txBuf), "OooooooooK=%d", 0x01);
		IOT_Lib_Send_ATCmd(txBuf);
	}
	else if (val == 0)
	{
		char txBuf[IOT_UART_TX_BUF_SIZE];
		snprintf(txBuf, sizeof(txBuf), "OK=%d", 0x00);
		IOT_Lib_Send_ATCmd(txBuf);
	}
}

/* -----------------------------------------------------------------------
 * 属性描述符表（上报属性注册表）
 * 根据实际修改此注册表
 * 新增上报字段只需在此处追加一行，格式：
 *   { "JsonKey", &g_iot_xxx, 0, IOT_ATTR_INT, false, true, NULL }  — 只读只上报
 *   { "JsonKey", &g_iot_xxx, 0, IOT_ATTR_INT, true,  true,  NULL }  — 可读写
 *   { "JsonKey", &g_iot_xxx, 0, IOT_ATTR_INT, true,  true, CallBack_xxx } — 可读写+回调
 * 只支持常量的字符串类型，如果需上报可变字符串，请单独实现上报函数，如 IOT_UserApp_ReportErrorCode();
 * ----------------------------------------------------------------------- */
static IOT_AttrEntry_t s_attrTable[]=
{
	//jsonkey,               value,                         lastVal,       type,           write,    allReport,   CallBack
	{"Ver",                  (int32_t*)&g_mcu_Ver,            0,          IOT_ATTR_STR,    false,      true,      NULL},
	{"McuManufacturer",      (int32_t*)&g_mcu_Manufacturer,   0,          IOT_ATTR_STR,    false,      true,      NULL},
	{"Status",               &g_iot_Status,                   0,          IOT_ATTR_INT,    false,      true,      NULL},
	{"InputTemp",            &g_iot_InputTemp,                0,          IOT_ATTR_INT,    false,      true,      NULL},
	{"OutputTemp",           &g_iot_OutputTemp,               0,          IOT_ATTR_INT,    false,      true,      NULL},
	{"SetTemp",              &g_iot_SetTemp,                  0,          IOT_ATTR_INT,    true,       true,      NULL},
	{"SetOutlet",            &g_iot_SetOutlet,                0,          IOT_ATTR_INT,    true,       true,      NULL},
	{"TotalOutlet",          &g_iot_TotalOutlet,              0,          IOT_ATTR_INT,    false,      true,      NULL},
	{"PureTDS",              &g_iot_PureTDS,                  0,          IOT_ATTR_INT,    false,      true,      NULL},
	{"RawTDS",               &g_iot_RawTDS,                   0,          IOT_ATTR_INT,    false,      true,      NULL},
	{"WarmWaterSet",         &g_iot_SmartMode,                0,          IOT_ATTR_INT,    true,       true,      NULL},
	{"HotWaterSet",          &g_iot_WinterMode,               0,          IOT_ATTR_INT,    true,       true,      NULL},
	{"Outlet120Set",         &g_iot_NightModeSet,             0,          IOT_ATTR_INT,    true,       true,      NULL},
	{"Outlet350Set",         &g_iot_SterilizerState,          0,          IOT_ATTR_INT,    true,       true,      NULL},
	{"Outlet800Set",         &g_iot_FamilyMode,               0,          IOT_ATTR_INT,    true,       true,      NULL},
	{"SterilizerState",      &g_iot_SterilizerState,          0,          IOT_ATTR_INT,    true,       true,      NULL},
	{"RefrigerationMode",    &g_RefrigerationMode,            0,          IOT_ATTR_INT,    true,       true,      CallBack_RefrigerationMode},
	{"MAXFilterReset",       &g_maxFilterResetVal,            0,          IOT_ATTR_INT,    true,       false,     CallBack_MAXFilterReset},
	{"PC5in1FilterReset",    &g_pc5in1FilterResetVal,         0,          IOT_ATTR_INT,    true,       false,     CallBack_PC5in1FilterReset},
	/*END----新增字段在此行之前追加----*/
};

#define ATTR_TABLE_SIZE  (sizeof(s_attrTable) / sizeof(s_attrTable[0]))

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
	char txBuf[IOT_PKG_MAX_SIZE];

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

	/* 将 MQTTSEND=%u, 头部拼接到 {json} 之前 */
	 int ret = snprintf(txBuf, sizeof(txBuf), "MQTTSEND=%u,%s", jsonLen, JsonBuff->buf);
    
    // 检查是否截断
    if (ret < 0 || ret >= sizeof(txBuf)) {
        // 处理错误：数据包太大，可能需要分包或丢弃
        JsonBuff_Begin(JsonBuff);
        return;
    }
	IOT_Lib_Send_ATCmd(txBuf);

	/* 重置，开启新包 */
	JsonBuff_Begin(JsonBuff);
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
	char tmp[64];

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
	char tmp[64];

	int n = snprintf(tmp, sizeof(tmp), "%s\"%s\":\"%s\"",
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
 * @param   json: 原始 JSON 字符串
 * @param   key:  目标字段名
 * @param   out:  输出整型值的存储地址
 * @return  bool: true=找到并解析成功  false=字段不存在或格式错误
 * @note    仅做简单线性搜索，不支持嵌套 JSON
 *******************************************************************************/
static bool Json_Get_Int_Value(const char *json, const char *key, int32_t *out)
{
	if (json == NULL || key == NULL || out == NULL)
        return false;
	/* 搜索 "key": */
	char search[64];

	snprintf(search, sizeof(search), "\"%s\":", key);
	const char *p = strstr(json, search);
	if (p == NULL) 
		return false;

	p += strlen(search);
	while (*p == ' ') p++;  /* 跳过空格 */

	char *endPtr;
    long val = strtol(p, &endPtr, 10);

    /* 检查是否解析到了任何数字 */
    if (endPtr == p) 
        return false;

    *out = (int32_t)val;
    return true;
}
/********************************************************************************
 * @brief   从 JSON 字符串中提取字符串字段值
 * @details 搜索 "key":"  模式，提取两个双引号之间的内容，
 *          自动追加 '\0' 终止符
 * @param   json:   原始 JSON 字符串
 * @param   key:    目标字段名
 * @param   outBuf: 输出字符串缓冲区
 * @param   bufLen: 缓冲区容量（含 '\0'）
 * @return  bool: true=找到并提取成功  false=字段不存在或值为空
 * @note    仅做简单线性搜索，不支持转义字符
 *******************************************************************************/
static bool Json_Get_Str_Value(const char *json, const char *key, char *outBuf, uint16_t bufLen)
{
	char search[48];
	snprintf(search, sizeof(search), "\"%s\":\"", key);
	const char *p = strstr(json, search);
	if (p == NULL) 
		return false;

	p += strlen(search);
	uint16_t i = 0;
	while (*p != '"' && *p != '\0' && i < bufLen - 1)
	{
		outBuf[i++] = *p++;
	}
	outBuf[i] = '\0';
	return (i > 0);
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
	int32_t val;
	const char *jsonStart = json;
	char outBuf[64];

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

	for (uint16_t i = 0; i < ATTR_TABLE_SIZE; i++)
	{
		IOT_AttrEntry_t *e = &s_attrTable[i];
		if (!e->write_enable) 
			continue;

		if(e->type == IOT_ATTR_INT)
		{
			if (Json_Get_Int_Value(jsonStart, e->key, &val))
			{
				*e->pVal = val;
				if (e->CallBack != NULL)
				{
					e->CallBack(&val);
				}
			}
		}
		else if(e->type == IOT_ATTR_STR)
		{
			if (Json_Get_Str_Value(jsonStart, e->key, outBuf,sizeof(outBuf)))
			{
				if (e->CallBack != NULL)
				{
					e->CallBack(outBuf);
				}
			}
		}
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
		char txBuf[IOT_UART_TX_BUF_SIZE];
		snprintf(txBuf, sizeof(txBuf), "FLTPROCESS=%s,%d,%s,%d,%s",
				 cfg->jsonKey, cfg->x, cfg->fx, cfg->y, cfg->fy);
		IOT_Lib_Send_ATCmd(txBuf);
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
		IOT_AttrEntry_t *e = &s_attrTable[i];
		if(e->allReport_enable)
		{
			
			if (e->type == IOT_ATTR_STR)
			{/* 16(AT+MQTTSEND=xxx,) + pos + e->key + e->pVal + 7(":""}\0\r\n") 是否超过总帧上限，如果超过，则立即闭合当前帧入队，此次处理的数据放到下一包 */
				if ((16 + JsonBuff.pos + strlen(e->key) + strlen((char *)e->pVal) + 7) > IOT_PKG_MAX_SIZE)
				{
					break;
				}
				JsonBuff_AddStr(&JsonBuff, e->key, (const char *)e->pVal);
				e->lastVal = (int32_t)(uint32_t)e->pVal; 
			}
			else
			{/* 16(AT+MQTTSEND=xxx,) + pos + e->key + e->pVal + 5(":}\0\r\n") 是否超过总帧上限，如果超过，则立即闭合当前帧入队，此次处理的数据放到下一包 */
				if ((16 + JsonBuff.pos + strlen(e->key) + sizeof(e->pVal) + 5) > IOT_PKG_MAX_SIZE)
				{
					break;
				}
				JsonBuff_AddInt(&JsonBuff, e->key, *e->pVal);
				e->lastVal = *e->pVal;
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
		IOT_AttrEntry_t *e = &s_attrTable[i];
		if (e->type == IOT_ATTR_INT)
		{
			int32_t cur = *e->pVal;
			if (cur != e->lastVal)
			{/* 16(AT+MQTTSEND=xxx,) + pos + e->key + e->pVal + 5(":}\0\r\n") 是否超过总帧上限，如果超过，则立即闭合当前帧入队，此次处理的数据放到下一包 */
				if ((16 + JsonBuff.pos + strlen(e->key) + sizeof(e->pVal) + 5) > IOT_PKG_MAX_SIZE)
				{
					break;
				}
				JsonBuff_AddInt(&JsonBuff, e->key, cur);
				e->lastVal = cur;
			}
		}
		else
		{
			//常量字符串不支持比较上报
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
 * @brief   错误代码上报
 * @details 上电连云成功后上报一次，其余时间有故障触发时立即上报
 * @param   errorCode 要上报的错误代码
 * @return  void
 * @note    需要使用时调用
 *******************************************************************************/
void IOT_UserApp_ReportErrorCode(char *errorCode)
{
/* 单独上报 ErrorCode*/
	char txBuf[IOT_UART_TX_BUF_SIZE];
	snprintf(txBuf, sizeof(txBuf), "MQTTSEND=%u,{\"ErrorCode\":\"%s\"}",
				(unsigned)(snprintf(NULL, 0, "{\"ErrorCode\":\"%s\"}", errorCode)),
				errorCode);
	IOT_Lib_Send_ATCmd(txBuf);
}

/********************************************************************************
 * @brief   告警代码上报
 * @details 上电连云成功后上报一次，其余时间有告警触发时立即上报
 * @param   errorCode 要上报的告警代码
 * @return  void
 * @note    需要使用时调用
 *******************************************************************************/
void IOT_UserApp_ReportWaringCode(int32_t waringCode)
{
/* 单独上报 WaingCode*/
	char txBuf[IOT_UART_TX_BUF_SIZE];
	snprintf(txBuf, sizeof(txBuf), "MQTTSEND=%u,{\"Warning\":%ld}",
				(unsigned)(snprintf(NULL, 0, "{\"Warning\":%ld}", (long)waringCode)),
				(long)waringCode);
	IOT_Lib_Send_ATCmd(txBuf);
}

/********************************************************************************
 * @brief   MCU版本号上报
 * @details 上电连云成功后上报一次
 * @param   void
 * @return  void
 * @note    需要使用时调用
 *******************************************************************************/
static void IOT_UserApp_CheckMCUOTA(void)
{
/* 单独上报 mcu version*/
	char txBuf[IOT_UART_TX_BUF_SIZE];
	snprintf(txBuf, sizeof(txBuf), "MCUOTACHECK=%s",MCU_VERSION);
	IOT_Lib_Send_ATCmd(txBuf);
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
	char txBuf[IOT_UART_TX_BUF_SIZE];

	switch (evt->e_type)
	{
		case IOT_EVT_AT_OK:
			if(Mcu_Ota_Get_State() == MCU_OTA_STATE_NEED_RESUME
			|| Mcu_Ota_Get_State() == MCU_OTA_STATE_DOWNLOAD_DONE)
			{
				#if MCU_OTA_UPDATE_MODE
					Mcu_Ota_Start_Transmit();//启动ymodem传输
				#else
					//写flash/eeprom 的ota标志
					//重启进boot进行ymodem协议传输固件
				#endif
			}
				
			break;

		case IOT_EVT_MQTT_CONNECT_OK:
			if (s_NetStatus != NET_STATUS_ONLINE)
			{
				s_NetStatus =  NET_STATUS_ONLINE;

				/* 连云成功立即上报一次 ErrorCode + Warning + MCU版本号 */
				IOT_UserApp_ReportErrorCode(g_iot_ErrorCode);
				IOT_UserApp_ReportWaringCode(g_iot_WaringCode);
				/* 检查 MCU OTA 版本 */
				IOT_UserApp_CheckMCUOTA();
			}
		
			break;

		case IOT_EVT_WIFI_AP_UP:
		case IOT_EVT_WIFI_CONNECTING:
			s_NetStatus =  NET_STATUS_CONNECTING;
			break;

		case IOT_EVT_WIFI_CONNECTED:
			s_NetStatus =  NET_STATUS_CONNECTTED;
			break;

		case IOT_EVT_WIFI_TIMEOUT:
		case IOT_EVT_WIFI_UNBIND_SUCCESS:
			s_NetStatus =  NET_STATUS_OFFLINE;

			break;

		case IOT_EVT_MQTT_RECV:
			/* TODO: 属性下发解析 */
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
			if (strcmp(evt->para_buf, MCU_VERSION) != 0)
			{
				Mcu_Ota_Set_State(MCU_OTA_STATE_WAIT_DOWNLOAD);
				IOT_Lib_Send_ATCmd("MCUOTADOWNLOAD");
			}
			break;

		case IOT_EVT_MCUOTA_DOWNLOAD_DONE:
			/* MCU OTA 固件下载完成 */
			snprintf(txBuf, sizeof(txBuf), "MCUOTASEND=%d",OTA_YMODEM_VALID_SIZE);
			IOT_Lib_Send_ATCmd(txBuf);
			Mcu_Ota_Set_State(MCU_OTA_STATE_DOWNLOAD_DONE);
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
 * @brief   获取网络时间
 * @details 拼装 AT+GETTIME\r\n 发送，
 *          WiFi 模组返回当前时间
 * @param   none
 * @return  void
 * @note    使用时调用
 *******************************************************************************/
void IOT_GetNetTime(void)
{
	char txBuf[IOT_UART_TX_BUF_SIZE];
	snprintf(txBuf, sizeof(txBuf), "GETTIME");
	IOT_Lib_Send_ATCmd(txBuf);
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
	char txBuf[IOT_UART_TX_BUF_SIZE];
	snprintf(txBuf, sizeof(txBuf), "FACTORYTEST");
	IOT_Lib_Send_ATCmd(txBuf);
	s_FactoryResult = FACTORY_NULL;
}

/********************************************************************************
 * @brief   产测结果获取
 * @details 获取后先通过 TimeParseResult_t 解析结果，如果成功，再使用获取到的时间；
 * @param   void
 * @return  s_NetTime
 * @note    需要使用时调用
 *******************************************************************************/
FactoryResult_t IOT_Return_FactoryResult(void)
{
	return s_FactoryResult;
}

/********************************************************************************
 * @brief   网络状态获取
 * @details 模组主动下发的网络状态；
 * @param   void
 * @return  s_NetTime
 * @note    需要使用时调用
 *******************************************************************************/
NetWorkStatus_t IOT_Return_NetStatus(void)
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
	for(uint16_t i = 0; i < ATTR_TABLE_SIZE; i++)
	{
		IOT_AttrEntry_t *e = &s_attrTable[i];
		if (e->type == IOT_ATTR_INT)
		{
			e->lastVal = ~*e->pVal;			//确保非全属性上报的值为0时可以执行差异属性上报
		}
	}


	s_bootCounter = 0;

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
	if(Mcu_Ota_Get_State() == MCU_OTA_STATE_YMODEM_TRANSMIT)
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
		IOT_Lib_ReportModelId(DEVICE_SERIES, DEVICE_MODEL_ID);
	#if HAVE_FILTER
		FilterRegisrter();
		FilterResetRegisrter();
	#endif
	}

	/* 串口发送间隔*/
	if(IOT_TXBusy_Return()) //还在发送时清 s_txIntervalCounter
	{
		s_txIntervalCounter = 0;
	}
	else
	{
		if(s_txIntervalCounter < IOT_TX_INTERVAL_MS)
			s_txIntervalCounter ++;
		else
		{
			IOT_TX_Handle();
			s_txIntervalCounter = 0;
		}
	}
	
	/* MCU OTA 升级超时检测 */
	if(Mcu_Ota_Get_State() != MCU_OTA_STATE_IDLE)
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
