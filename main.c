#define STM32L432xx
#define RX_STR_LEN		80

#include <limits.h>
#include <ctype.h>
#include <string.h>
#include "stm32l4xx.h"
#include "ssd1331.h"

volatile uint8_t led_flg = 0;
volatile uint8_t usart2_flg = 0;
volatile uint8_t echo_en = 1;
volatile uint8_t info_en = 0;
volatile char rx_byte = 0;

__attribute__((weak)) int _close(int file) { (void)file; return -1; }
__attribute__((weak)) int _lseek(int file, int ptr, int dir) { (void)file; (void)ptr; (void)dir; return -1; }
__attribute__((weak)) int _read(int file, char *ptr, int len) { (void)file; (void)ptr; (void)len; return 0; }
__attribute__((weak)) int _write(int file, char *ptr, int len) { (void)file; (void)ptr; (void)len; return 0; }
__attribute__((weak)) int _fstat(int file, char *ptr, int len) { (void)file; (void)ptr; (void)len; return 0; }
__attribute__((weak)) int _getpid(void) { return 1;}
__attribute__((weak)) int _kill(int pid, int sig) {(void)pid; (void)sig; return -1;}
__attribute__((weak)) int _isatty(int file) {(void)file; return 1;}


/* Private function prototypes -----------------------------------------------*/
void GPIO_Init(void);
void SPI1_Init(void);
void USART2_Init(void);
void TIM1_Init(void);

void USART2_WriteChar(char ch);
void USART2_WriteString(const char * str);

void cmd_handler(char * cmd_str);
void text_cmd_parse_and_exec(char * value_str, uint8_t chSize);
void rect_cmd_parse_and_exec(char * value_str);
void fill_cmd_parse_and_exec(char * value_str);
void line_cmd_parse_and_exec(char * value_str);
void circle_cmd_parse_and_exec(char * value_str);
enum Color string_to_color(char * str);
unsigned long string_to_uint(const char *nptr, char **endptr);
static size_t int_to_str(char *buf, size_t max_len, int value); 


int main(void) 
{
    uint8_t uart2_cnt = 0;
	char uart2_str[RX_STR_LEN];

    // Enable GPIOB clock (example for PB3)
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

    // Enable GPIOA clock (AHB2 bus)
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    // Enable USART2 clock (APB1 bus 1)
    RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;

    // Enable SPI1 clock
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    
    GPIO_Init();
    USART2_Init();
    SPI1_Init();
    TIM1_Init();
    __enable_irq(); 

    USART2_WriteString("PROGRAM START\r\n");
    ssd1331_init();
    ssd1331_clear_screen(BLACK);
    ssd1331_draw_rect(0, 0, 95, 63, PURPLE);
    ssd1331_draw_rect(5, 5, 85, 53, YELLOW);
    ssd1331_draw_rect(10, 10, 75, 43, RED);
    ssd1331_draw_rect(15, 15, 65, 33, GREEN);
    ssd1331_draw_rect(20, 20, 55, 23, BLUE);
    ssd1331_display_string(33,23, "LAB",FONT_1608, CYAN, BLACK);

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
			if (echo_en)
				USART2_WriteChar(rx_byte);

			if ((rx_byte == '\n') || (rx_byte == '\r'))
			{
				uart2_str[uart2_cnt] = '\0';
				USART2_WriteString("\n\r");
				cmd_handler(uart2_str);
				memset(uart2_str,'\0', RX_STR_LEN );
				uart2_cnt = 0;
			}
			else if (uart2_cnt > RX_STR_LEN - 1)
			{
				USART2_WriteString("\n\rCharacter limit Exceeded\n\r");
			}
			else
			{
				uart2_str[uart2_cnt] = rx_byte;
				uart2_cnt++;
			}
			usart2_flg = 0;

        }
    }
}


/* Initalization Function Definitions -----------------------------------------------*/
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

void GPIO_Init(void)
{
    // --- Configure PA2 (TX) ---
    // Set PA2 mode to Alternate Function (10 binary)
    // Set PA2 Alternate Function to AF7 (0111 binary) on Low Register
    GPIOA->MODER &= ~GPIO_MODER_MODE2_Msk;
    GPIOA->MODER |= (2UL << GPIO_MODER_MODE2_Pos);
    GPIOA->AFR[0] &= ~(0xFUL << GPIO_AFRL_AFSEL2_Pos);
    GPIOA->AFR[0] |= (7UL << GPIO_AFRL_AFSEL2_Pos);

    // --- Configure PA15 (RX) ---
    // Set PA15 mode to Alternate Function (10 binary)
    // Set PA15 Alternate Function to AF3 (0011 binary) on High Register
    GPIOA->MODER &= ~GPIO_MODER_MODE15_Msk;
    GPIOA->MODER |= (2UL << GPIO_MODER_MODE15_Pos);
    GPIOA->AFR[1] &= ~(0xFUL << GPIO_AFRH_AFSEL15_Pos);
    GPIOA->AFR[1] |= (3UL << GPIO_AFRH_AFSEL15_Pos);

    // Optional: Set high-speed output for TX to ensure crisp edges
    GPIOA->OSPEEDR |= (3UL << GPIO_OSPEEDR_OSPEED2_Pos);
    
        // 2. Configure PA5 (SCK), PA6 (MISO), PA7 (MOSI) for Alternate Function mode (10)
    GPIOA->MODER &= ~((3U << (5 * 2)) | (3U << (6 * 2)) | (3U << (7 * 2)));
    GPIOA->MODER |=  ((2U << (5 * 2)) | (2U << (6 * 2)) | (2U << (7 * 2)));

    // 3. Set high speed for PA5, PA6, PA7
    GPIOA->OSPEEDR |= ((3U << (5 * 2)) | (3U << (6 * 2)) | (3U << (7 * 2)));

    // 4. Map PA5, PA6, PA7 to Alternate Function 5 (SPI1) in AFR[0] (Low Register for pins 0-7)
    GPIOA->AFR[0] &= ~((0xFU << (5 * 4)) | (0xFU << (6 * 4)) | (0xFU << (7 * 4)));
    GPIOA->AFR[0] |=  ((5U  << (5 * 4)) | (5U  << (6 * 4)) | (5U  << (7 * 4))); 
     
    // Set PA4,PB3,PB4,PB5 as output
    GPIOA->MODER &= ~GPIO_MODER_MODE4_Msk;
    GPIOA->MODER |=  (1UL << GPIO_MODER_MODE4_Pos);
    GPIOB->MODER &= ~((3U << (3 * 2)) | (3U << (4 * 2)) | (3U << (5 * 2)));
    GPIOB->MODER |=  ((1U << (3 * 2)) | (1U << (4 * 2)) | (1U << (5 * 2)));

}

void SPI1_Init(void)
{
    // 3. Ensure SPI is disabled while changing configuration
    SPI1->CR1 &= ~SPI_CR1_SPE;

    // 4. Clear old BR bits and set new prescaler (e.g., divide by 16)
    // Formula: SPI_CLK = APB2_CLOCK / Prescaler
    SPI1->CR1 &= ~SPI_CR1_BR;          // Clears BR[2:0] (sets division by 2)
    SPI1->CR1 |= SPI_CR1_BR_0 | SPI_CR1_BR_1; // Sets BR = 011 -> divide by 16
    
        // 5. Configure SPI1 Control Register 1 (CR1)
    // Master mode (MSTR), Software slave management (SSM + SSI), Baud rate = fPCLK / 8 (BR = 010)
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (2U << SPI_CR1_BR_Pos);

    // 6. Configure SPI1 Control Register 2 (CR2)
    // 8-bit data size (DS = 0111), Motorola frame format, SS output enable if needed
    SPI1->CR2 = (7U << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;

    // 7. Enable SPI1 peripheral
    SPI1->CR1 |= SPI_CR1_SPE;
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



/* USART Communication Function Definitions -----------------------------------------------*/
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


/**
  * @brief uart command handler
  * @param cmd_str - command string
  * @retval None
  */
void cmd_handler(char * cmd_str)
{
	uint8_t i = 0;
	char cmd[RX_STR_LEN];
	char value[RX_STR_LEN];
	char tmp[50];
	uint8_t cmd_cnt = 0;
	uint8_t value_cnt = 0;
	char * str_ptr = cmd;
	char c;
	unsigned long x;
	char * endptr = NULL;


	value[0] = '\0';
	if (strchr(cmd_str, ':') == NULL)
	{
		strcpy(cmd, cmd_str);
		memset(value,'\0', RX_STR_LEN);
	}
	else
	{
		for (i = 0; i < strlen(cmd_str); i++)
		{
			c = cmd_str[i];
			if (c == ':')
			{
				*str_ptr = '\0';
				str_ptr = value;
			}
			else
			{
				*str_ptr = c;
				str_ptr++;
			}


		}
		*str_ptr = '\0';
	}
	cmd_cnt = strlen(cmd);
	value_cnt = strlen(value);

	if(info_en)
	{

        USART2_WriteString("CMD: ");
		int_to_str(tmp, sizeof(tmp), cmd_cnt);
		USART2_WriteString(tmp);
        USART2_WriteString(" VALUE: ");
		int_to_str(tmp, sizeof(tmp), value_cnt);
		USART2_WriteString(tmp);
        USART2_WriteString("\n\r");
	}


	if(strcmp("info", cmd) == 0)
	{
		if(value_cnt == 0)
		{
			int_to_str(tmp,sizeof(tmp), info_en);
			USART2_WriteString(tmp);
            USART2_WriteString("\n\r");
		}
		else
		{
			x = string_to_uint(value,&endptr);
			if(endptr == value)
				USART2_WriteString("INVALID INPUT!\n\r");
			else
				info_en = (uint8_t)x;
		}
	}
	else if(strcmp("echo", cmd) == 0)
	{
		if(value_cnt == 0)
		{
			int_to_str(tmp,sizeof(tmp), echo_en);
			USART2_WriteString(tmp);
            USART2_WriteString("\n\r");
		}
		else
		{
			x = string_to_uint(value,&endptr);
			if(endptr == value)
				USART2_WriteString("INVALID INPUT!\n\r");
			else
				echo_en = (uint8_t)x;
		}
	}
	else if(strcmp("clear", cmd) == 0)
	{
		if(value_cnt == 0)
			USART2_WriteString("clear:<color>\n\r");
		else
			ssd1331_clear_screen(string_to_color(value));
	}
	else if(strcmp("stext", cmd) == 0)
	{
		if(value_cnt == 0)
			USART2_WriteString("stext:<x>,<y>,<text>,<color>,<bcolor>\n\r");
		else
			text_cmd_parse_and_exec(value,FONT_1206);
	}
	else if(strcmp("ltext", cmd) == 0)
	{
		if(value_cnt == 0)
			USART2_WriteString("stext:<x>,<y>,<text>,<color>,<bcolor>\n\r");
		else
			text_cmd_parse_and_exec(value,FONT_1608);
	}
	else if(strcmp("rect", cmd) == 0)
	{
		if(value_cnt == 0)
			USART2_WriteString("rect:<x>,<y>,<l>,<h>,<color>\n\r");
		else
			rect_cmd_parse_and_exec(value);
	}
	else if(strcmp("fill", cmd) == 0)
	{
		if(value_cnt == 0)
			USART2_WriteString("fill:<x>,<y>,<l>,<h>,<color>\n\r");
		else
			fill_cmd_parse_and_exec(value);
	}
	else if(strcmp("line", cmd) == 0)
	{
		if(value_cnt == 0)
			USART2_WriteString("line:<x0>,<y0>,<x1>,<y1>,<color>\n\r");
		else
			line_cmd_parse_and_exec(value);
	}
	else if(strcmp("circle", cmd) == 0)
	{
		if(value_cnt == 0)
			USART2_WriteString("circle:<x>,<y>,<radius>,<color>\n\r");
		else
			circle_cmd_parse_and_exec(value);
	}

}


/**
  * @brief parses and executes a text command if value_str conditions are met
  * @param value_str - string containing parameters to execute the command
  * @retval None
  */
void text_cmd_parse_and_exec(char * value_str, uint8_t chSize)
{
	char * token = NULL;
	char * tokens[5] ={NULL,NULL,NULL,NULL,NULL};
	uint8_t cnt = 0;
	char * endptr = NULL;

	token = strtok(value_str, ",");
	if(token == NULL)
	{
		USART2_WriteString("INVALID VALUE FOR COMMAND!\n\r");
		return;
	}

	while(token != NULL)
	{
		if(cnt < 5)
			tokens[cnt] = token;
		token = strtok(NULL,",");
		cnt++;
	}

	if((cnt < 4)||(cnt > 5))
	{
		USART2_WriteString("INVALID OPTIONS FOR COMMAND!\n\r");
		return;
	}
	uint8_t xp = (uint8_t)string_to_uint(tokens[0], &endptr);
	if(endptr == tokens[0])
	{
		USART2_WriteString("<x> OPTIONS IS INVALID!\n\r");
		return;
	}
	uint8_t yp = (uint8_t)string_to_uint(tokens[1], &endptr);
	if(endptr == tokens[1])
	{
		USART2_WriteString("<y> OPTIONS IS INVALID!\n\r");
		return;
	}

	if(cnt == 5)
		ssd1331_display_string(xp,yp,tokens[2], chSize, string_to_color(tokens[3]), string_to_color(tokens[4]));
	else
		ssd1331_display_string(xp,yp,tokens[2], chSize, string_to_color(tokens[3]), BLACK);


}


/**
  * @brief parses and executes a rect command if value_str conditions are met
  * @param value_str - string containing parameters to execute the command
  * @retval None
  */
void rect_cmd_parse_and_exec(char * value_str)
{
	char * token = NULL;
	char * tokens[5] ={NULL,NULL,NULL,NULL,NULL};
	uint8_t cnt = 0;
    char * endptr = NULL;

	token = strtok(value_str, ",");
	if(token == NULL)
	{
		USART2_WriteString("INVALID VALUE FOR COMMAND!\n\r");
		return;
	}

	while(token != NULL)
	{
		if(cnt < 5)
			tokens[cnt] = token;
		token = strtok(NULL,",");
		cnt++;
	}

	if(cnt != 5)
	{
		USART2_WriteString("INVALID OPTIONS FOR COMMAND!\n\r");
		return;
	}
    uint8_t xp = (uint8_t)string_to_uint(tokens[0], &endptr);
	if(endptr == tokens[0])
	{
		USART2_WriteString("<x> OPTIONS IS INVALID!\n\r");
		return;
	}
	uint8_t yp = (uint8_t)string_to_uint(tokens[1], &endptr);
	if(endptr == tokens[1])
	{
		USART2_WriteString("<y> OPTIONS IS INVALID!\n\r");
		return;
	}
	uint8_t l = (uint8_t)string_to_uint(tokens[2], &endptr);
	if(endptr == tokens[2])
	{
		USART2_WriteString("<l> OPTIONS IS INVALID!\n\r");
		return;
	}
	uint8_t h = (uint8_t)string_to_uint(tokens[3], &endptr);
	if(endptr == tokens[3])
	{
		USART2_WriteString("<h> OPTIONS IS INVALID!\n\r");
		return;
	}

	ssd1331_draw_rect(xp,yp,l,h, string_to_color(tokens[4]));
}


/**
  * @brief parses and executes a fill command if value_str conditions are met
  * @param value_str - string containing parameters to execute the command
  * @retval None
  */
void fill_cmd_parse_and_exec(char * value_str)
{
	char * token = NULL;
    char * tokens[5] ={NULL,NULL,NULL,NULL,NULL};
    uint8_t cnt = 0;
    char * endptr = NULL;

    token = strtok(value_str, ",");
    if(token == NULL)
    {
        USART2_WriteString("INVALID VALUE FOR COMMAND!\n\r");
        return;
    }

    while(token != NULL)
    {
        if(cnt < 5)
            tokens[cnt] = token;
        token = strtok(NULL,",");
        cnt++;
    }

    if(cnt != 5)
    {
        USART2_WriteString("INVALID OPTIONS FOR COMMAND!\n\r");
        return;
    }
    uint8_t xp = (uint8_t)string_to_uint(tokens[0], &endptr);
	if(endptr == tokens[0])
	{
		USART2_WriteString("<x> OPTIONS IS INVALID!\n\r");
		return;
	}
	uint8_t yp = (uint8_t)string_to_uint(tokens[1], &endptr);
	if(endptr == tokens[1])
	{
		USART2_WriteString("<y> OPTIONS IS INVALID!\n\r");
		return;
	}
	uint8_t l = (uint8_t)string_to_uint(tokens[2], &endptr);
	if(endptr == tokens[2])
	{
		USART2_WriteString("<l> OPTIONS IS INVALID!\n\r");
		return;
	}
	uint8_t h = (uint8_t)string_to_uint(tokens[3], &endptr);
	if(endptr == tokens[3])
	{
		USART2_WriteString("<h> OPTIONS IS INVALID!\n\r");
		return;
	}

    ssd1331_fill_rect(xp,yp,l,h, string_to_color(tokens[4]));
}


/**
  * @brief parses and executes a line command if value_str conditions are met
  * @param value_str - string containing parameters to execute the command
  * @retval None
  */
void line_cmd_parse_and_exec(char * value_str)
{
	char * token = NULL;
    char * tokens[5] ={NULL,NULL,NULL,NULL,NULL};
    uint8_t cnt = 0;
    char * endptr = NULL;
    
    token = strtok(value_str, ",");
    if(token == NULL)
    {
        USART2_WriteString("INVALID VALUE FOR COMMAND!\n\r");
        return;
    }

    while(token != NULL)
    {
        if(cnt < 5)
            tokens[cnt] = token;
        token = strtok(NULL,",");
        cnt++;
    }

    if(cnt != 5)
    {
        USART2_WriteString("INVALID OPTIONS FOR COMMAND!\n\r");
        return;
    }
    uint8_t x0 = (uint8_t)string_to_uint(tokens[0], &endptr);
    if(endptr == tokens[0])
    {
        USART2_WriteString("<x0> OPTIONS IS INVALID!\n\r");
        return;
    }
    uint8_t y0 = (uint8_t)string_to_uint(tokens[1], &endptr);
    if(endptr == tokens[1])
    {
        USART2_WriteString("<y0> OPTIONS IS INVALID!\n\r");
        return;
    }
    uint8_t x1 = (uint8_t)string_to_uint(tokens[2], &endptr);
    if(endptr == tokens[2])
    {
        USART2_WriteString("<x1> OPTIONS IS INVALID!\n\r");
        return;
    }
    uint8_t y1 = (uint8_t)string_to_uint(tokens[3], &endptr);
    if(endptr == tokens[3])
    {
        USART2_WriteString("<y1> OPTIONS IS INVALID!\n\r");
        return;
    }

    ssd1331_draw_line(x0,y0,x1,y1, string_to_color(tokens[4]));
}


/**
  * @brief parses and executes a circle command if value_str conditions are met
  * @param value_str - string containing parameters to execute the command
  * @retval None
  */
void circle_cmd_parse_and_exec(char * value_str)
{
	char * token = NULL;
    char * tokens[4] ={NULL,NULL,NULL,NULL};
    uint8_t cnt = 0;
    char * endptr = NULL;

    token = strtok(value_str, ",");
    if(token == NULL)
    {
        USART2_WriteString("INVALID VALUE FOR COMMAND!\n\r");
        return;
    }

    while(token != NULL)
    {
        if(cnt < 4)
            tokens[cnt] = token;
        token = strtok(NULL,",");
        cnt++;
    }

    if(cnt != 4)
    {
        USART2_WriteString("INVALID OPTIONS FOR COMMAND!\n\r");
        return;
    }
    uint8_t xp = (uint8_t)string_to_uint(tokens[0], &endptr);
    if(endptr == tokens[0])
    {
        USART2_WriteString("<x> OPTIONS IS INVALID!\n\r");
        return;
    }
    uint8_t yp = (uint8_t)string_to_uint(tokens[1], &endptr);
    if(endptr == tokens[1])
    {
        USART2_WriteString("<y> OPTIONS IS INVALID!\n\r");
        return;
    }
    uint8_t radius = (uint8_t)string_to_uint(tokens[2], &endptr);
    if(endptr == tokens[2])
    {
        USART2_WriteString("<radius> OPTIONS IS INVALID!\n\r");
        return;
    }
    ssd1331_draw_circle(xp, yp, radius, string_to_color(tokens[3]));
}


/**
  * @brief converts a string to Color
  * @param str - string containing number
  * @retval Color enum value
  */
enum Color string_to_color(char * str)
{
	enum Color tmp;

	if((strcmp("BLACK", str) == 0) || (strcmp("black", str) == 0))
		tmp = BLACK;
	else if((strcmp("GREY", str) == 0) || (strcmp("grey", str) == 0))
		tmp = GREY;
	else if((strcmp("WHITE", str) == 0) || (strcmp("white", str) == 0))
		tmp = WHITE;
	else if((strcmp("RED", str) == 0) || (strcmp("red", str) == 0))
		tmp = RED;
	else if((strcmp("PINK", str) == 0) || (strcmp("pink", str) == 0))
		tmp = PINK;
	else if((strcmp("YELLOW", str) == 0) || (strcmp("yellow", str) == 0))
		tmp = YELLOW;
	else if((strcmp("GOLDEN", str) == 0) || (strcmp("golden", str) == 0))
		tmp = GOLDEN;
	else if((strcmp("BROWN", str) == 0) || (strcmp("brown", str) == 0))
		tmp = BROWN;
	else if((strcmp("BLUE", str) == 0) || (strcmp("blue", str) == 0))
		tmp = BLUE;
	else if((strcmp("CYAN", str) == 0) || (strcmp("cyan", str) == 0))
		tmp = CYAN;
	else if((strcmp("GREEN", str) == 0) || (strcmp("green", str) == 0))
		tmp = GREEN;
	else if((strcmp("PURPLE", str) == 0) || (strcmp("purple", str) == 0))
		tmp = PURPLE;
	else
		tmp = BLACK;

	return tmp;
}


/* Helper Function Definitions -----------------------------------------------*/
/**
  * @brief converts a string to unsigned int
  * @param str - string containing number
  * @param errnum - unsigned integer representing a success or failure
  * @retval None
  */
unsigned long string_to_uint(const char *nptr, char **endptr)
{

    const char *s = nptr;
    unsigned long acc = 0;
    int c;
    int any = 0;
    int neg = 0;

    // 1. Skip leading whitespace
    do {
        c = (unsigned char)*s++;
    } while (isspace(c));

    // 2. Handle optional sign
    if (c == '-') {
        neg = 1;
        c = *s++;
    } else if (c == '+') {
        c = *s++;
    }

    // 3. Pre-calculated limits for base 10 overflow protection
    const unsigned long cutoff = ULONG_MAX / 10;
    const int cutlim = (int)(ULONG_MAX % 10);

    // 4. Convert digits
    while (isdigit(c)) {
        c -= '0';
        
        // Check for overflow before multiplying
        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) {
            any = -1;
        } else {
            any = 1;
            acc = acc * 10 + c;
        }
        c = (unsigned char)*s++;
    }

    // 5. Finalize output and track end pointer
    if (any < 0) {
        acc = ULONG_MAX;
    } else if (neg) {
        acc = -acc; // Standard two's complement negation
    }

    if (endptr != 0) {
        // Step back by 1 because the while loop read one character past the last valid digit
        *endptr = (char *)(any ? s - 1 : nptr);
    }

    return acc;
}


/**
  * @brief converts an integer to string
  * @param buf - buffer to cunstruct string in
  * @param max_len - maximum length allowed by buffer
  * @param value - int to be converted
  * @retval size of integer string
  */
static size_t int_to_str(char *buf, size_t max_len, int value) 
{
    char temp[12]; // Fits 32-bit int + sign + null
    size_t i = 0;
    int is_negative = 0;

    if (value == 0) 
    {
        temp[i++] = '0';
    }
    else 
    {
        if (value < 0) 
        {
            is_negative = 1;
            // Handle INT_MIN overflow edge case safely
            unsigned int uval = (unsigned int)(-value);
            while (uval > 0) 
            {
                temp[i++] = (uval % 10) + '0';
                uval /= 10;
            }
        }
        else 
        {
            while (value > 0) 
            {
                temp[i++] = (value % 10) + '0';
                value /= 10;
            }
        }
    }

    if (is_negative) 
    {
        temp[i++] = '-';
    }

    // Reverse temp array into the destination buffer while checking limits
    size_t written = 0;
    while (i > 0 && written < max_len) 
    {
        buf[written++] = temp[--i];
    }
    if(written < max_len)
        buf[written] = '\0';
    else
       buf[max_len - 1] = '\0';

    return written;
}


/* Interrupt Function Definitions -----------------------------------------------*/
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
