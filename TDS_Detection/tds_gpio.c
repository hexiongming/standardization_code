#include "tds_gpio.h"
#include "tds_calculation.h"


//示例，可自我实现：
void TDS_PORT_Init_OUT(uint8_t ch)
{
	switch (ch)
	{
		case TDS_CH_1_P_PORT:
			PORT_Init(PORTD, PIN0, OUTPUT);
			break;
		case TDS_CH_1_N_PORT:
			PORT_Init(PORTD, PIN1, OUTPUT);
			break;
		case TDS_CH_2_P_PORT:
			PORT_Init(PORTD, PIN2, OUTPUT);
			break;
		case TDS_CH_2_N_PORT:
			PORT_Init(PORTD, PIN3, OUTPUT);
			break;
		
		default:
			break;
	}
}
void TDS_PORT_Init_IN(uint8_t ch)
{
	switch (ch)
	{
		case TDS_CH_1_P_PORT:
			PORT_Init(PORTD, PIN0, INPUT);
			break;
		case TDS_CH_1_N_PORT:
			PORT_Init(PORTD, PIN1, INPUT);
			break;
		case TDS_CH_2_P_PORT:
			PORT_Init(PORTD, PIN2, INPUT);
			break;
		case TDS_CH_2_N_PORT:
			PORT_Init(PORTD, PIN3, INPUT);
			break;
		
		default:
			break;
	}
}
void TDS_PORT_GPIO_SET(uint8_t ch)
{
	switch (ch)
	{
		case TDS_CH_1_P_PORT:
			PORT_SetBit(PORTD, PIN0);
			break;
		case TDS_CH_1_N_PORT:
			PORT_SetBit(PORTD, PIN1);
			break;
		case TDS_CH_2_P_PORT:
			PORT_SetBit(PORTD, PIN2);
			break;
		case TDS_CH_2_N_PORT:
			PORT_SetBit(PORTD, PIN3);
			break;
		
		default:
			break;
	}
}
void TDS_PORT_GPIO_CLR(uint8_t ch)
{
	switch (ch)
	{
		case TDS_CH_1_P_PORT:
			PORT_ClrBit(PORTD, PIN0);
			break;
		case TDS_CH_1_N_PORT:
			PORT_ClrBit(PORTD, PIN1);
			break;
		case TDS_CH_2_P_PORT:
			PORT_ClrBit(PORTD, PIN2);
			break;
		case TDS_CH_2_N_PORT:
			PORT_ClrBit(PORTD, PIN3);
			break;
		
		default:
			break;
	}
}
void TDS_PORT_GPIO_XOR(uint8_t ch)
{
	switch (ch)
	{
		case TDS_CH_1_P_PORT:
			PORT_ToggleBit(PORTD, PIN0);
			break;
		case TDS_CH_1_N_PORT:
			PORT_ToggleBit(PORTD, PIN1);
			break;
		case TDS_CH_2_P_PORT:
			PORT_ToggleBit(PORTD, PIN2);
			break;
		case TDS_CH_2_N_PORT:
			PORT_ToggleBit(PORTD, PIN3);
			break;
		
		default:
			break;
	}
}
uint16_t TDS_PORT_GPIO_READ(uint8_t ch)
{
	uint16_t ret = 0;
	switch (ch)
	{
		case TDS_CH_1_P_PORT:
			ret = PORT_GetBit(PORTD, PIN0);
			break;
		case TDS_CH_1_N_PORT:
			ret = PORT_GetBit(PORTD, PIN1);
			break;
		case TDS_CH_2_P_PORT:
			ret = PORT_GetBit(PORTD, PIN2);
			break;
		case TDS_CH_2_N_PORT:
			ret = PORT_GetBit(PORTD, PIN3);
			break;
		
		default:
			break;
	}
	return ret;
}




