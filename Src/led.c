#include "led.h"
#include "rcc.h"
#include "ws2812b.h"

#define LED_COUNT               3
#define LED_TICK_HZ             50

#define LED_ACT_OFF_TICKS       2       //Гашение при активности, 40 мс
#define LED_ACT_ON_TICKS        3       //Минимум горит между гашениями, 60 мс
#define LED_IDENTIFY_TICKS      12      //Полупериод мигания identify, ~2 Гц
#define LED_NO_USB_TICKS        25      //Полупериод мигания без USB, 1 Гц

/*
Цепочка WS2812: PB6 -> D102 (0) -> D104 (1) -> D106 (2).
На плате слева направо: D106, D104, D102.
Канал -> индекс в цепочке, CAN1 крайний левый.
*/
static const uint8_t led_map[LED_COUNT] = {2, 1, 0};

struct led_ch_t{
    volatile enum led_ch_state_t state;
    volatile bool activity;
    volatile bool identify;
    uint8_t act_ticks;
    bool act_off;
};

static struct led_ch_t led_ch[LED_COUNT];
static volatile bool usb_connected = false;

static void led_update_timer_init(void);
static uint32_t led_channel_color(struct led_ch_t *c);

void led_init(void){
    ws2812b_init();
    //Start update timer
    led_update_timer_init();
}

 static void led_update_timer_init(void){

    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM6EN;
    /*
    TIM_CLK SYSCLK_FREQ (APB1 без делителя)
    Timer UI = 10000
    */
	TIM6->PSC = SYSCLK_FREQ/10000 - 1;
	TIM6->ARR = 10000 / LED_TICK_HZ - 1;
	TIM6->CNT=0;
	TIM6->DIER |= TIM_DIER_UIE;
	TIM6->CR1 |= TIM_CR1_CEN;
	NVIC_SetPriority(TIM6_DAC_IRQn, 7);
	NVIC_EnableIRQ(TIM6_DAC_IRQn);
 }

static uint32_t led_channel_color(struct led_ch_t *c){
    uint32_t color;

    switch(c->state){
    case LED_CH_OK:
        color = LED_COLOR_GREEN;
        break;
    case LED_CH_WARNING:
        color = LED_COLOR_YELLOW;
        break;
    case LED_CH_ERROR:
        color = LED_COLOR_RED;
        break;
    default:
        c->activity = false;
        c->act_off = false;
        c->act_ticks = 0;
        return LED_COLOR_BLUE;
    }

    if(c->act_ticks > 0){
        c->act_ticks--;
    }
    if(c->act_ticks == 0){
        if(c->act_off){
            c->act_off = false;
            c->act_ticks = LED_ACT_ON_TICKS;
        }else if(c->activity){
            c->activity = false;
            c->act_off = true;
            c->act_ticks = LED_ACT_OFF_TICKS;
        }
    }

    return c->act_off ? LED_COLOR_OFF : color;
}

 void TIM6_DAC_IRQHandler(void){
    static uint32_t tick = 0;

    if(TIM6->SR & TIM_SR_UIF) {
		TIM6->SR &= ~TIM_SR_UIF;

        tick++;
        const bool identify_on = ((tick / LED_IDENTIFY_TICKS) & 1) == 0;
        const bool no_usb_on = ((tick / LED_NO_USB_TICKS) & 1) == 0;

        for(uint8_t ch = 0; ch < LED_COUNT; ch++){
            struct led_ch_t *c = &led_ch[ch];
            uint32_t color;

            if(c->identify){
                color = identify_on ? LED_COLOR_YELLOW : LED_COLOR_OFF;
            }else if(!usb_connected){
                color = no_usb_on ? LED_COLOR_BLUE : LED_COLOR_OFF;
            }else{
                color = led_channel_color(c);
            }
            ws2812b_set_rgb(led_map[ch], color);
        }

        ws2812b_send();
    }
 }

void led_channel_state(uint8_t ch, enum led_ch_state_t state){
    if(ch < LED_COUNT){
        led_ch[ch].state = state;
    }
}

void led_channel_activity(uint8_t ch){
    if(ch < LED_COUNT){
        led_ch[ch].activity = true;
    }
}

void led_usb_connected(bool connected){
    usb_connected = connected;
}

void led_identify(uint8_t ch, bool on){
    if(ch < LED_COUNT){
        led_ch[ch].identify = on;
    }
}

void led_bootloader(void){
    NVIC_DisableIRQ(TIM6_DAC_IRQn);
    TIM6->CR1 &= ~TIM_CR1_CEN;

    for(uint8_t ch = 0; ch < LED_COUNT; ch++){
        ws2812b_set_rgb(led_map[ch], LED_COLOR_PURPLE);
    }
    ws2812b_send();
}
