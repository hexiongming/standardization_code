#ifndef TDS_GPIO_H
#define TDS_GPIO_H



#include <stdint.h>

//示例，可自我实现：
#include "gpio.h"



void TDS_PORT_Init_OUT(uint8_t ch);
void TDS_PORT_Init_IN(uint8_t ch);
void TDS_PORT_GPIO_SET(uint8_t ch);
void TDS_PORT_GPIO_CLR(uint8_t ch);
void TDS_PORT_GPIO_XOR(uint8_t ch);
uint16_t TDS_PORT_GPIO_READ(uint8_t ch);


#endif
