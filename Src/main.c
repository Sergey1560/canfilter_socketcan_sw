#include "main.h"
#include "can.h"
#include "led.h"

int main(void){
  TRACE_START;
  TRACE_ONIDLE;

  RCC_init();
 
  led_init();

  can_hw_init();

  while (1){
  }
}

