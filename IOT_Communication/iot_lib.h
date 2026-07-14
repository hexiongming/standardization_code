/*******************************************************************************
 * @file    iot_lib.h
 * @author  hexm
 * @version V1.0
 * @date    2026-04-20
 * @brief   AT 指令解析层头文件（中间层）
 * @attention 健康生活事业部 - 软件及 IOT 组
 *
 * 职责：识别并分类从 WiFi 模组收到的 AT 响应 / 事件帧，
 *       向上层提供结构化的事件通知，向下层调用串口发送接口。
******************************************************************************/
/**
 * 本文件只提供以下接口供外部用户调用：
 * IOT_Lib_NewBleSet();				启动配网新蓝牙配网
 * IOT_Lib_WifiAPSet()；			启动配网热点配网
 * IOT_Lib_FactoryReset();			恢复出厂设置
 * IOT_Lib_Unbind();				设备解绑
*/


#ifndef IOT_LIB_H
#define IOT_LIB_H

#include "iot_uart.h"

#ifdef __cplusplus
extern "C" {
#endif


/* ========================= 事件类型枚举 ========================= */
typedef enum
{
    /* universal successful return result.*/
	RESULT_SUCCESS            = 0x00,
    /* universal error return result.*/
	RESULT_ERROR              = 0x01,
} Result_e;

typedef enum
{
    IOT_SOFTSTART_NONE = 0,
    IOT_SOFTSTART_1 = 1,            //恢复出厂设置
    IOT_SOFTSTART_2 = 2,            //热点配网
    IOT_SOFTSTART_3 = 3,            //设备解绑
    IOT_SOFTSTART_4 = 4,            //旧蓝牙配网
    IOT_SOFTSTART_5 = 5,            //新蓝牙配网
} IOT_SoftStart_e;


typedef enum
{
    IOT_EVT_NONE = 0,

	/* AT 通用响应 */
    IOT_EVT_AT_OK,                  /* OK */
    IOT_EVT_AT_ERROR,               /* ERROR */
	IOT_EVT_GET_TIME,				/* TIME */
	IOT_EVT_FACTORYTEST,			/* FACTORYTEST */

    /* WiFi 网络事件 */
	IOT_EVT_MQTT_CONNECT_OK,        /* +MQTTEVENT:CONNECT,SUCCESS 云端连接成功 */
    IOT_EVT_WIFI_AP_UP,             /* +WEVENT:AP_UP            进入配网 */
    IOT_EVT_WIFI_CONNECTING,        /* +WEVENT:CONNECTING       连接路由器中 */
    IOT_EVT_WIFI_CONNECTED,         /* +WEVENT:CONNECTTED       已连接路由器 */
    IOT_EVT_WIFI_TIMEOUT,           /* +WEVENT:TIMEOUT          配网超时 */
    IOT_EVT_WIFI_UNBIND_SUCCESS,    /* +WEVENT:UNBIND,SUCCESS   解绑成功 */

    /* 云端下发指令 */
    IOT_EVT_MQTT_RECV,              /* +MQTTRECV:0,<len>,<json>  云端下发 */

    /* WiFi MOUDLE OTA 事件 */
    IOT_EVT_OTA_PROGRESS,           /* +OTAEVENT:				OTA 进行中 */
    IOT_EVT_OTA_SUCCESS,            /* +OTAEVENT:SUCCESS        OTA 成功 */

    /* MCU OTA 事件 */
    IOT_EVT_MCUOTA_VERSION,         /* +MCUOTAEVENT:VERSION,xxx */
    IOT_EVT_MCUOTA_DOWNLOAD_DONE,   /* +MCUOTAEVENT:DOWNLOAD,DONE */
    IOT_EVT_MCUOTA_TRANSPORT_REBOOT,/* +MCUOTAEVENT:TRANSPORT,REBOOT */

    IOT_EVT_UNKNOWN,
} IOT_EventType_e;

/* ========================= AT 帧描述符表 ========================= */
/*
 * 匹配模式：
 *   IOT_MATCH_EXACT   — 全字符串精确匹配（用于 OK / ERROR 等短响应）
 *   IOT_MATCH_PREFIX  — 前缀匹配（用于所有 +WEVENT / +OTAEVENT 等事件帧）
 */
typedef enum
{
    IOT_MATCH_EXACT  = 0,
    IOT_MATCH_PREFIX,
} IOT_MatchType_t;


typedef struct
{
    const char        *prefix;
    IOT_MatchType_t    matchType;
    IOT_EventType_e    evtType;
} IOT_FrameEntry_t;

/* ========================= 事件数据结构 ========================= */
typedef struct
{
    IOT_EventType_e		e_type;								/* 事件类型 */
    char				para_buf[IOT_RX_CMD_MAX_SIZE];		/* 非全字匹配时，解析关键字后的属性参数 */
} IOT_Event_t;

/* ========================= 回调函数类型 ========================= */
/**
 * @brief 事件回调，由上层（iot_user_app）注册
 * @param evt  解析完成的事件结构体指针
 */
typedef void (*IOT_EventCallback_t)(const IOT_Event_t *evt);

/* ========================= 公共接口 ========================= */

/**
 * @brief  初始化 AT 解析层，注册事件回调
 * @param  cb  上层事件处理回调
 */
void IOT_Lib_Init(IOT_EventCallback_t cb);

/**
 * @brief  主循环轮询函数，从串口读取数据并解析
 *         在 main loop 中周期调用（建议 ≤10ms 一次）
 */
void IOT_RX_Handle(void);

/**
 * @brief  发送 AT 指令（自动拼装 AT+<cmd>\r\n）
 * @param  cmd  不含 "AT+" 前缀和结束符的指令字符串，如 "SOFTSTART=2"
 *              上报 JSON 时传入 "MQTTSEND=<len>,<json>"
 */
void IOT_Lib_Send_ATCmd(const char *cmd);

/**
 * @brief  上报设备型号（上电必调）
 * @param  series   系列名，如 "MargaritaIOT"
 * @param  modelId  型号，如 "ADD8666"
 */
void IOT_Lib_ReportModelId(const char *series, const char *modelId);


//供外部用户调用接口，触发对应模式
/**
 * @brief  启动配网
 * @param  mode  5=新蓝牙配网
 */
void IOT_Lib_NewBleSet(void);

/**
 * @brief  启动配网
 * @param  mode  2=热点配网
 */
void IOT_Lib_WifiAPSet(void);

/**
 * @brief  WiFi 模组恢复出厂设置
 */
void IOT_Lib_FactoryReset(void);

/**
 * @brief  设备解绑
 */
void IOT_Lib_Unbind(void);



#ifdef __cplusplus
}
#endif

#endif /* IOT_LIB_H */

/**** (c) *************************** END OF FILE **/
