/*******************************************************************************
 * @file    iot_user_app.h
 * @author  hexm
 * @version V1.0
 * @date    2026-04-20
 * @brief   IoT 业务应用层头文件（应用层）
 * @attention 健康生活事业部 - 软件及 IOT 组
******************************************************************************/
/**
 * 移植需要修改的点：
 * 1、 s_attrTable 属性映射表，根据实际需要实现的功能与通信协议进行 key /value 映射,
 *     并根据 key 的属性修改对应的 type (int/str数据类型)/write_enable(是否可写)/allReport_enable(全属性上报属性是否包含该属性)
 * 2、 根据实际机型修改 g_device_Series / g_device_ModelID / g_mcu_Manufacturer
 * 3、 修改 s_filterLifeConfig 和 s_filterResetConfig 滤芯的配置参数；如果没有滤芯功能，则 HAVE_FILTER 定义为0
 * 4、 由于差异上报不支持字符串比较，所以如有字符串参数变化后需要上报，需要主动调用 IOT_UserApp_InitiativeReport() 传入需要上报的属性符 主动上报
 * 5、 需要在 Device_Cmd_Process() 函数中实现APP指令下发的功能执行逻辑或变量赋值；
 * 6、 需要在 Device_ParamVlaue_Get() 函数中实现属性列表中的参数获取逻辑，返回获取到的参数值
 * 7、 属性表中储存的都是uint32_t 类型，,实际使用中请自行转换为实际参数类型
 * 
 * 本文件提供以下接口供外部调用：
 * IOT_GetNetTime();					发送获取网络时间指令
 * IOT_Return_NetTime();				读取返回网络时间的结果 
 * IOT_FactorTest();					发送进入产测模式的指令 
 * IOT_Return_FactoryResult();			读取返回产测的结果
 * IOT_UserApp_Init();					IOT初始化（上电后调用一次）
 * IOT_UserApp_Handle();				IOT事件处理主循环轮询（在 main loop 中 IOT_POLL_INTERVAL_MS (默认10ms) 周期调用）
 * IOT_UserApp_InitiativeReport();      需要主动上报某个属性的时候调用，每次只能上报一个属性
 */
#ifndef IOT_USER_APP_H
#define IOT_USER_APP_H

#include "iot_lib.h"
#include "iot_uart.h"
#include "iot_mcu_ota.h"
#include "dis_softwareVer.h"

#ifdef __cplusplus
extern "C" {
#endif


/* ========================= 宏定义 ========================= */
#define HAVE_FILTER    			1					//是否有滤芯功能

/*
 * 单包总长度上限（字节），含完整 AT 帧：AT+MQTTSEND=<len>,<json>\r\n
 */
#define IOT_PKG_MAX_SIZE        			IOT_UART_TX_BUF_SIZE    	/* 上报数据封包总长度上限（字节）*/
#define IOT_SINGLE_INSTRUCTION_MAX_LEN		(IOT_UART_RX_QUEUE_SIZE/2)	/* 下发单个指令最大长度（字节）*/


#define IOT_POLL_INTERVAL_MS        (10U)     							/* IOT_UserApp_Handle 调用周期（ms）*/
#define IOT_TX_INTERVAL_MS          (50U / IOT_POLL_INTERVAL_MS)     	/* 两次发送之间的最小间隔（ms）*/
#define IOT_BOOT_DELAY_MS           (3000U / IOT_POLL_INTERVAL_MS)    	/* 上电等待模组就绪时间（ms）*/
#define IOT_REPORT_INTERVAL_MS      (1000U / IOT_POLL_INTERVAL_MS)   	/* 属性差异上报最小间隔（ms）*/
#define IOT_FULL_REPORT_INTERVAL_MS (3600000U / IOT_POLL_INTERVAL_MS) 	/* 全属性定时上报间隔（ms）*/
#define IOT_MCU_OTA_TIME_MS 		(600000U / IOT_POLL_INTERVAL_MS) 	/* MCU OTA 超时时间（ms）*/

/* ========================= 属性描述符表 ========================= */

/*
 * KEY Vlue 设备属性表
 */
typedef enum
{
    VER,
    MCUMANUFACTURER,
    STATUS,
    INPUTTEMP,
    OUTPUTTEMP,
    SETTEMP,
    SETOUTLET,
    TOTALOUTLET,
    PURETDS,
    RAWTDS,
    WARMWATERSET,
    HOTWATERSET,
    OUTLET120SET,
    OUTLET350SET,
    OUTLET800SET,
    STERILIZERSTATE,
    REFRIGERATIONMODE,
    MAXFILTERRESET,
    PC5IN1FILTERRESET,
	AIPLAN,
	ERRORCODE,
	WARNING,
} DeviceParam_e;


/*
 * 数据类型标志：
 *   IOT_ATTR_INT  — 整型，JSON 值不加引号，如 "Status":1
 *   IOT_ATTR_STR  — 字符串，JSON 值加引号，如 "Ver":"0.0.1"
 *                   使用 IOT_ATTR_STR 时，pVal 须指向 char[] 首地址，
 *                   并将其强转为 const int32_t *
 *   IOT_ATTR_JSON  — JSON对象，JSON 值加引号，如 "Attribute":{"code":0,"msg":"OK"}
 */
typedef enum
{
	IOT_ATTR_INT = 0,   /* 整型属性 */
	IOT_ATTR_STR,       /* 字符串属性 */
	IOT_ATTR_JSON       /* JSON属性 */
} IOT_AttrType_t;

/*
 * 属性描述符：描述一个需要上报/接收的设备属性字段
 *   key      — 云端 JSON Key 字符串
 *   e_param  — 设备属性枚举表索引
 *   Int_Val  — 整型属性值
 *   p_StrVal — 字符型属性值指针
 *   type     — 数据类型
 *   writable — true：云端可下发写入此字段；false：只上报不接收
 * 	 allReport_enable：true=全属性上报使能
 */
typedef void (*IOT_AttrWriteCb_t)(void *pData);

typedef struct
{
	const char           *key;				/* 属性描述符 Key */
	DeviceParam_e        e_param;      		/* 设备属性表 */
	int32_t              Int_Val;			/* INT 值上次上报的值 */
	char                 *p_StrVal;			/* STR 值指针 */
	IOT_AttrType_t       type;				/* 数据类型 */
	bool_t               write_enable;  	/* true=云端可写 */
	bool_t               allReport_enable;  /* true=全属性上报使能 */
} IOT_AttrEntry_t;

typedef struct {
    const char *jsonKey;		/* JSON键名，如"ROFilterLife" */
    int x;						/* 第一阈值百分比 */
    const char *fx;				/* 第一阈值告警标识 */
    int y;						/* 第二阈值百分比 */
    const char *fy;				/* 第二阈值告警标识 */
} FilterLifeConfig_t;

typedef struct {
    const char *resetKey;			/* 复位指令键名，如"ROFilterReset" */
    const char *displayName;		/* 滤芯显示名称，如"RO" */
} FilterResetConfig_t;

/**
 * @brief 时间结构体，存储解析后的网络时间
 */
typedef enum {
	TIME_NULL,				// 无数据返回
    TIME_NETWORK_ERROR,		// 网络错误（未获取到时间）
	TIME_PARSE_SUCCESS,		// 时间解析成功    
    TIME_FORMAT_ERROR		// 响应格式错误
} TimeParseResult_t;

typedef struct {
	TimeParseResult_t result;	// 时间解析结果
    uint16_t year;    			// 4位年份(范围0~65535，满足4位纪年需求)
    uint8_t  month;   			// 月份(1~12，2位以内)
    uint8_t  day;     			// 日期(1~31，2位以内)
    uint8_t  hour;    			// 小时(0~23，2位以内)
    uint8_t  minute;  			// 分钟(0~59，2位以内)
    uint8_t  second;  			// 秒钟(0~59，2位以内)
	uint8_t  week;				// 星期(0~6，0代表星期日，1代表星期一，以此类推)
} NetTime_t;

/**
 * @brief 产测结果
 */
typedef enum {
    FACTORY_NULL,    	
    FACTORY_TESTING,    // 正在测试
    FACTORY_SUCCESS,	// 成功
	FACTORY_FAIL		// 失败
} FactoryResult_t;

/**
 * @brief 网络状态
 */
typedef enum {
    NET_STATUS_OFFLINE,			// 离线    	
    NET_STATUS_CONNECTING,		// 正在连接路由器
    NET_STATUS_CONNECTTED,		// 已连上路由器，未连上云服务器
	NET_STATUS_ONLINE			// 已连上云服务器
} NetWorkStatus_t;
/* ========================= JSON 构建工具 ========================= */

/*
 * 简易 JSON 构建器（支持自动分包，帧头在 Begin 时预写入）
 *
 *   buf        — 完整 AT 帧缓冲区，格式：AT+MQTTSEND=NNN,{...}\r\n
 *   jsonStart  — JSON 内容（'{'）在 buf 中的起始偏移，由 Begin 写入后固定
 *   pos        — 当前 JSON 内容写入位置（相对 buf 起始）
 *   hasField   — 当前包是否已有字段（用于逗号处理）
 *
 * 超包规则：当 pos > IOT_PKG_MAX_SIZE 时，
 * 自动闭合当前帧入队，重新开启新帧继续追加。
 */
typedef struct
{
	char     buf[IOT_PKG_MAX_SIZE];
	uint16_t jsonStart; /* JSON '{' 在 buf 中的偏移 */
	uint16_t pos;       /* 当前写入位置（buf 下标） */
	bool_t   hasField;	/* 当前帧是否已写入有效字段 */
} JsonBuilder_t;

/* ========================= 公共接口 ========================= */

/**
 * @brief  获取网络时间
 * 		   按实际需要使用调用
 */
void IOT_GetNetTime(void);

/**
 * @brief  返回网络时间
 * 		   按实际需要使用调用
 */
NetTime_t IOT_Return_NetTime(void);

/**
 * @brief  进入产测模式
 * 		   按实际需要使用调用
 */
void IOT_FactorTest(void);

/**
 * @brief  返回产测结果
 * 		   按实际需要使用调用
 */
FactoryResult_t IOT_Return_FactoryResult(void);

/**
 * @brief  返回联网状态
 * 		   按实际需要使用调用
 */
NetWorkStatus_t IOT_Return_NetStatus(void);

/**
 * @brief  错误代码上报
 * @param   errorCode 要上报的错误代码
 *         需要使用时调用
 */
void IOT_UserApp_ReportErrorCode(char *errorCode);

/**
 * @brief  告警代码单独上报
 * @param   waringCode 要上报的告警代码
 *         需要使用时调用
 */
void IOT_UserApp_ReportWaringCode(int32_t waringCode);

/**
 * @brief   主动上报某些属性
 * @param   DeviceParam_e
 * @note    需要主动上报某个属性的时候调用，每次只能上报一个属性
*/
void IOT_UserApp_InitiativeReport(DeviceParam_e param);

/**
 * @brief  应用层初始化（上电后调用一次）
 *         内部完成：IOT_Lib_Init、属性快照初始化
 */
void IOT_UserApp_Init(void);

/**
 * @brief  主循环轮询（在 main loop 中周期调用）
 *         内部调用 IOT_RX_Handle 并处理定时上报逻辑
 */
void IOT_UserApp_Handle(void);


#ifdef __cplusplus
}
#endif

#endif /* IOT_USER_APP_H */

/**** (c) *************************** END OF FILE **/
