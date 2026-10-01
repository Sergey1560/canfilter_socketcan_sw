#ifndef WS2812_H
#define WS2812_H
#include "common_defs.h"
#include "rcc.h"

//https://dimoon.ru/mikrokontrolleryi/drayver-svetodiodnoy-lentyi-na-ws2812b-dlya-stm32f103c8.html

#define WS2812B_NUM_LEDS        3
//#define DATA_LEN ((WS2812B_NUM_LEDS * 24) + 2)
//INT16U DMA_buf[LEDS_NUM+2][COLRS][8];

#define DATA_LEN ((WS2812B_NUM_LEDS+2) * 24) 


//TIM8 на APB2 без делителя
#define WS2812B_NS_TO_TICKS(ns) ((SYSCLK_FREQ / 1000000UL) * (ns) / 1000UL)

//Период следования бит в тиках таймера
//должно быть 1.25мкс
#define WS2812B_TIMER_AAR       (WS2812B_NS_TO_TICKS(1250) - 1)

//Передача лог. нуля 0.4мкс
#define WS2812B_0_VAL           (WS2812B_NS_TO_TICKS(350))
//Передача лог. единицы 0.85мкс
#define WS2812B_1_VAL           (WS2812B_NS_TO_TICKS(700))
//Сигнал RET или RESET более 50мкс
#define WS2812B_TIMER_RET       (WS2812B_TIMER_AAR * 60)


#define WS2812B_COLOR_BLUE      (uint32_t)0xFF
#define WS2812B_COLOR_GREEN     (uint32_t)0xFF00
#define WS2812B_COLOR_RED       (uint32_t)0xFF0000


void ws2812b_init(void);
void ws2812b_buff_clear(void);
int ws2812b_set(int pixn, uint8_t r, uint8_t g, uint8_t b);
void ws2812b_set_rgb(int pixn, uint32_t rgb);
int ws2812b_send(void);
int ws2812b_is_ready(void);

#endif

