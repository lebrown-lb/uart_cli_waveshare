/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _SUPPORT_FUNCTIONS_H_
#define _SUPPORT_FUNCTIONS_H_

#include "stm32l432xx.h"

extern void GPIO_Write_Pin(GPIO_TypeDef * GPIOx,uint8_t pin, uint8_t state);
extern void SPI1_Write_Byte(uint8_t data);



#endif
/*-------------------------------END OF FILE-------------------------------*/
