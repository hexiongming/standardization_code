/*******************************************************************************
 * @file    iot_mcu_ota.h
 * @author  hexm
 * @version V1.0
 * @date    2026-05-20
 * @brief   mcu_ota处理文件
 * @attention 健康生活事业部 - 软件及 IOT 组
 *
 * 职责：处理WiFi模块给MCU OTA过程中的指令解析以及固件包传输
******************************************************************************/
/**
 * 移植时需要修改的点：
 * 1、 Mcu_Ota_Flash_Write();		函数需要实现，按实际硬件更改
 * 2、 MCU_OTA_START_ADDR			MCU OTA 起始地址
 * 3、 MCU_OTA_END_ADDR				MCU OTA 结束地址
 * 4、 需要包含flash硬件底层驱动头文件
 * 5、 MCU_OTA_UPDATE_MODE 			根据实际修改 0: 在boot中传输固件包    1: 在APP传输包，传输完成后在boot中重启覆盖
 *                                  若在boot传输固件包，则需要将 iot_mcu_ota.c 和 iot_mcu_ota.h 文件移植在boot程序中，
 * 									并在boot固件中将 MCU_OTA_UPDATE_MODE 中置1；
 * Mcu_OtaSetFlag_Reboot();需要自己实现具体功能：
 * 若是在boot中传输固件：
	1、写flash/eeprom 的ota标志
	2、重启进boot进行ymodem协议传输固件
	3、在重启进入boot后将 mcu_ota_data.e_mcu_ota_state = MCU_OTA_STATE_DOWNLOAD_DONE; ,再调用 Mcu_Ota_Ymodem_Handle();
 * 若是在app中传输固件：
	1、写flash/eeprom 的ota标志
	2、在boot中进行固件覆盖或跳转到新App区执行；

 * 5、通过 mcu_ota_data.e_mcu_ota_state == MCU_OTA_STATE_TRANSMIT_DONE; 判断是否传输完成，传输完成后重新执行新固件。

 * 其它无需更改
 */


#ifndef IOT_MCU_OTA_H
#define IOT_MCU_OTA_H

#include "iot_lib.h"

#include "flash.h"					// flash硬件底层驱动头文件

#ifdef __cplusplus
extern "C" {
#endif

//========================= MCU OTA配置 =========================
#define MCU_OTA_UPDATE_MODE         1									// 0: 在boot中传输固件包    1: 在APP传输包，传输完成后在boot中重启覆盖
#define MCU_OTA_PACKET_BUF_SIZE     (OTA_YMODEM_VALID_SIZE + 5U)		// Ymodem协议 OTA包传输大小 3字节头+data[]+2字节校验
#define MCU_OTA_START_ADDR          (96*1024)							//MCU OTA起始地址
#define MCU_OTA_END_ADDR            (116*1024)							//MCU OTA结束地址
#define MCU_ONE_PAGE_SIZE			512

#define MCU_OTA_TIME_OUT_COUNT      3000								//30秒超时 单位10ms

#define MCU_OTA_REBOOT()            Mcu_OtaSetFlag_Reboot()

//========================= Ymodem 常量 =========================
#define SOH     0x01    // 128B
#define STX     0x02    // 1024B
#define EOT     0x04
#define ACK     0x06
#define NAK     0x15
#define CAN     0x18
#define C       0x43

//=========================Ymodem step =========================
typedef enum {
	YMODEM_NULL = 0,
	YMODEM_HEADER,
	YMODEM_PACKET,
	YMODEM_EOT_NAK,
	YMODEM_EOT_ACK,
	YMODEM_END
} ymodem_step_e;

//=========================MCU OTA 状态 =========================
typedef enum {
	MCU_OTA_STATE_IDLE = 0,
	MCU_OTA_STATE_WAIT_DOWNLOAD,
	MCU_OTA_STATE_DOWNLOAD_DONE,
	MCU_OTA_STATE_SEND_YMODEM_SIZE,
	MCU_OTA_STATE_YMODEM_TRANSMIT,
	MCU_OTA_STATE_TRANSMIT_DONE,
	MCU_OTA_STATE_NEED_RESUME
} mcu_ota_state_e;



//========================= MCU ota 数据结构 =========================
typedef struct {
	mcu_ota_state_e  e_mcu_ota_state;				//OTA状态
	ymodem_step_e 	e_ymodem_step;
	uint32_t	packet_size;						//包大小
	uint32_t	packet_sum;							//分包总数
	uint32_t	packet_num;							//已接收的包数
	uint16_t	packet_serial_sum;					//已传输255包数据的次数
	uint32_t	receive_byte;						//已传输的字节数
} mcu_ota_data_t;



//========================= 外部文件调用接口 =========================

/********************************************************************************
 * @brief   Ymodem数据处理
 * @details 从RxQueueu获取数据包，并调用协议解析
 * @param   void
 * @return  Result_e
 * @note    在 Mcu_Ota_Get_State().e_mcu_ota_state == MCU_OTA_STATE_YMODEM_TRANSMIT 时调用
 *******************************************************************************/
Result_e Mcu_Ota_Ymodem_Handle(void) ;

/********************************************************************************
 * @brief   设置OTA当前状态
 * @details 设置OTA状态值
 * @param   mcu_ota_state_e
 * @return  /
 * @note    供应用层设置OTA状态
 *******************************************************************************/
void Mcu_Ota_Set_State(mcu_ota_state_e state);

/********************************************************************************
 * @brief   获取OTA所有信息
 * @details 获取OTA状态、进度、固件大小等信息
 * @param   void
 * @return  mcu_ota_data_t
 * @note    /
 *******************************************************************************/
mcu_ota_data_t Mcu_Ota_Get_State(void);

/********************************************************************************
 * @brief   启动Ymodem传输
 * @details 发送C命令启动Ymodem协议传输固件数据
 * @param   void
 * @return  void
 * @note    调用后进入TRANSMIT状态，开始接收Ymodem数据帧
 *******************************************************************************/
void Mcu_Ota_Start_Transmit(void);

/********************************************************************************
 * @brief   协商Ymodem传输字节
 * @details 发送AT+MCUOTASEND 命令协商Ymodem传输字节数
 * @param   void
 * @return  void
 * @note    在NEED_RESUME状态下调用，继续之前中断的传输
 *******************************************************************************/
void Mcu_Ota_HandShake_Ymodem_Size(void);

/********************************************************************************
 * @brief   重新开始Ymodem传输
 * @details 发送AT+MCUOTARESUME命令恢复中断的Ymodem传输
 * @param   void
 * @return  void
 * @note    在NEED_RESUME状态下调用，继续之前中断的传输
 *******************************************************************************/
void Mcu_Ota_Resume_Transmit(void);

/********************************************************************************
 * @brief   MCU OTA初始化
 * @details 保存底层硬件接口函数指针，初始化OTA状态机
 * @param   void
 * @return  void
 * @note    在IOT初始化和OTA启动时调用
 *******************************************************************************/
void Mcu_Ota_Init(void);




#ifdef __cplusplus
}
#endif




#endif
/**** (c) *************************** END OF FILE **/
