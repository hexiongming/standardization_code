/**
 *****************************************************************************
 * @file    dis_softwareVer.c
 * @author  hexm
 * @version V1.0.0
 * @date    2026-06-10
 * @brief   显示软件版本号模块，该文件包含 dis_softwareVer.h中的函数声明
 * @attention 健康生活事业部 - 软件及 IOT 组
 *****************************************************************************
 */

/* 头文件 */
#include "dis_softwareVer.h"

/* 静态变量 */
static VersionDisplayCtrl_t s_verCtrlData = {0};

/* 板配置数组 - 编译时根据宏定义初始化 */
static const BoardConfig_t s_boardConfigs[] = BOARD_CONFIGS_INITIALIZER;
// 计算板子配置数组长度
#define BOARD_CFG_COUNT (sizeof(s_boardConfigs) / sizeof(s_boardConfigs[0]))

//示例
uint16_t borard_version_main_display = 03;
uint16_t borard_version_main_ctrl = 13;
uint16_t borard_version_sub_display = 90;
uint16_t borard_version_suc_display = 359;
static uint16_t *app_ver = &borard_version_suc_display; //定义需要APP显示的版本号

/********************************************************************************
 * @brief   显示当前板类型或版本号
 * @details 根据当前状态显示板类型或版本号
 * @param   void
 * @return  void
 * @note    每2秒切换一次显示内容
 *******************************************************************************/
static void ShowCurrentBoardOrVersion(void)
{
    /* 检查当前索引是否有效 */
    if (s_verCtrlData.currentIndex >= BOARD_MAX_COUNT)
    {
        return;
    }
    
    /* 获取当前板的配置 */
    const BoardConfig_t* boardConfig = &s_boardConfigs[s_verCtrlData.currentIndex];
    
    if (s_verCtrlData.isShowingBoard)
    {
        /* 显示板类型 */
		if(DIS_VER_MODE == 0) 
		{                                                            
            /* 开发阶段: 十位数码管显示U，个位数码管显示板类型 */           
			DIS_DIGIT_SHOW(0, ('U' - 'A') + 10); 
		}
		else
		{ 
			DIS_DIGIT_SHOW(0,DIGIT_Off);
		}
		DIS_DIGIT_SHOW(1, boardConfig->boardChar - 'A' + 10); 
    }
    else
    {
        /* 显示版本号 */
		uint8_t tens = *(boardConfig->version) / 10;  //十位
        uint8_t units = *(boardConfig->version) % 10; //个位
        DIS_DIGIT_SHOW(0, tens); 
        DIS_DIGIT_SHOW(1, units);
    }
}

/* 外部接口函数实现 */
/********************************************************************************
 * @brief   启动版本显示流程
 * @details 从第一个有效板开始按顺序显示板类型和版本号
 * @param   void
 * @return  void
 * @note    启动后会立即显示第一个有效板的类型
 *******************************************************************************/
void DisSoftwareVer_Start(void)
{
    /* 检查是否有板需要显示 */
    if (BOARD_MAX_COUNT == 0)
    {
        return;
    }
    
    /* 重置显示状态 */
    s_verCtrlData.currentIndex = 0;
    s_verCtrlData.displayTimer = 0;
    s_verCtrlData.isShowingBoard = 1;  /* 从显示板类型开始 */
    s_verCtrlData.isActive = 1;
    
    /* 立即显示第一个板的类型 */
    ShowCurrentBoardOrVersion();
}

/********************************************************************************
 * @brief   停止版本显示流程
 * @details 立即终止版本显示并清空数码管
 * @param   void
 * @return  void
 * @note    可在显示过程中随时调用以提前退出
 *******************************************************************************/
void DisSoftwareVer_Stop(void)
{
    s_verCtrlData.isActive = 0;
    DIS_CLEAR();
}

/********************************************************************************
 * @brief   版本显示处理函数
 * @details 在主循环中周期性调用，处理显示切换逻辑
 * @param   void
 * @return  void
 * @note    每10ms调用一次，确保时序准确
 *******************************************************************************/
void DisSoftwareVer_Process(void)
{
    /* 如果未激活，直接返回 */
    if (!s_verCtrlData.isActive)
    {
        return;
    }
    
    /* 每10ms累加一次，DIS_VER_TIME次为2秒 */
    s_verCtrlData.displayTimer++;
    
    /* 每2秒切换一次显示内容 */
    if (s_verCtrlData.displayTimer >= DIS_VER_TIME)
    {
        s_verCtrlData.displayTimer = 0;
        
        if (s_verCtrlData.isShowingBoard)
        {
            /* 当前显示的是板类型(2秒)，切换到显示版本号 */
            s_verCtrlData.isShowingBoard = 0;
        }
        else
        {
            /* 当前显示的是版本号(2秒)，切换到下一个板的类型 */
            s_verCtrlData.isShowingBoard = 1;
            s_verCtrlData.currentIndex++;
            
            /* 检查是否已显示完所有板 */
            if (s_verCtrlData.currentIndex >= BOARD_MAX_COUNT)
            {
                #if(0 == DIS_CYCLE_MODE)
                /* 所有板显示完毕，退出版本显示模式 */
                DisSoftwareVer_Stop();
                return;
                #else
                /*循环显示模式 */
                s_verCtrlData.currentIndex = 0;
                #endif
            }
        }
		ShowCurrentBoardOrVersion();
    }
}

/********************************************************************************
 * @brief   检查是否正在显示版本信息
 * @details 查询版本显示模块的激活状态
 * @param   void
 * @return  0:未显示, 1:正在显示
 * @note    可用于判断是否可以执行其他显示任务
 *******************************************************************************/
uint8_t DisSoftwareVer_IsActive(void)
{
    return s_verCtrlData.isActive;
}

/********************************************************************************
 * @brief   当前版本号转换成APP显示格式
 * @details 根据 app_ver 转换成APP显示格式
 * @param   buf APP显示的版本号接收指针
 * @param   buf_len APP显示的版本号接收指针长度
 * @param   dis_mode 显示模式 0:显示主控版本，1:显示所有板子的版本号
 * @return  len 版本号长度
 * @note    如测试阶段显示板版本号为13，则APP显示为0.1.3
 *          如正式阶段显示板版本号为103，则APP显示为1.10.3 		
 *          若显示所有板子的版本号，则显示为0.A3.B13.C90.D359，或1.A3.B13.C90.D359
 *******************************************************************************/
uint8_t Return_APPShowVer(char *buf, uint16_t buf_len,uint8_t dis_mode)
{
	int len = 0;

    if(buf == NULL || buf_len == 0)
    {
        return 0;
    }
	if(dis_mode == 0)
    {
		len = snprintf(buf, buf_len, "%d.%d.%d", DIS_VER_MODE, (*app_ver)/10, (*app_ver)%10);
	}
	else if(dis_mode == 1)
	{
		char *pBuf = buf;
		size_t remainLen = buf_len;

		//第一个字节为测试或正式版本
		len = snprintf(pBuf, remainLen, "%d", DIS_VER_MODE);
		if(len >= remainLen)
		{
			// 缓冲区溢出，直接截断
			buf[buf_len - 1] = '\0';
		}
		pBuf += len;
		remainLen -= len;

		// 循环遍历所有板子配置，自动拼接
		for(uint8_t i = 0; i < BOARD_CFG_COUNT; i++)
		{
			len = snprintf(pBuf, remainLen, ".%c%d", s_boardConfigs[i].boardChar,*s_boardConfigs[i].version);
			if(len >= remainLen)
			{
				buf[buf_len - 1] = '\0';
				break;
			}
			pBuf += len;
			remainLen -= len;
		}

		// 最终字符串有效长度
		len = strlen(buf);
	}	
	return len;
}

/**** (c) *************************** END OF FILE **/
