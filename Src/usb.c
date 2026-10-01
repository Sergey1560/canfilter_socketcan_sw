#include <string.h>
#include "usb.h"
#include "gs_usb.h"
#include "led.h"
#include "systime.h"
#include "tusb.h"

enum{
	STRID_LANGID = 0,
	STRID_MANUFACTURER,
	STRID_PRODUCT,
	STRID_SERIAL,
	STRID_INTERFACE,
	STRID_COUNT
};

#define USB_CONFIG_TOTAL_LEN	(TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN)
#define USB_STR_MAX_CHARS		32

static const tusb_desc_device_t desc_device = {
	.bLength			= sizeof(tusb_desc_device_t),
	.bDescriptorType	= TUSB_DESC_DEVICE,
	.bcdUSB				= 0x0200,
	.bDeviceClass		= 0x00,
	.bDeviceSubClass	= 0x00,
	.bDeviceProtocol	= 0x00,
	.bMaxPacketSize0	= CFG_TUD_ENDPOINT0_SIZE,
	.idVendor			= USB_VID,
	.idProduct			= USB_PID,
	.bcdDevice			= USB_BCD_DEVICE,
	.iManufacturer		= STRID_MANUFACTURER,
	.iProduct			= STRID_PRODUCT,
	.iSerialNumber		= STRID_SERIAL,
	.bNumConfigurations	= 1
};

static const uint8_t desc_configuration[] = {
	TUD_CONFIG_DESCRIPTOR(1, 1, 0, USB_CONFIG_TOTAL_LEN, 0x00, USB_MAX_POWER_MA),
	TUD_VENDOR_DESCRIPTOR(GS_USB_ITF_NUM, STRID_INTERFACE, GS_USB_EP_OUT, GS_USB_EP_IN, GS_USB_EP_SIZE),
};

static const char *const string_desc[STRID_COUNT] = {
	[STRID_MANUFACTURER]	= USB_MANUFACTURER_STR,
	[STRID_PRODUCT]			= USB_PRODUCT_STR,
	[STRID_INTERFACE]		= USB_INTERFACE_STR,
};

static uint16_t desc_str[USB_STR_MAX_CHARS + 1];

void usb_init(void){
	/* USB 48MHz от HSI48 с подстройкой CRS по SOF */
	RCC->CRRCR |= RCC_CRRCR_HSI48ON;
	while(!(RCC->CRRCR & RCC_CRRCR_HSI48RDY)){};

	RCC->CCIPR &= ~RCC_CCIPR_CLK48SEL;		//00: HSI48

	RCC->APB1ENR1 |= RCC_APB1ENR1_CRSEN|RCC_APB1ENR1_USBEN;
	(void)RCC->APB1ENR1;

	uint32_t tmp = CRS->CFGR;
	tmp &= ~CRS_CFGR_SYNCSRC;
	tmp |= (2UL << CRS_CFGR_SYNCSRC_Pos);	//USB SOF
	CRS->CFGR = tmp;
	CRS->CR |= CRS_CR_AUTOTRIMEN|CRS_CR_CEN;

	/* Ниже FDCAN (5): прерывание USB только ставит события для tud_task() */
	NVIC_SetPriority(USB_HP_IRQn, 6);
	NVIC_SetPriority(USB_LP_IRQn, 6);

	const tusb_rhport_init_t dev_init = {
		.role = TUSB_ROLE_DEVICE,
		.speed = TUSB_SPEED_FULL
	};

	if(!tusb_init(0, &dev_init)){
		ERROR("TinyUSB init failed");
		return;
	}

	DEBUG("USB init done, %04X:%04X", USB_VID, USB_PID);
}

void USB_HP_IRQHandler(void){
	tud_int_handler(0);
}

void USB_LP_IRQHandler(void){
	tud_int_handler(0);
}

uint32_t tusb_time_millis_api(void){
	return systime_ms();
}

/* Без VBUS sense отключение кабеля видно как suspend */
void tud_mount_cb(void){
	led_usb_connected(true);
}

void tud_umount_cb(void){
	led_usb_connected(false);
}

void tud_suspend_cb(bool UNUSED(remote_wakeup_en)){
	led_usb_connected(false);
}

void tud_resume_cb(void){
	led_usb_connected(tud_mounted());
}

/*********************** Дескрипторы ***********************/

uint8_t const *tud_descriptor_device_cb(void){
	return (uint8_t const *)&desc_device;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t UNUSED(index)){
	return desc_configuration;
}

/* Серийный номер из 96 бит UID */
static size_t usb_serial(uint16_t *dst){
	static const char hex[] = "0123456789ABCDEF";
	const uint32_t uid[3] = {
		*(const uint32_t *)(UID_BASE),
		*(const uint32_t *)(UID_BASE + 4),
		*(const uint32_t *)(UID_BASE + 8)
	};
	size_t n = 0;

	for(int w = 2; w >= 0; w--){
		for(int shift = 28; shift >= 0; shift -= 4){
			dst[n++] = hex[(uid[w] >> shift) & 0x0F];
		}
	}
	return n;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t UNUSED(langid)){
	size_t chr_count;

	if(index == STRID_LANGID){
		desc_str[1] = 0x0409;
		chr_count = 1;
	}else if(index == STRID_SERIAL){
		chr_count = usb_serial(&desc_str[1]);
	}else{
		if(index >= STRID_COUNT || string_desc[index] == NULL){
			return NULL;
		}
		const char *str = string_desc[index];
		chr_count = strlen(str);
		if(chr_count > USB_STR_MAX_CHARS){
			chr_count = USB_STR_MAX_CHARS;
		}
		for(size_t i = 0; i < chr_count; i++){
			desc_str[1 + i] = (uint8_t)str[i];
		}
	}

	desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
	return desc_str;
}
