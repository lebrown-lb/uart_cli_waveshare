#define STM32L432xx
#include "stm32l4xx.h"

volatile uint8_t led_flg = 0;
volatile uint8_t usart2_flg = 0;
volatile char rx_byte = 0;

void SystemInit(void) 
{
    // 1. Enable Power Control clock
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    
    // 2. Select Voltage Scaling Range 1 (Required for > 26 MHz up to 80 MHz)
    PWR->CR1 &= ~PWR_CR1_VOS;
    PWR->CR1 |= PWR_CR1_VOS_0; // VOS = 01 (Range 1)
    
    // 3. Enable MSI and set range to 4 MHz
    RCC->CR |= RCC_CR_MSION;
    while ((RCC->CR & RCC_CR_MSIRDY) == 0);
    
    RCC->CR &= ~RCC_CR_MSIRANGE;
    RCC->CR |= RCC_CR_MSIRANGE_6; // Range 6 = 4 MHz
    RCC->CR |= RCC_CR_MSIRGSEL;   // Use MSIRANGE from CR
    
    // 4. Configure Flash Latency to 4 Wait States (for 80 MHz @ VOS Range 1)
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |= FLASH_ACR_LATENCY_4WS;
    FLASH->ACR |= FLASH_ACR_PRFTEN; // Enable Prefetch
    FLASH->ACR |= FLASH_ACR_ICEN;   // Instruction Cache
    FLASH->ACR |= FLASH_ACR_DCEN;   // Data Cache

    // 5. Configure Main PLL: VCO input = 4 MHz (MSI), PLLN = 40, PLLR = 2
    // VCO frequency = 4 MHz * 40 = 160 MHz
    // SYSCLK = 160 MHz / 2 = 80 MHz
    RCC->PLLCFGR = 0; // Reset PLL configuration
    RCC->PLLCFGR |= (RCC_PLLCFGR_PLLSRC_MSI << RCC_PLLCFGR_PLLSRC_Pos);
    RCC->PLLCFGR |= (40 << RCC_PLLCFGR_PLLN_Pos);
    RCC->PLLCFGR |= ((0) << RCC_PLLCFGR_PLLM_Pos); // PLLM = 1 (division factor 1)
    RCC->PLLCFGR |= RCC_PLLCFGR_PLLREN;           // Enable PLLR output
    RCC->PLLCFGR |= (0 << RCC_PLLCFGR_PLLR_Pos);  // PLLR = 2 (bit value 0)

    // 6. Enable Main PLL and wait for ready flag
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0);

    // 7. Select PLL as System Clock Source
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    // 8. Configure AHB/APB prescalers to 1 (HCLK = 80 MHz, PCLK1 = 80 MHz, PCLK2 = 80 MHz)
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
}

void delay(volatile uint32_t count) 
{
    while (count--) 
    {
        __asm__("nop");
    }
}

void GPIO_Init(void)
{
    // --- Configure PA2 (TX) ---
    // Set PA2 mode to Alternate Function (10 binary)
    GPIOA->MODER &= ~GPIO_MODER_MODE2_Msk;
    GPIOA->MODER |= (2UL << GPIO_MODER_MODE2_Pos);

    // Set PA2 Alternate Function to AF7 (0111 binary) on Low Register
    GPIOA->AFR[0] &= ~(0xFUL << GPIO_AFRL_AFSEL2_Pos);
    GPIOA->AFR[0] |= (7UL << GPIO_AFRL_AFSEL2_Pos);

    // --- Configure PA15 (RX) ---
    // Set PA15 mode to Alternate Function (10 binary)
    GPIOA->MODER &= ~GPIO_MODER_MODE15_Msk;
    GPIOA->MODER |= (2UL << GPIO_MODER_MODE15_Pos);

    // Set PA15 Alternate Function to AF3 (0011 binary) on High Register
    GPIOA->AFR[1] &= ~(0xFUL << GPIO_AFRH_AFSEL15_Pos);
    GPIOA->AFR[1] |= (3UL << GPIO_AFRH_AFSEL15_Pos);

    // Optional: Set high-speed output for TX to ensure crisp edges
    GPIOA->OSPEEDR |= (3UL << GPIO_OSPEEDR_OSPEED2_Pos);
    
    // Set PB3 as output
    GPIOB->MODER &= ~(3U << (3 * 2));
    GPIOB->MODER |=  (1U << (3 * 2));

}

void USART2_Init(void)
{
    // Disable USART2 before configuring control bits
    USART2->CR1 &= ~USART_CR1_UE;

    // Configure Word Length: 8-bit Data (M1=0, M0=0)
    USART2->CR1 &= ~(USART_CR1_M0 | USART_CR1_M1);

    // Configure Parity: None (PCE=0)
    USART2->CR1 &= ~USART_CR1_PCE;

    // Configure Stop Bits: 1 Stop Bit (STOP=00 binary in CR2)
    USART2->CR2 &= ~USART_CR2_STOP_Msk;

    // Set Baud Rate Divider (Calculated for 80MHz clock to 115200 baud)
    USART2->BRR = 694; 

    // Enable Transmitter (TE), Receiver (RE), and USART peripheral (UE)
    USART2->CR1 |= (USART_CR1_TE | USART_CR1_RE | USART_CR1_UE);

    // Enable the RX Not Empty Interrupt (RXNEIE)
    USART2->CR1 |= USART_CR1_RXNEIE;

    // USART2 global Interrupt vector index is 38 on STM32L432xx
    // 1. Set priority (Optional: e.g., priority level 2 out of 0-15)
    NVIC_SetPriority(USART2_IRQn, 2);

    // 2. Enable the USART2 interrupt line in the NVIC
    NVIC_EnableIRQ(USART2_IRQn);
}

void TIM1_Init(void) 
{
    // 1. Enable TIM1 clock (TIM1 is on APB2 bus)
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

    // 2. Set Prescaler and Auto-Reload for desired frequency
    // Assuming 4 MHz default MSI clock: Timer frequency = 4MHz / (PSC + 1)
    TIM1->PSC = 7999;  // 4000-1 -> Timer clock = 1000 Hz (1 ms tick)
    TIM1->ARR = 9999;   // 1000 ticks -> Overflow every 1 second

    
    // 3. Force register load and clear immediate flag
    TIM1->EGR |= TIM_EGR_UG;
    TIM1->SR &= ~TIM_SR_UIF;

    // 3. Enable Update Interrupt (optional)
    TIM1->DIER |= TIM_DIER_UIE;
    NVIC_SetPriority(TIM1_UP_TIM16_IRQn, 1); // Set safe priority
    NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);

    // 4. Advanced Timer Specific: Main Output Enable (MOE)
    TIM1->BDTR |= TIM_BDTR_MOE;

    // 5. Enable Timer Counter
    TIM1->CR1 |= TIM_CR1_CEN;
}

void USART2_WriteChar(char ch) 
{
    // Wait until Transmit Data Register Empty flag (TXE) is set
    while (!(USART2->ISR & USART_ISR_TXE));
    // Write character byte to Transmit Data Register
    USART2->TDR = ch;
}

void USART2_WriteString(const char * str)
{
    // Loop through each character until the null terminator '\0' is reached
    while (*str != '\0') {
        // Wait until Transmit Data Register Empty flag (TXE) is set
        while (!(USART2->ISR & USART_ISR_TXE));
        
        // Write character byte to Transmit Data Register and advance pointer
        USART2->TDR = *str++;
    }
    
    // Optional: Wait for Transmission Complete (TC) to ensure the last byte 
    // has physically cleared the shift register before leaving the function.
    while (!(USART2->ISR & USART_ISR_TC));

}

void TIM1_UP_TIM16_IRQHandler(void) 
{
    if (TIM1->SR & TIM_SR_UIF) 
    {  
        // Check update interrupt flag
        TIM1->SR &= ~TIM_SR_UIF;  // Clear flag
        led_flg = 1;
    }
}

void USART2_IRQHandler(void) 
{
    // Check if the Receive Data Register Not Empty flag is set
    if (USART2->ISR & USART_ISR_RXNE)
    {
        
        // Reading USART2->RDR automatically clears the RXNE flag
        rx_byte = (char)(USART2->RDR & 0xFF); 
        
        // Signal your main application loop that data is available
        usart2_flg = 1;
    }
    
    // Optional Check: Handle Overrun Error (ORE) if data comes too fast
    if (USART2->ISR & USART_ISR_ORE) 
    {
        // Clear overrun flag by writing to Clear Interrupt Flag Register
        USART2->ICR |= USART_ICR_ORECF;
    }
}

int main(void) 
{
    // Enable GPIOB clock (example for PB3)
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

    // Enable GPIOA clock (AHB2 bus)
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    // Enable USART2 clock (APB1 bus 1)
    RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;
    
    GPIO_Init();
    USART2_Init();
    TIM1_Init();
    __enable_irq(); 

    USART2_WriteString("PROGRAM START\r\n");

    while(1) 
    {
        // Toggle PB3
        if (led_flg)
        {
            GPIOB->ODR ^= (1U << 3);
            led_flg = 0;
        }
        if(usart2_flg)
        {
            USART2_WriteChar(rx_byte);
            if(rx_byte == '\n' || rx_byte == '\r')
                USART2_WriteString("\r\n");
            usart2_flg = 0;

        }
    }
}
