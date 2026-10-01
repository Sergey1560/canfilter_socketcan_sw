#include "led.h"
#include "rcc.h"

#include "ws2812b.h"

static void led_update_timer_init(void);
static void alive_blink(void);

//static uint32_t startup_color[] = {LED_COLOR_GREEN,LED_COLOR_RED,LED_COLOR_YELLOW,LED_COLOR_RED,LED_COLOR_GREEN,LED_COLOR_RED,LED_COLOR_GREEN};

static uint32_t blink_color[] = {LED_COLOR_GREEN,LED_COLOR_OFF};

void led_init(void){
    ws2812b_init();
    //Start update timer
    led_update_timer_init();

    alive_blink();
}

 static void led_update_timer_init(void){

    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM6EN;
    /*
    TIM_CLK SYSCLK_FREQ (APB1 без делителя)
    Timer UI = 10000
    */
	TIM6->PSC = SYSCLK_FREQ/10000 - 1;
	TIM6->ARR = 1000 - 1; 			//1000 - 10Hz
	TIM6->CNT=0;
	TIM6->DIER |= TIM_DIER_UIE;
	TIM6->CR1 |= TIM_CR1_CEN;
	NVIC_EnableIRQ(TIM6_DAC_IRQn);
 }

 void TIM6_DAC_IRQHandler(void){
    if(TIM6->SR & TIM_SR_UIF) {
		TIM6->SR &= ~TIM_SR_UIF;
        ws2812b_send();  
    }
 }


 static void alive_blink(void){

    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM7EN;
    /*
    TIM_CLK SYSCLK_FREQ (APB1 без делителя)
    Timer UI = 10000
    */
	TIM7->PSC = SYSCLK_FREQ/10000 - 1;
	TIM7->ARR = 5000 - 1; 			//1000 - 10Hz
	TIM7->CNT=0;
	TIM7->DIER |= TIM_DIER_UIE;
	TIM7->CR1 |= TIM_CR1_CEN;
	NVIC_EnableIRQ(TIM7_DAC_IRQn);


 }

 void TIM7_DAC_IRQHandler(void){
    static uint32_t index = 0;
    if(TIM7->SR & TIM_SR_UIF) {
		TIM7->SR &= ~TIM_SR_UIF;

        index = index ^ 1;
        ws2812b_set_rgb(2, blink_color[index]);

    }
 }

