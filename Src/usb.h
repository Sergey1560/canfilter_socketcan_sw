#ifndef USB_H
#define USB_H

#include "common_defs.h"

/* VID:PID candleLight, драйвер gs_usb привязывается автоматически */
#define USB_VID					0x1D50
#define USB_PID					0x606F
#define USB_BCD_DEVICE			0x0100

#define USB_MANUFACTURER_STR	"CanFilter"
#define USB_PRODUCT_STR			"CanFilter G4 gs_usb"
#define USB_INTERFACE_STR		"gs_usb"
#define USB_DFU_STR				"CanFilter DFU"

/* 3 трансивера TJA1051 (до 70 мА в доминантном состоянии) + МК + светодиоды */
#define USB_MAX_POWER_MA		250

void usb_init(void);

#endif
