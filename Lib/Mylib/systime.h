#ifndef SYSTIME_H
#define SYSTIME_H

#include "common_defs.h"

/*
TIM2 - 32 битный счётчик микросекунд (метки времени gs_usb)
SysTick - счётчик миллисекунд
*/
void systime_init(void);
uint32_t systime_us(void);
uint32_t systime_ms(void);

#endif
