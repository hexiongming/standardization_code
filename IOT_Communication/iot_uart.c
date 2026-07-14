/*******************************************************************************
 * @file    iot_uart.c
 * @author  hexm
 * @version V1.0.0
 * @date    2026-04-20
 * @brief   串口收发模块（底层）
 * @attention 健康生活事业部 - 软件及 IOT 组
 *
 * 职责：环形缓冲区管理 + 字节收发，不含任何协议逻辑。
 * 移植说明：将 HAL_UART_xxx 替换为目标平台的串口 API 即可。
******************************************************************************/
#include "iot_uart.h"




/* ========================= 全局缓冲区 ========================= */
static IOT_RxQueue_t s_iotRxQueue;    /* 接收环形缓冲区，通过 iot_uart.h extern 对外可见 */
static IOT_TxQueue_t  s_iotTxQueue;
static uint8_t s_TXBusy;				  /* 检查发送是否忙 */


/********************************************************************************
 * @brief   判断环形缓冲区是否已满
 * @details 通过比较写指针下一位是否等于读指针来判断满状态，
 *          牺牲一个字节换取无需额外计数变量
 * @param   rxbuf: 环形缓冲区指针
 * @return  bool: true=已满  false=未满
 * @note    仅供模块内部使用
 *******************************************************************************/
static inline bool Queue_IsFull(const IOT_RxQueue_t *rxbuf)
{
	return ((rxbuf->tail + 1) % IOT_UART_RX_QUEUE_SIZE) == rxbuf->head;
}

/********************************************************************************
 * @brief   判断环形缓冲区是否为空
 * @details 读写指针相等即为空
 * @param   rxbuf: 环形缓冲区指针
 * @return  bool: true=为空  false=非空
 * @note    仅供模块内部使用
 *******************************************************************************/
static inline bool Queue_IsEmpty(const IOT_RxQueue_t *rxbuf)
{
	return rxbuf->head == rxbuf->tail;
}

/********************************************************************************
 * @brief   向环形缓冲区压入一个字节
 * @details 缓冲区满时静默丢弃，不覆盖旧数据，保证已有数据完整性
 * @param   rxbuf:   环形缓冲区指针
 * @param   byte: 待压入的字节
 * @return  void
 * @note    在中断上下文中调用，禁止阻塞
 *******************************************************************************/
static inline void RX_Queue_Push(IOT_RxQueue_t *rxbuf, uint8_t byte)
{
	if (!Queue_IsFull(rxbuf))
	{
		rxbuf->buf[rxbuf->tail] = byte;
		rxbuf->tail = (rxbuf->tail + 1) % IOT_UART_RX_QUEUE_SIZE;
	}
	/* 满时丢弃，防止覆盖旧数据 */
}

/********************************************************************************
 * @brief   从环形缓冲区弹出一个字节
 * @details 缓冲区为空时返回 false，不修改输出参数
 * @param   rxbuf:   环形缓冲区指针
 * @param   byte: 输出字节的存储地址
 * @return  bool: true=成功弹出  false=缓冲区为空
 * @note    仅供模块内部使用
 *******************************************************************************/
static inline bool RX_Queue_Pop(IOT_RxQueue_t *rxbuf, uint8_t *byte)
{
	if (Queue_IsEmpty(rxbuf)) 
		return false;

	*byte = rxbuf->buf[rxbuf->head];
	rxbuf->head = (rxbuf->head + 1) % IOT_UART_RX_QUEUE_SIZE;
	return true;
}

/* ========================= 公共接口实现 ========================= */
/********************************************************************************
 * @brief   接收队列初始化
 * @details 清空接收环形缓冲区
 * @param   void
 * @return  void
 * @note    需要使用时调用一次
 *******************************************************************************/
void IOT_RxQueue_Init(void)
{
	memset(&s_iotRxQueue, 0, sizeof(IOT_RxQueue_t));
}
/********************************************************************************
 * @brief   串口模块初始化
 * @details 清空接收环形缓冲区，并完成底层串口硬件初始化
 * @param   uart_init_func: 硬件串口初始化函数，参数为波特率 
 * @return  void
 * @note    上电后调用一次
 *******************************************************************************/
void IOT_UART_Init(void)
{
	memset(&s_iotRxQueue, 0, sizeof(IOT_RxQueue_t));
	memset(&s_iotTxQueue, 0, sizeof(IOT_TxQueue_t));
	IOT_UART_PLATFORM_INIT(IOT_UART_BAUDRATE);
}

/********************************************************************************
 * @brief   发送指定长度的字节数组
 * @details 调用平台适配宏 IOT_UART_PLATFORM_SEND 完成实际发送，
 *          移植时将该宏替换为目标平台的串口发送 API
 * @param   data: 待发送数据缓冲区指针，不可为 NULL
 * @param   len:  发送字节数，为 0 时直接返回
 * @return  void
 * @note    当前为阻塞发送，如需 DMA/中断发送，修改平台适配宏即可
 *******************************************************************************/
static void IOT_UART_Send(uint8_t *data, uint16_t len)
{
	if (data == NULL || len == 0) 
		return;

	IOT_UART_PLATFORM_SEND(data, len);
}


/********************************************************************************
 * @brief   将字符串压入发送队列
 * @details 拷贝完整 AT 帧字符串到队列槽位，队列满时静默丢弃
 * @param   str: 待入队的完整 AT 帧字符串，如 "AT+SOFTSTART=2\r\n"
 * @return  bool: true=入队成功  false=队列已满
 * @note    所有上层发送操作均通过此接口入队，统一由 IOT_TX_Handle 发出
 *******************************************************************************/
bool IOT_UART_TxEnQueue(const char *str)
{
	if (str == NULL || s_iotTxQueue.count >= IOT_UART_TX_BUF_NUM) 
		return false;

	strncpy(s_iotTxQueue.txbuf[s_iotTxQueue.tail], str, IOT_UART_TX_BUF_SIZE - 1);
	s_iotTxQueue.txbuf[s_iotTxQueue.tail][IOT_UART_TX_BUF_SIZE - 1] = '\0';
	s_iotTxQueue.tail = (s_iotTxQueue.tail + 1) % IOT_UART_TX_BUF_NUM;
	s_iotTxQueue.count++;
	return true;
}

/********************************************************************************
 * @brief   设置t 忙状态
 * @details 供外部调用，设置 tx 忙状态
 * @param   void
 * @return  void
 * @note    在Tx发送时调用
 *******************************************************************************/
void IOT_TXSetBusy(void)
{
	s_TXBusy = 1;
}
/********************************************************************************
 * @brief   设置tx闲状态
 * @details 供外部调用，设置 tx 忙状态
 * @param   void
 * @return  void
 * @note    在 硬件串口中断处理函数发送完成后调用
 *******************************************************************************/
void IOT_TXSetIdle(void)
{
	s_TXBusy = 0;
	s_iotTxQueue.head = (s_iotTxQueue.head + 1) % IOT_UART_TX_BUF_NUM;
	s_iotTxQueue.count--;
}
/********************************************************************************
 * @brief   查询tx忙/闲状态
 * @details 供外部调用，查询 tx 忙/闲状态
 * @param   value
 * @return  void
 * @note    需要使用时调用，一般在要发数据前调用
 *******************************************************************************/
uint8_t IOT_TXBusy_Return(void)
{
	return s_TXBusy;
}

/********************************************************************************
 * @brief   查询TxQueue 空间
 * @details 供外部调用,查询TXQueue 剩余空间
 * @param   value
 * @return  void
 * @note    在TxQueue入队前调用查询剩余空间
 *******************************************************************************/
uint8_t IOT_TXQueue_Size_Return(void)
{
	return (IOT_UART_TX_BUF_NUM - s_iotTxQueue.count);
}

/********************************************************************************
 * @brief   发送队列轮询
 * @details 队列非空且发送间隔已到时，取队头一条数据发送并出队；
 *          间隔未到时保留队头，下次调用再试，不丢失数据
 * @param   void
 * @return  void
 * @note    在 IOT_RX_Handle 中每次调用
 *******************************************************************************/
void IOT_TX_Handle(void)
{
	/* 检查硬件是否正在发送，如果忙则直接返回 */
    if (s_TXBusy)
        return;
	if (s_iotTxQueue.count == 0) 
		return;

	IOT_UART_Send((uint8_t *)s_iotTxQueue.txbuf[s_iotTxQueue.head],
				  (uint16_t)strlen(s_iotTxQueue.txbuf[s_iotTxQueue.head]));

	IOT_TXSetBusy();
	// s_iotTxQueue.head = (s_iotTxQueue.head + 1) % IOT_UART_TX_BUF_NUM;
	// s_iotTxQueue.count--;
	
}

/*******************************************************************************
 * @brief   读取接收队列中存在的数据长度
 * @details 尝试从接收的环形队列读取有效数据大小。
 * @param   void
 * @return  已缓存的数据长度
 ******************************************************************************/
uint16_t IOT_RxQueue_DataLen(void)
{
	uint16_t head = s_iotRxQueue.head;
    uint16_t tail = s_iotRxQueue.tail;
    
    if (tail >= head)
    {
        return (uint16_t)(tail - head);
    }
    else
    {
        // 处理环绕情况：尾部在头部之前
        return (uint16_t)(IOT_UART_RX_QUEUE_SIZE - head + tail);
    }
}

/*******************************************************************************
 * @brief   从接收缓冲区读取n个字节的数据
 * @details 尝试从接收的环形队列读取n个字节有效数据。
 * @param   buf: 输出缓冲区指针
 * @param   len: 读取字节数
 * @return  成功读取的字节数，0表示无数据
 ******************************************************************************/
uint16_t IOT_RxQueue_ReadData(uint8_t *buff, uint16_t n)
{
	if (buff == NULL || n == 0)
    {
        return 0;
    }

    uint16_t count = 0;
    uint8_t byte;

    while (count < n)
    {
        if (!RX_Queue_Pop(&s_iotRxQueue, &byte))
        {
            break; // 队列已空，退出循环
        }
        buff[count++] = byte;
    }

    return count;
}
/********************************************************************************
 * @brief   检查接收队列中是否包含完整的一行数据
 * @details 遍历环形缓冲区，查找是否有 '\r' 或 '\n'。
 *          此操作不会修改头尾指针，不会消耗数据。
 * @param   void
 * @return  bool: true=存在完整行  false=数据不完整或为空
 * @note    用于在读取前预判，避免读出半截数据
 *******************************************************************************/
static bool IOT_RxQueue_HasCompleteLine(void)
{
    uint16_t i = s_iotRxQueue.head;
    uint8_t  byte;

    // 如果队列空，直接返回 false
    if (Queue_IsEmpty(&s_iotRxQueue))
        return false;

    // 遍历队列直到尾部
    while (i != s_iotRxQueue.tail)
    {
        byte = s_iotRxQueue.buf[i];
        
        // 发现行结束符
        if (byte == '\r' || byte == '\n')
            return true;
            
        // 移动索引到下一个位置
        i = (i + 1) % IOT_UART_RX_QUEUE_SIZE;
    }

    return false;
}
/********************************************************************************
 * @brief   从接收缓冲区读取一行数据
 * @details 以 '\r' 或 '\n' 作为行结束符，行首多余的换行符会被跳过。
 *          读取完成后在 buf 末尾自动追加 '\0'。
 *          在主循环中轮询调用，返回 0 表示暂无完整行可读。
 * @param   buf:    输出缓冲区指针
 * @param   maxLen: 缓冲区最大容量（含末尾 '\0'）
 * @return  uint16_t: 实际读取的有效字节数（不含 '\0'），0 表示无数据
 * @note    非线程安全，若在 RTOS 环境中使用需加互斥保护
 *******************************************************************************/
uint16_t IOT_UART_ReadLine(uint8_t *buf, uint16_t maxLen)
{
	uint16_t idx = 0;
	uint8_t  byte;

	if (buf == NULL || maxLen == 0)
        return 0;
		
	if (!IOT_RxQueue_HasCompleteLine())
    {
        return 0; /* 没有完整帧，直接返回 */
    }
	while (idx < (maxLen - 1))
	{
		if (!RX_Queue_Pop(&s_iotRxQueue, &byte))
			break;  /* 缓冲区空 */

		if (byte == '\r' || byte == '\n') 
		{
			if (idx > 0) 
				break;   		/* 遇到行尾，结束本行 */
			else 
				continue;        /* 跳过行首多余的 \r\n */
		}
		buf[idx++] = byte;
	}

	buf[idx] = '\0';
	return idx;
}

/********************************************************************************
 * @brief   UART 接收中断回调
 * @details 在平台 UART RX 中断服务函数（ISR）中调用，
 *          将接收到的字节压入环形缓冲区供主循环读取
 * @param   byte: 中断接收到的单个字节
 * @return  void
 * @note    此函数在中断上下文执行，禁止调用任何阻塞操作
 *******************************************************************************/
void IOT_UART_RxISR(uint8_t byte)
{
	RX_Queue_Push(&s_iotRxQueue, byte);
}

/**** (c) *************************** END OF FILE **/
