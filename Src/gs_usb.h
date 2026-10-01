#ifndef GS_USB_H
#define GS_USB_H

#include <stdbool.h>
#include "common_defs.h"

/*
Протокол gs_usb (candleLight), см. linux drivers/net/can/usb/gs_usb.c
Все поля little-endian.
*/

#define GS_USB_ITF_NUM			0
#define GS_USB_EP_IN			0x81
#define GS_USB_EP_OUT			0x02
#define GS_USB_EP_SIZE			64

#define GS_USB_SW_VERSION		2
#define GS_USB_HW_VERSION		1

/* Максимум кадров "в полёте" на канал у драйвера Linux (GS_MAX_TX_URBS) */
#define GS_USB_HOST_MAX_TX		10

enum gs_usb_breq{
	GS_USB_BREQ_HOST_FORMAT = 0,
	GS_USB_BREQ_BITTIMING,
	GS_USB_BREQ_MODE,
	GS_USB_BREQ_BERR,
	GS_USB_BREQ_BT_CONST,
	GS_USB_BREQ_DEVICE_CONFIG,
	GS_USB_BREQ_TIMESTAMP,
	GS_USB_BREQ_IDENTIFY,
	GS_USB_BREQ_GET_USER_ID,
	GS_USB_BREQ_SET_USER_ID,
	GS_USB_BREQ_DATA_BITTIMING,
	GS_USB_BREQ_BT_CONST_EXT,
	GS_USB_BREQ_SET_TERMINATION,
	GS_USB_BREQ_GET_TERMINATION,
	GS_USB_BREQ_GET_STATE,
};

enum gs_can_mode{
	GS_CAN_MODE_RESET = 0,
	GS_CAN_MODE_START
};

enum gs_can_state{
	GS_CAN_STATE_ERROR_ACTIVE = 0,
	GS_CAN_STATE_ERROR_WARNING,
	GS_CAN_STATE_ERROR_PASSIVE,
	GS_CAN_STATE_BUS_OFF,
	GS_CAN_STATE_STOPPED,
	GS_CAN_STATE_SLEEPING
};

/* gs_device_mode.flags */
#define GS_CAN_MODE_NORMAL					0
#define GS_CAN_MODE_LISTEN_ONLY				(1UL << 0)
#define GS_CAN_MODE_LOOP_BACK				(1UL << 1)
#define GS_CAN_MODE_TRIPLE_SAMPLE			(1UL << 2)
#define GS_CAN_MODE_ONE_SHOT				(1UL << 3)
#define GS_CAN_MODE_HW_TIMESTAMP			(1UL << 4)
#define GS_CAN_MODE_PAD_PKTS_TO_MAX_PKT_SIZE (1UL << 7)
#define GS_CAN_MODE_FD						(1UL << 8)
#define GS_CAN_MODE_BERR_REPORTING			(1UL << 12)

/* gs_device_bt_const.feature */
#define GS_CAN_FEATURE_LISTEN_ONLY			(1UL << 0)
#define GS_CAN_FEATURE_LOOP_BACK			(1UL << 1)
#define GS_CAN_FEATURE_TRIPLE_SAMPLE		(1UL << 2)
#define GS_CAN_FEATURE_ONE_SHOT				(1UL << 3)
#define GS_CAN_FEATURE_HW_TIMESTAMP			(1UL << 4)
#define GS_CAN_FEATURE_IDENTIFY				(1UL << 5)
#define GS_CAN_FEATURE_USER_ID				(1UL << 6)
#define GS_CAN_FEATURE_PAD_PKTS_TO_MAX_PKT_SIZE (1UL << 7)
#define GS_CAN_FEATURE_FD					(1UL << 8)
#define GS_CAN_FEATURE_REQ_USB_QUIRK_LPC546XX (1UL << 9)
#define GS_CAN_FEATURE_BT_CONST_EXT			(1UL << 10)
#define GS_CAN_FEATURE_TERMINATION			(1UL << 11)
#define GS_CAN_FEATURE_BERR_REPORTING		(1UL << 12)
#define GS_CAN_FEATURE_GET_STATE			(1UL << 13)

/* gs_host_frame.flags */
#define GS_CAN_FLAG_OVERFLOW				(1U << 0)
#define GS_CAN_FLAG_FD						(1U << 1)
#define GS_CAN_FLAG_BRS						(1U << 2)
#define GS_CAN_FLAG_ESI						(1U << 3)

#define GS_HOST_FRAME_ECHO_ID_RX			0xFFFFFFFFUL

/* linux/can/error.h */
#define CAN_ERR_DLC							8
#define CAN_ERR_TX_TIMEOUT					0x00000001UL
#define CAN_ERR_LOSTARB						0x00000002UL
#define CAN_ERR_CRTL						0x00000004UL
#define CAN_ERR_PROT						0x00000008UL
#define CAN_ERR_TRX							0x00000010UL
#define CAN_ERR_ACK							0x00000020UL
#define CAN_ERR_BUSOFF						0x00000040UL
#define CAN_ERR_BUSERROR					0x00000080UL
#define CAN_ERR_RESTARTED					0x00000100UL
#define CAN_ERR_CNT							0x00000200UL

#define CAN_ERR_CRTL_RX_OVERFLOW			0x01
#define CAN_ERR_CRTL_RX_WARNING				0x04
#define CAN_ERR_CRTL_TX_WARNING				0x08
#define CAN_ERR_CRTL_RX_PASSIVE				0x10
#define CAN_ERR_CRTL_TX_PASSIVE				0x20
#define CAN_ERR_CRTL_ACTIVE					0x40

#define CAN_ERR_PROT_FORM					0x02
#define CAN_ERR_PROT_STUFF					0x04
#define CAN_ERR_PROT_BIT0					0x08
#define CAN_ERR_PROT_BIT1					0x10

#define CAN_ERR_PROT_LOC_CRC_SEQ			0x08
#define CAN_ERR_PROT_LOC_ACK				0x19

struct gs_host_config{
	uint32_t byte_order;
} __attribute__((packed));

struct gs_device_config{
	uint8_t reserved1;
	uint8_t reserved2;
	uint8_t reserved3;
	uint8_t icount;			//Число каналов - 1
	uint32_t sw_version;
	uint32_t hw_version;
} __attribute__((packed));

struct gs_device_mode{
	uint32_t mode;
	uint32_t flags;
} __attribute__((packed));

struct gs_device_state{
	uint32_t state;
	uint32_t rxerr;
	uint32_t txerr;
} __attribute__((packed));

struct gs_device_bittiming{
	uint32_t prop_seg;
	uint32_t phase_seg1;
	uint32_t phase_seg2;
	uint32_t sjw;
	uint32_t brp;
} __attribute__((packed));

struct gs_identify_mode{
	uint32_t mode;
} __attribute__((packed));

struct gs_device_bt_const{
	uint32_t feature;
	uint32_t fclk_can;
	uint32_t tseg1_min;
	uint32_t tseg1_max;
	uint32_t tseg2_min;
	uint32_t tseg2_max;
	uint32_t sjw_max;
	uint32_t brp_min;
	uint32_t brp_max;
	uint32_t brp_inc;
} __attribute__((packed));

struct gs_device_bt_const_extended{
	uint32_t feature;
	uint32_t fclk_can;
	uint32_t tseg1_min;
	uint32_t tseg1_max;
	uint32_t tseg2_min;
	uint32_t tseg2_max;
	uint32_t sjw_max;
	uint32_t brp_min;
	uint32_t brp_max;
	uint32_t brp_inc;

	uint32_t dtseg1_min;
	uint32_t dtseg1_max;
	uint32_t dtseg2_min;
	uint32_t dtseg2_max;
	uint32_t dsjw_max;
	uint32_t dbrp_min;
	uint32_t dbrp_max;
	uint32_t dbrp_inc;
} __attribute__((packed));

#define GS_HOST_FRAME_HDR_SIZE		12
#define GS_HOST_FRAME_CLASSIC_DATA	8
#define GS_HOST_FRAME_FD_DATA		64
#define GS_HOST_FRAME_TS_SIZE		4

struct gs_host_frame{
	uint32_t echo_id;
	uint32_t can_id;
	uint8_t can_dlc;
	uint8_t channel;
	uint8_t flags;
	uint8_t reserved;
	/* data[8] | data[8]+timestamp | data[64] | data[64]+timestamp */
	uint8_t data[GS_HOST_FRAME_FD_DATA + GS_HOST_FRAME_TS_SIZE];
} __attribute__((packed, aligned(4)));

void gs_usb_poll(void);

#endif
