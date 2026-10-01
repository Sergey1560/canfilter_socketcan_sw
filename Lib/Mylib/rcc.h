#ifndef RCC_H
#define RCC_H
#include "common_defs.h"

/*
HSE 8MHz / M1 * N40 = VCO 320MHz
SYSCLK = VCO / R2 = 160MHz
FDCAN  = VCO / Q4 = 80MHz (удобно для CAN FD: 1/2/4/5/8 Mbit/s без дробных квантов)
*/
#define HSE_FREQ (8000000UL)

#define PLL_M   1
#define PLL_N   40
#define PLL_R   0  //2
#define PLL_Q   1  //4
#define PLL_P   2

#define PLL_VCO_FREQ    (HSE_FREQ / PLL_M * PLL_N)
#define SYSCLK_FREQ     (PLL_VCO_FREQ / (2 * (PLL_R + 1)))
#define FDCAN_CLK_FREQ  (PLL_VCO_FREQ / (2 * (PLL_Q + 1)))



void RCC_init(void);


#endif
