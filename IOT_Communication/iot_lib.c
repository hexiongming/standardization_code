/*******************************************************************************
 * @file    iot_lib.c
 * @author  hexm
 * @version V1.0.0
 * @date    2026-04-20
 * @brief   AT 指令解析层（中间层）
 * @attention 健康生活事业部 - 软件及 IOT 组
 *
 * 解析规则（协议 V2.0）：
 *   上行（MCU->WiFi）：AT+<CMD>=<params>\r
 *   下行（WiFi->MCU）：+MQTTRECV:0,<len>,<json>\r\n
 *                      +WEVENT:<xxx>\r\n
 *                      +MQTTEVENT:<xxx>\r\n
 *                      OK\r\n / ERROR\r\n
******************************************************************************/
#include "iot_lib.h"

/* ========================= 静态变量 ========================= */
static IOT_EventCallback_t s_eventCb = NULL;

/*
 * AT 帧描述符表（下行帧注册表）
 *
 * 新增一条下行帧只需在此处追加一行，格式：
 *   { "+WEVENT:NEWEVT",  IOT_MATCH_PREFIX, IOT_EVT_XXX} 
 *   { "+NEWCMD:DATA,",   IOT_MATCH_PREFIX, IOT_EVT_YYY} 
 *
 * 注意：前缀更长的条目须排在更短的同前缀条目之前，
 *       如 "+OTAEVENT:DOWNLOADING" 须在 "+OTAEVENT:" 之前。
 */
static const IOT_FrameEntry_t s_frameTable[] = {
	/* --- AT 通用响应 --- */
	{ "OK",                             IOT_MATCH_EXACT,  IOT_EVT_AT_OK,                 		},
	{ "ERROR",                          IOT_MATCH_EXACT,  IOT_EVT_AT_ERROR,              		},
	{ "+GETTIME:",                      IOT_MATCH_PREFIX, IOT_EVT_GET_TIME,              		},
	{ "+FACEVENT:",                     IOT_MATCH_PREFIX, IOT_EVT_FACTORYTEST,           		},

	/* --- 控制命令下发 */
	{ "+MQTTRECV:0,",                   IOT_MATCH_PREFIX, IOT_EVT_MQTT_RECV,             		},

	/* --- WiFi 事件 */
	{ "+MQTTEVENT:CONNECT,SUCCESS",     IOT_MATCH_EXACT, IOT_EVT_MQTT_CONNECT_OK,       		},
	{ "+WEVENT:AP_UP",                  IOT_MATCH_EXACT, IOT_EVT_WIFI_AP_UP,            		},
	{ "+WEVENT:CONNECTING",             IOT_MATCH_EXACT, IOT_EVT_WIFI_CONNECTING,       		},
	{ "+WEVENT:CONNECTTED",             IOT_MATCH_EXACT, IOT_EVT_WIFI_CONNECTED,        		},
	{ "+WEVENT:TIMEOUT",                IOT_MATCH_EXACT, IOT_EVT_WIFI_TIMEOUT,          		},
	{ "+WEVENT:UNBIND,SUCCESS",         IOT_MATCH_EXACT, IOT_EVT_WIFI_UNBIND_SUCCESS,   		},


	/* ---WIFI OTA 事件（具体事件须排在通配前缀 +OTAEVENT: 之前） --- */
	{ "+OTAEVENT:DOWNLOADING",          IOT_MATCH_EXACT, IOT_EVT_OTA_PROGRESS,       			},
	{ "+OTAEVENT:INSTALLING",           IOT_MATCH_EXACT, IOT_EVT_OTA_PROGRESS,       			},
	{ "+OTAEVENT:SUCCESS",              IOT_MATCH_EXACT, IOT_EVT_OTA_SUCCESS,           		},

	/* --- MCU OTA 事件（带额外解析的条目须排在同前缀纯映射之前） --- */
	{ "+MCUOTAEVENT:VERSION,",          IOT_MATCH_PREFIX, IOT_EVT_MCUOTA_VERSION,        		},
	{ "+MCUOTAEVENT:DOWNLOAD,DONE",     IOT_MATCH_EXACT,  IOT_EVT_MCUOTA_DOWNLOAD_DONE,  		},
	{ "+MCUOTAEVENT:TRANSPORT,REBOOT",  IOT_MATCH_EXACT, IOT_EVT_MCUOTA_TRANSPORT_REBOOT,		},

	/* ---- 新增下行帧在此行之前追加 ---- */
};

#define FRAME_TABLE_SIZE  (sizeof(s_frameTable) / sizeof(s_frameTable[0]))


/********************************************************************************
 * @brief   WiFi模组下发的命令帧解析函数
 * @details 遍历 s_frameTable[]，对每条描述符按 matchType 做精确或前缀匹配，
 *          命中后设置事件类型，若描述符挂有 parser 则调用额外解析；
 *          parser 返回 false 时丢弃该帧（type 保持 IOT_EVT_UNKNOWN）。
 *          新增下行帧只需在 s_frameTable[] 末尾追加一行，本函数无需修改。
 * @param   line: 以 '\0' 结尾的原始行字符串
 * @param   evt:  输出事件结构体指针，未命中时 e_type=IOT_EVT_UNKNOWN
 * @return  void
 * @note    空行时设置 e_type=IOT_EVT_NONE，不触发回调；
 *          同前缀条目的顺序决定优先级，前缀更长的须排在更前面
 *******************************************************************************/
static void LineCmd_Parse(const char *line, IOT_Event_t *evt)
{
	memset(evt, 0, sizeof(IOT_Event_t));
	evt->e_type = IOT_EVT_UNKNOWN;

	if (line[0] == '\0')
	{
		evt->e_type = IOT_EVT_NONE;
		return;
	}

	for (uint16_t i = 0; i < FRAME_TABLE_SIZE; i++)
	{
		const IOT_FrameEntry_t *e = &s_frameTable[i];

		if (strncmp(line, e->prefix, strlen(e->prefix)) != 0) 
			continue;

		/* 命中：设置默认事件类型，若非全字匹配，则将剩余部分传入evt->json */
		evt->e_type = e->evtType;

		/* 若非全字匹配，则将剩余部分传入evt->json，并补结束符'\0' */
		if(e->matchType == IOT_MATCH_EXACT)
			return;
		else
		{
			const char *p = line + strlen(e->prefix);
			strncpy(evt->para_buf, p, sizeof(evt->para_buf) - 1);
			evt->para_buf[sizeof(evt->para_buf) - 1] = '\0';
		}
		return;
	}
}

/* ========================= 公共接口实现 ========================= */

/********************************************************************************
 * @brief   AT 解析层初始化
 * @details 保存上层事件回调函数指针，并调用串口底层初始化
 * @param   cb: 上层事件处理回调，不可为 NULL
 * @return  void
 * @note    在 IOT_UserApp_Init 中调用一次
 *******************************************************************************/
void IOT_Lib_Init(IOT_EventCallback_t cb)
{
	s_eventCb = cb;
}

/********************************************************************************
 * @brief   AT 解析层主循环轮询
 * @details 从串口底层读取一行数据，调用 ParseLine 解析后触发事件回调。
 *          每次调用最多处理一行，建议在主循环中以 ≤10ms 周期调用
 * @param   void
 * @return  void
 * @note    无完整行时立即返回，不阻塞
 *******************************************************************************/
void IOT_RX_Handle(void)
{
	uint8_t  s_lineBuf[IOT_RX_CMD_MAX_SIZE];			/* 接收单条指令缓冲区     */
	/* 从接收环形队列中读取一帧AT指令数据*/
	uint16_t len = IOT_UART_ReadLine(s_lineBuf, sizeof(s_lineBuf));
	if (len == 0) 
		return;

	/* 匹配注册的AT 帧描述符表中的命令类型，并解析命令的有效字段 */
	IOT_Event_t evt;
	LineCmd_Parse((const char *)s_lineBuf, &evt);

	/* 通过 IOT_Event_Handle 回调函数，处理对应指令类型的执行动作 */
	if (evt.e_type != IOT_EVT_NONE && s_eventCb != NULL)
	{
		s_eventCb(&evt);
	}
}

/********************************************************************************
 * @brief   将 AT 指令加入发送队列
 * @details 拼装为 AT+<cmd>\r\n 格式后压入发送队列，
 *          由 IOT_TX_Handle 按发送间隔逐条取出发送，不会因间隔丢失数据。
 * @param   cmd: 不含 "AT+" 前缀和结束符的指令字符串
 * @return  void
 *******************************************************************************/
void IOT_Lib_Send_ATCmd(const char *cmd)
{
	static char txBuf[IOT_UART_TX_BUF_SIZE];
	memset(txBuf, 0, sizeof(txBuf));
	snprintf(txBuf, sizeof(txBuf), "AT+%s\r\n", cmd);
	IOT_UART_TxEnQueue(txBuf);
}

/********************************************************************************
 * @brief   上报设备型号给 WiFi 模组
 * @details 拼装 AT+MODELID=<series>,<modelid>\r\n 并发送，
 *          WiFi 模组返回 OK 表示保存成功，须在每次上电时调用
 * @param   series:  设备系列名，如 "MargaritaIOT"
 * @param   modelId: 设备型号，如 "ADD8666"
 * @return  void
 * @note    须在上电 3 秒后、连网操作之前调用；
 *******************************************************************************/
void IOT_Lib_ReportModelId(const char *series, const char *modelId)
{
	char txBuf[IOT_UART_TX_BUF_SIZE];
	snprintf(txBuf, sizeof(txBuf), "MODELID=%s,%s", series, modelId);
	IOT_Lib_Send_ATCmd(txBuf);
}

/********************************************************************************
 * @brief   触发模组进入对应状态
 * @details 发送 AT+SOFTSTART=<mode>\r\n 指令，触发 WiFi 模组进入XX状态，
 * @param   mode: 状态模式
 * 			@arg 1: 恢复出厂
 *          @arg 2: 热点配网（二期/四期）
 * 			@arg 3: 解绑
 * 			@arg 4: 旧蓝牙配网
 *          @arg 5: 新蓝牙配网（新品）
 * @return  void
 * @note    OK 响应代表模组收到指令
 *******************************************************************************/
static void IOT_Lib_SoftStartConfig(uint8_t mode)
{
	char txBuf[16];
	snprintf(txBuf, sizeof(txBuf), "SOFTSTART=%u", mode);
	IOT_Lib_Send_ATCmd(txBuf);
}
/********************************************************************************
 * @brief   WiFi 模组进入新蓝牙配网
 * @details 发送 AT+SOFTSTART=5\r，触发 WiFi 模组进入蓝牙配网状态
 * @param   void
 * @return  void
 * @note    OK 响应仅代表模组收到指令，以 +WEVENT:AP_UP 为真正进入配网的标志；
 *******************************************************************************/
void IOT_Lib_NewBleSet(void)
{
	IOT_Lib_SoftStartConfig(IOT_SOFTSTART_5);
}
/********************************************************************************
 * @brief   WiFi 模组进入AP热点配网
 * @details 发送 AT+SOFTSTART=2\r，触发 WiFi 模组进入热点配网状态
 * @param   void
 * @return  void
 * @note    OK 响应仅代表模组收到指令，以 +WEVENT:AP_UP 为真正进入配网的标志；
 *******************************************************************************/
void IOT_Lib_WifiAPSet(void)
{
	IOT_Lib_SoftStartConfig(IOT_SOFTSTART_2);
}
/********************************************************************************
 * @brief   WiFi 模组恢复出厂设置
 * @details 发送 AT+SOFTSTART=1\r，清除模组内所有配置参数
 * @param   void
 * @return  void
 * @note    执行后模组内 WiFi 信息、绑定信息等全部清除，不可恢复
 *******************************************************************************/
void IOT_Lib_FactoryReset(void)
{
	IOT_Lib_SoftStartConfig(IOT_SOFTSTART_1);
}

/********************************************************************************
 * @brief   触发设备解绑
 * @details 发送 AT+SOFTSTART=3\r，以 +WEVENT:UNBIND,SUCCESS 为解绑成功标志，
 *          解绑成功后模组将自动进入配网状态
 * @param   void
 * @return  void
 * @note    OK 响应仅代表模组收到指令，真正解绑以 UNBIND,SUCCESS 事件为准
 *******************************************************************************/
void IOT_Lib_Unbind(void)
{
	IOT_Lib_SoftStartConfig(IOT_SOFTSTART_3);
}


/**** (c) *************************** END OF FILE **/
