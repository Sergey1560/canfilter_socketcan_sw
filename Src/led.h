#ifndef LED_H
#define LED_H

#include "common_defs.h"


#define LED_COLOR_BR        (uint32_t)8 //якрость 0-255

#define LED_COLOR_OFF       (uint32_t)(0x000000)
#define LED_COLOR_GREEN     (uint32_t)(LED_COLOR_BR << 8)
#define LED_COLOR_RED       (uint32_t)(LED_COLOR_BR << 16)
#define LED_COLOR_YELLOW    (uint32_t)(LED_COLOR_RED|LED_COLOR_GREEN)

enum led_cmd_t{
    LED_CMD_OFF,
    LED_CMD_RED,
    LED_CMD_GREEN,
    LED_CMD_YELLOW,
    LED_CMD_BLINK_RED,
    LED_CMD_BLINK_GREEN,
    LED_CMD_BLINK_YELLOW,
    LED_CMD_END_LIST
};

void led_init(void);
void led_set_mode(enum led_cmd_t cmd);

#endif
