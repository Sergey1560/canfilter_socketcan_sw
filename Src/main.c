#include "main.h"
#include "bootloader.h"
#include "can.h"
#include "led.h"
#include "systime.h"
#include "usb.h"
#include "gs_usb.h"
#include "tusb.h"

int main(void){
  bootloader_check();

  TRACE_START;
  TRACE_ONIDLE;

  RCC_init();

  systime_init();

  led_init();

  can_hw_init();

  usb_init();

  while (1){
    tud_task();
    gs_usb_poll();
    can_poll();
    bootloader_poll();
  }
}
