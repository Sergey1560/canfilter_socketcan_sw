#include "systime.h"
#include "rcc.h"

static volatile uint32_t ms_ticks = 0;

void systime_init(void){
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
	(void)RCC->APB1ENR1;

	//TIM2 на APB1 без делителя, 1 МГц
	TIM2->CR1 = 0;
	TIM2->PSC = SYSCLK_FREQ / 1000000UL - 1;
	TIM2->ARR = 0xFFFFFFFF;
	TIM2->CNT = 0;
	TIM2->EGR = TIM_EGR_UG;
	TIM2->CR1 = TIM_CR1_CEN;

	SysTick_Config(SYSCLK_FREQ / 1000UL);
}

uint32_t systime_us(void){
	return TIM2->CNT;
}

uint32_t systime_ms(void){
	return ms_ticks;
}

void SysTick_Handler(void){
	ms_ticks++;
}
