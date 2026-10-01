#include "ws2812b.h"

volatile uint32_t flag_rdy = 0;
static uint16_t led_array[DATA_LEN];

//PB6 TIM8 CH1 AF5
void ws2812b_init(void){
    flag_rdy = 0;

    RCC->AHB1ENR |= RCC_AHB1ENR_DMAMUX1EN|RCC_AHB1ENR_DMA1EN;
    RCC->AHB2ENR |=  RCC_AHB2ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM8EN; 
    //PB6
    GPIOB->MODER &= ~GPIO_MODER_MODER6;
    GPIOB->MODER |= GPIO_MODER_MODER6_1;

    GPIOB->OTYPER &= ~GPIO_OTYPER_OT_6;
    GPIOB->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR6;

    GPIOB->PUPDR &= ~GPIO_PUPDR_PUPDR6;

    GPIOB->AFR[0] &= ~GPIO_AFRL_AFRL6;
    GPIOB->AFR[0] |= (5 << GPIO_AFRL_AFSEL6_Pos);

    //TIM8 CH1
    TIM8->CCER |= TIM_CCER_CC1E;  
    TIM8->CCMR1 &= ~(TIM_CCMR1_OC1M);
    TIM8->CCMR1 |= TIM_CCMR1_OC1M_2|TIM_CCMR1_OC1M_1|TIM_CCMR1_OC1PE;

    TIM8->CR1 |= TIM_CR1_OPM|TIM_CR1_ARPE;  

    TIM8->PSC = 0;
    TIM8->CNT = 0;
    TIM8->CCR1 = 0;
    TIM8->ARR = WS2812B_TIMER_AAR;

    TIM8->EGR |= TIM_EGR_UG;
    while(!(TIM8->SR & TIM_SR_UIF)){};
    TIM8->SR = 0;
    TIM8->BDTR |= TIM_BDTR_MOE;

    TIM8->CR1 |= 1;
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    NVIC_SetPriority(DMA1_Channel1_IRQn,6);
    ws2812b_buff_clear();
    ws2812b_send();
}



/*
STM32G431 Cat2
DMAMUX channels 0 to 5 are connected to DMA1 channels 1 to 6
DMAMUX channels 6 to 11 are connected to DMA2 channels 1 to 6

STM32G474 Cat3
For category 3 and category 4 devices:
DMAMUX channels 0 to 7 are connected to DMA1 channels 1 to 8
DMAMUX channels 8 to 15 are connected to DMA2 channels 1 to 8
*/

int ws2812b_send(void){
    
    DMAMUX1_Channel0->CCR = 49;
    
    DMA1_Channel1->CCR = 0;
    DMA1_Channel1->CNDTR = sizeof(led_array)/sizeof(led_array[0]);
    DMA1_Channel1->CPAR = (uint32_t)(&TIM8->CCR1);
    DMA1_Channel1->CMAR = (uint32_t)(led_array); 

    DMA1_Channel1->CCR = DMA_CCR_MINC |\
                       DMA_CCR_PSIZE_0|\
                       DMA_CCR_MSIZE_0|\
                       DMA_CCR_PL|\
                       DMA_CCR_TCIE|\
                       DMA_CCR_DIR;

    TIM8->CR1 = 0;
    TIM8->ARR = WS2812B_TIMER_AAR;
    TIM8->CCR1 = 0x0000; 
    TIM8->CNT = 0; 

    TIM8->EGR |= TIM_EGR_UG;
    while(!(TIM8->SR & TIM_SR_UIF)){};
    TIM8->SR = 0;

    TIM8->DIER = TIM_DIER_CC1DE; 
    TIM8->CR1 |= TIM_CR1_CEN; 

    DMA1->IFCR = DMA_IFCR_CTEIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTCIF1 | DMA_IFCR_CGIF1;
    DMA1_Channel1->CCR |= DMA_CCR_EN; 

    return 0;
}

void DMA1_Channel1_IRQHandler(void){
  TRACE_ENTER_ISR;
  DMA1_Channel1->CCR &= ~DMA_CCR_EN;
  DMA1->IFCR = DMA_IFCR_CTEIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTCIF1 | DMA_IFCR_CGIF1;
  flag_rdy = 1;
  TRACE_EXIT_ISR;
}


void ws2812b_buff_clear(void){
  for(uint32_t i = 0; i<(WS2812B_NUM_LEDS*24); i++){
    led_array[i] = WS2812B_0_VAL;
  } 
  for(uint32_t i=(WS2812B_NUM_LEDS*24); i<DATA_LEN; i++){
    led_array[i] = 0;  
  }
}

int ws2812b_set(int pixn, uint8_t r, uint8_t g, uint8_t b){
  int offset = pixn*24;
  int i;
  uint8_t tmp;
  
  if(pixn > (WS2812B_NUM_LEDS - 1))
    return 1;
  
  //g component
  tmp = g;
  for(i=0; i<8; i++){
    if(tmp & 0x80)
      led_array[offset + i] = WS2812B_1_VAL;
    else
      led_array[offset + i] = WS2812B_0_VAL;
    tmp<<=1;
  }
  
  //r component
  tmp = r;
  for(i=0; i<8; i++){
    if(tmp & 0x80)
      led_array[offset + i + 8] = WS2812B_1_VAL;
    else
      led_array[offset + i + 8] = WS2812B_0_VAL;
    tmp<<=1;
  }
  
  //b component
  tmp = b;
  for(i=0; i<8; i++){
    if(tmp & 0x80)
      led_array[offset + i + 16] = WS2812B_1_VAL;
    else
      led_array[offset + i + 16] = WS2812B_0_VAL;
    tmp<<=1;
  }
  
  return 0;
}

void ws2812b_set_rgb(int pixn, uint32_t rgb){
  uint8_t r,g,b;

  r = (rgb >> 16) & 0xFF;
  g = (rgb >> 8) & 0xFF;
  b = (rgb) & 0xFF;


  ws2812b_set(pixn,r,g,b);
}


int ws2812b_is_ready(void){
  return flag_rdy;
}

