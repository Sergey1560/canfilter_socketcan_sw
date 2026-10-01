#include <stdbool.h>
#include "bootloader.h"
#include "systime.h"
#include "led.h"
#include "tusb.h"

#define BOOTLOADER_MAGIC			0xB007DF11UL
#define BOOTLOADER_SYSMEM_BASE		0x1FFF0000UL

#define BOOTLOADER_DETACH_DELAY_MS		20		//Завершение control transfer DETACH
#define BOOTLOADER_DISCONNECT_MS		100		//Хост должен увидеть отключение

enum bootloader_state_t{
	BL_IDLE = 0,
	BL_REQUESTED,
	BL_DISCONNECTED
};

__attribute__((section(".noinit"))) static volatile uint32_t boot_magic;

static volatile enum bootloader_state_t bl_state = BL_IDLE;
static uint32_t bl_time;

void bootloader_check(void){
	if(boot_magic != BOOTLOADER_MAGIC){
		return;
	}
	boot_magic = 0;

	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
	(void)RCC->APB2ENR;

	//Системная память по адресу 0
	SYSCFG->MEMRMP = (SYSCFG->MEMRMP & ~SYSCFG_MEMRMP_MEM_MODE) | SYSCFG_MEMRMP_MEM_MODE_0;
	SCB->VTOR = BOOTLOADER_SYSMEM_BASE;

	const uint32_t sp = *(volatile const uint32_t *)BOOTLOADER_SYSMEM_BASE;
	const uint32_t pc = *(volatile const uint32_t *)(BOOTLOADER_SYSMEM_BASE + 4);

	__DSB();
	__ISB();
	__asm volatile(
		"msr msp, %0\n"
		"bx %1\n"
		:: "r"(sp), "r"(pc) : "memory");

	while(1){};
}

void bootloader_request(void){
	if(bl_state == BL_IDLE){
		bl_time = systime_ms();
		bl_state = BL_REQUESTED;
	}
}

void bootloader_poll(void){
	switch(bl_state){
	case BL_REQUESTED:
		if(systime_ms() - bl_time >= BOOTLOADER_DETACH_DELAY_MS){
			INFO("Reboot to system bootloader");
			led_bootloader();
			tud_disconnect();
			bl_time = systime_ms();
			bl_state = BL_DISCONNECTED;
		}
		break;

	case BL_DISCONNECTED:
		if(systime_ms() - bl_time >= BOOTLOADER_DISCONNECT_MS){
			boot_magic = BOOTLOADER_MAGIC;
			__DSB();
			NVIC_SystemReset();
		}
		break;

	default:
		break;
	}
}

/* DFU_DETACH от dfu-util */
void tud_dfu_runtime_reboot_to_dfu_cb(void){
	bootloader_request();
}
