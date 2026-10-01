#ifndef LED_H
#define LED_H

#include <stdbool.h>
#include "common_defs.h"


#define LED_COLOR_BR        (uint32_t)8 //якрость 0-255

#define LED_COLOR_OFF       (uint32_t)(0x000000)
#define LED_COLOR_BLUE      (uint32_t)(LED_COLOR_BR)
#define LED_COLOR_GREEN     (uint32_t)(LED_COLOR_BR << 8)
#define LED_COLOR_RED       (uint32_t)(LED_COLOR_BR << 16)
#define LED_COLOR_YELLOW    (uint32_t)(LED_COLOR_RED|LED_COLOR_GREEN)

/*
Индикация канала:
  OFF     - синий, интерфейс опущен
  OK      - зелёный, error active
  WARNING - жёлтый, error warning / error passive
  ERROR   - красный, bus-off
Приём/передача кадра коротко гасит светодиод.
Нет USB хоста - все светодиоды медленно мигают синим.
*/
enum led_ch_state_t{
    LED_CH_OFF = 0,
    LED_CH_OK,
    LED_CH_WARNING,
    LED_CH_ERROR
};

void led_init(void);

/* Можно вызывать из прерываний */
void led_channel_state(uint8_t ch, enum led_ch_state_t state);
void led_channel_activity(uint8_t ch);
void led_usb_connected(bool connected);

/* Мигание светодиодом канала жёлтым (ethtool -p canX) */
void led_identify(uint8_t ch, bool on);

#endif
