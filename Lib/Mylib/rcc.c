#include "rcc.h"
#include "dwt.h"

void RCC_init(void){

	uint32_t tmp;

	dwt_init();

	RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
	(void)RCC->APB1ENR1;

	//Отключение dead battery pull-down UCPD1 на CC1/CC2 (PB6 - LED, PB4 - CAN3_S)
	PWR->CR3 |= PWR_CR3_UCPD_DBDIS;

	//Перед переходом в boost AHB делится на 2 (RM0440 6.1.5)
	tmp = RCC->CFGR;
	tmp &= ~RCC_CFGR_HPRE;
	tmp |= RCC_CFGR_HPRE_DIV2;
	RCC->CFGR = tmp;

	//Scale 1, boost mode
	PWR->CR5 &= ~PWR_CR5_R1MODE;
	tmp = PWR->CR1;
	tmp &= ~PWR_CR1_VOS;
	tmp |= PWR_CR1_VOS_0;
	PWR->CR1 = tmp;

	while(PWR->SR2 & PWR_SR2_VOSF){};

	tmp = FLASH->ACR;
	tmp &= ~FLASH_ACR_LATENCY;
	tmp |= FLASH_ACR_LATENCY_4WS;
	FLASH->ACR = tmp;

	while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_4WS){};

	RCC->CR |= RCC_CR_HSEON;
	while(!(RCC->CR & RCC_CR_HSERDY)){};

	RCC->CR &= ~(RCC_CR_PLLON);
	while(RCC->CR & RCC_CR_PLLRDY){};

	RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLM|RCC_PLLCFGR_PLLN|RCC_PLLCFGR_PLLR|RCC_PLLCFGR_PLLSRC|RCC_PLLCFGR_PLLQ|RCC_PLLCFGR_PLLQEN);
	RCC->PLLCFGR |= ((PLL_M - 1) << RCC_PLLCFGR_PLLM_Pos)|\
					(PLL_N << RCC_PLLCFGR_PLLN_Pos)|\
					((PLL_R) << RCC_PLLCFGR_PLLR_Pos)|\
					(PLL_Q << RCC_PLLCFGR_PLLQ_Pos)|\
					(RCC_PLLCFGR_PLLQEN)|\
					(RCC_PLLCFGR_PLLSRC_HSE << RCC_PLLCFGR_PLLSRC_Pos);

	RCC->CR |= RCC_CR_PLLON;
	RCC->PLLCFGR |= RCC_PLLCFGR_PLLREN;

	while((RCC->CR & RCC_CR_PLLRDY) == 0){};

	tmp = RCC->CFGR;
	tmp &= ~(RCC_CFGR_SW);
	tmp |= RCC_CFGR_SW;
	RCC->CFGR = tmp;

	while((RCC->CFGR & RCC_CFGR_SWS) != (RCC_CFGR_SWS)) {};

	//Не менее 1 мкс на AHB/2, затем AHB без делителя
	tmp = DWT->CYCCNT;
	while((DWT->CYCCNT - tmp) < (SYSCLK_FREQ / 1000000UL)){};
	RCC->CFGR &= ~RCC_CFGR_HPRE;

	SystemCoreClockUpdate();

	DEBUG("SystemCoreClock %d",SystemCoreClock);
}
