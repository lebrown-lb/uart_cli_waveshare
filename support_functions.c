#include <stdint.h>
#include "support_functions.h"
#include "stm32l432xx.h"

void GPIO_Write_Pin(GPIO_TypeDef * GPIOx,uint8_t pin, uint8_t state)
{
    if (state) 
    {
        // Set the corresponding bit in the lower 16-bits of BSRR
        GPIOx->BSRR = (1U << pin);
    } 
    else 
    {
        // Reset the corresponding bit using the upper 16-bits of BSRR
        GPIOx->BSRR = (1U << (pin + 16));
    }


}

void SPI1_Write_Byte(uint8_t data)
{
    // 3. Wait until the Transmit Buffer Empty (TXE) flag is set
    while (!(SPI1->SR & SPI_SR_TXE));

    // 4. Write the byte to the Data Register
    *(volatile uint8_t *)&SPI1->DR = data;

    // 5. Wait for transmission to finish (BSY flag to clear)
    while (SPI1->SR & SPI_SR_BSY);

}
