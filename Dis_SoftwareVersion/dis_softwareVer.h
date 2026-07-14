/**
 *****************************************************************************
 * @file    dis_softwareVer.h
 * @author  hexm
 * @version V1.0.0
 * @date    2026-06-10
 * @brief   显示软件版本号模块头文件
 * @attention 健康生活事业部 - 软件及 IOT 组
 *****************************************************************************/
/**
 * 移植需要修改的点：
 * 1、DIS_VER_MODE 显示模式0：测试 1：正式
 * 2、BoardType_e、BoardChar_e 板类型、板代表的符号及对应版本号的定义，版本号可以宏定义指向获取
 * 3、根据上面的定义，修改配置数组宏定义：BOARD_CONFIGS_INITIALIZER
 * 4、包含对应接口的头文件
 * 
 * 本文件提供以下接口供外部调用：
 * void DisSoftwareVer_Start(void);				//启动版本显示流程，启动后会立即显示第一个板的标识
 * void DisSoftwareVer_Stop(void);				//停止版本显示流程，可在显示过程中随时调用以提前退出
 * void DisSoftwareVer_Process(void);			//版本显示处理函数，每10ms调用一次，确保时序准确
 * uint8_t DisSoftwareVer_IsActive(void);		//检查是否正在显示版本信息，0:未显示, 1:正在显示
 */

#ifndef DIS_SOFTWARE_VER_H
#define DIS_SOFTWARE_VER_H

#include <stdint.h>

//#include "digit_display.h"

/* 版本显示模式宏定义 */
#define DIS_VER_MODE          1     /* 0:开发阶段(UA,UB,UC...)，1:正式阶段(A,B,C...) */
#define DIS_CYCLE_MODE        1     /* 0:显示一次即退出，1:循环显示 */		

/* 调用时间 */
#define DIS_CALL_TIME        10     				/* 函数调用时间，单位ms，建议10ms周期调用 */
#define DIS_VER_TIME   		(2000 / DIS_CALL_TIME)  /* 显示时间，单位ms，默认2秒 */

/* ================ 用户配置区域 - 根据实际机型修改 ================ */

/* 板类型枚举 - 包含所有板 */
typedef enum {
    BOARD_MAIN_DISPLAY = 0,  /* 主显示板 */
    BOARD_MAIN_CTRL,         /* 主控板 */
    BOARD_SUB_DISPLAY,       /* 副显示屏 */
	BOARD_SUC_DISPLAY,
    BOARD_MAX_COUNT      /* 最大板数量 */
} BoardType_e;

/* 板类型字符枚举 - 与BoardType_e一一对应 */
typedef enum {
    BOARD_CHAR_MAIN_DISPLAY = 'A',  /* 主显示板字符: A */
    BOARD_CHAR_MAIN_CTRL = 'B',     /* 主控板字符: B */
    BOARD_CHAR_SUB_DISPLAY = 'C',   /* 副显示屏字符: C */
	BOARD_CHAR_SUC_DISPLAY = 'D',   /* 副显示屏字符: C */
} BoardChar_e;

/* 各板版本号宏定义 - 用户根据实际需求配置 */
#define BOARD_VERSION_MAIN_DISPLAY     23     /* 主显示板版本号 (00-99) */
#define BOARD_VERSION_MAIN_CTRL        20     /* 主控板版本号 (00-99) */
#define BOARD_VERSION_SUB_DISPLAY      99     /* 副显示屏版本号 (00-99) */
#define BOARD_VERSION_SUC_DISPLAY      359     /* 副显示屏版本号 (00-99) */

/* 板配置数组 - 编译时根据宏定义初始化 */
#define BOARD_CONFIGS_INITIALIZER  \
{                               \
	{BOARD_MAIN_DISPLAY, BOARD_CHAR_MAIN_DISPLAY, BOARD_VERSION_MAIN_DISPLAY}, \
	{BOARD_MAIN_CTRL,    BOARD_CHAR_MAIN_CTRL,    BOARD_VERSION_MAIN_CTRL},    \
	{BOARD_SUB_DISPLAY,  BOARD_CHAR_SUB_DISPLAY,  BOARD_VERSION_SUB_DISPLAY},  \
	{BOARD_SUC_DISPLAY,  BOARD_CHAR_SUC_DISPLAY,  BOARD_VERSION_SUC_DISPLAY},  \
}\

/* 数码管显示函数宏定义 - 可根据硬件平台修改 */
#define DIS_CLEAR()                 		DigitDisplay_Clear()
#define DIS_DIGIT_SHOW(digit,value)			DigitDisplay_ShowChar(digit,value)
#ifndef DIGIT_Off
#define DIGIT_Off							0
#endif


/* 结构体定义 */

/* 板配置结构体 - 包含板类型和版本号 */
typedef struct {
    BoardType_e boardType;    /* 板类型 */
    BoardChar_e boardChar;    /* 板字符 */
    uint16_t version;         /* 版本号 (0-359) */
} BoardConfig_t;

/* 版本显示控制结构体 */
typedef struct
{
    uint8_t currentIndex;                     /* 当前显示索引 */
    uint16_t displayTimer;                    /* 显示计时器(10ms计数) */
    uint8_t isShowingBoard;                   /* 当前显示状态 (0:显示版本号, 1:显示板标识) */
    uint8_t isActive;                         /* 是否处于版本显示模式 */
} VersionDisplayCtrl_t;


/* 函数声明 */
/********************************************************************************
 * @brief   启动版本显示流程
 * @details 从第一个板开始按顺序显示板标识和版本号
 * @param   void
 * @return  void
 * @note    启动后会立即显示第一个板的标识
 *******************************************************************************/
void DisSoftwareVer_Start(void);

/********************************************************************************
 * @brief   停止版本显示流程
 * @details 立即终止版本显示并清空数码管
 * @param   void
 * @return  void
 * @note    可在显示过程中随时调用以提前退出
 *******************************************************************************/
void DisSoftwareVer_Stop(void);

/********************************************************************************
 * @brief   版本显示处理函数
 * @details 在主循环中周期性调用，处理显示切换逻辑
 * @param   void
 * @return  void
 * @note    每10ms调用一次，确保时序准确
 *******************************************************************************/
void DisSoftwareVer_Process(void);

/********************************************************************************
 * @brief   检查是否正在显示版本信息
 * @details 查询版本显示模块的激活状态
 * @param   void
 * @return  0:未显示, 1:正在显示
 * @note    可用于判断是否可以执行其他显示任务
 *******************************************************************************/
uint8_t DisSoftwareVer_IsActive(void);

#endif

/**** (c) *************************** END OF FILE **/
