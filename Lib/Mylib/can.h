#ifndef CAN_INIT_H
#define CAN_INIT_H
#include <stdbool.h>
#include "common_defs.h"

#define CAN_DEBUG

/*
FDCAN1: PB8 RX, PB9 TX, PC13 S, PB7 VIO трансивера
FDCAN2: PB12 RX, PB13 TX, PB14 S
FDCAN3: PB3 RX, PA15 TX, PB4 S
*/
#define 	CAN1_ENABLE_IO	GPIOB->BSRR = GPIO_BSRR_BS7
#define 	CAN1_DISABLE_IO	GPIOB->BSRR = GPIO_BSRR_BR7

enum can_channel_t{
	CAN_CH1 = 0,
	CAN_CH2,
	CAN_CH3,
	CAN_CH_COUNT
};

/*
https://phryniszak.github.io/stm32g-fdcan/
Частота FDCAN 80Mhz, по умолчанию 500Kb/s:
80 квантов на бит, точка выборки 80%
*/
#define CAN_DEFAULT_BRP		2
#define CAN_DEFAULT_TSEG1	63
#define CAN_DEFAULT_TSEG2	16
#define CAN_DEFAULT_SJW		16

/* Пауза в bus-off перед восстановлением (аналог restart-ms в Linux) */
#define CAN_BUSOFF_RESTART_MS	100

/* Ограничения NBTP / DBTP */
#define CAN_NBT_TSEG1_MIN	2
#define CAN_NBT_TSEG1_MAX	256
#define CAN_NBT_TSEG2_MIN	2
#define CAN_NBT_TSEG2_MAX	128
#define CAN_NBT_SJW_MAX		128
#define CAN_NBT_BRP_MIN		1
#define CAN_NBT_BRP_MAX		512

#define CAN_DBT_TSEG1_MIN	1
#define CAN_DBT_TSEG1_MAX	32
#define CAN_DBT_TSEG2_MIN	1
#define CAN_DBT_TSEG2_MAX	16
#define CAN_DBT_SJW_MAX		16
#define CAN_DBT_BRP_MIN		1
#define CAN_DBT_BRP_MAX		32

/* Формат идентификатора как в Linux (struct can_frame.can_id) */
#define CAN_ID_EFF			(1UL << 31)
#define CAN_ID_RTR			(1UL << 30)
#define CAN_ID_ERR			(1UL << 29)
#define CAN_ID_SFF_MASK		0x000007FFUL
#define CAN_ID_EFF_MASK		0x1FFFFFFFUL

/* can_frame_t.flags */
#define CAN_FLAG_FD			(1U << 0)
#define CAN_FLAG_BRS		(1U << 1)
#define CAN_FLAG_ESI		(1U << 2)

/* Режимы для can_start() */
#define CAN_MODE_LISTEN_ONLY	(1U << 0)
#define CAN_MODE_LOOPBACK		(1U << 1)	//Внутренняя петля, на шину ничего не уходит
#define CAN_MODE_ONE_SHOT		(1U << 2)
#define CAN_MODE_FD				(1U << 3)
#define CAN_MODE_BERR			(1U << 4)	//Прерывания по ошибкам протокола

enum can_state_t{
	CAN_STATE_ACTIVE = 0,
	CAN_STATE_WARNING,
	CAN_STATE_PASSIVE,
	CAN_STATE_BUS_OFF,
	CAN_STATE_STOPPED
};

/* PSR.LEC / PSR.DLEC */
enum can_lec_t{
	CAN_LEC_NONE = 0,
	CAN_LEC_STUFF,
	CAN_LEC_FORM,
	CAN_LEC_ACK,
	CAN_LEC_BIT1,
	CAN_LEC_BIT0,
	CAN_LEC_CRC,
	CAN_LEC_NO_CHANGE
};

struct can_frame_t{
	uint32_t id;		//CAN_ID_EFF/CAN_ID_RTR + 11/29 бит
	uint8_t dlc;		//Код DLC 0..15
	uint8_t flags;		//CAN_FLAG_*
	uint8_t data[64] ALGN4;
};

struct can_timing_t{
	uint16_t brp;
	uint16_t tseg1;		//prop_seg + phase_seg1
	uint16_t tseg2;
	uint16_t sjw;
};

/*********************** Message RAM ***********************/

#define CAN_STD_FILTER_SFT_RANGE				0
#define CAN_STD_FILTER_SFT_DUAL					1
#define CAN_STD_FILTER_SFT_CLASSIC				2
#define CAN_STD_FILTER_SFT_DISABLED				3

#define CAN_STD_FILTER_SFEC_DISABLE				0
#define CAN_STD_FILTER_SFEC_FIFO0				1
#define CAN_STD_FILTER_SFEC_FIFO1				2
#define CAN_STD_FILTER_SFEC_REJECT				3
#define CAN_STD_FILTER_SFEC_SET_PRI				4
#define CAN_STD_FILTER_SFEC_SET_PRI_FIFO0		5
#define CAN_STD_FILTER_SFEC_SET_PRI_FIFO1		6

/* Поля элементов RX/TX буферов */
#define CAN_ELEM_ESI		(1UL << 31)
#define CAN_ELEM_XTD		(1UL << 30)
#define CAN_ELEM_RTR		(1UL << 29)
#define CAN_ELEM_MM_Pos		24
#define CAN_ELEM_EFC		(1UL << 23)
#define CAN_ELEM_FDF		(1UL << 21)
#define CAN_ELEM_BRS		(1UL << 20)
#define CAN_ELEM_DLC_Pos	16

struct std_filter_t{
	union
	{
		uint32_t value;
		struct{
			unsigned int sfid2:11;
			unsigned int padding:5;
			unsigned int sfid1:11;
			unsigned int sfec:3;
			unsigned int sft:2;
		};
	};
};

struct can_ext_filter_element_t{
	union
	{
		uint32_t f0_value;
		struct{
			unsigned int efid1:29;
			unsigned int efec:3;
		};
	};
	union
	{
		uint32_t f1_value;
		struct{
			unsigned int efid2:29;
			unsigned int padding:1;
			unsigned int eft:2;
		};
	};
};

struct can_rx_fifo_element_t{
	uint32_t b[18];
};

struct can_tx_fifo_element_t{
	uint32_t b[18];
};

struct can_tx_event_element_t{
	uint32_t b[2];
};

struct fdcan_mem_t {
	uint32_t std_filter[28];
	struct can_ext_filter_element_t ext_filter[8];
	struct can_rx_fifo_element_t rx0_fifo[3];
	struct can_rx_fifo_element_t rx1_fifo[3];
	struct can_tx_event_element_t tx_event[3];
	struct can_tx_fifo_element_t can_tx_fifo[3];
};

struct fdcan_sram_t {
	struct fdcan_mem_t fdcan[CAN_CH_COUNT];
};

/*********************** API ***********************/

void can_hw_init(void);

bool can_set_timing(uint8_t ch, const struct can_timing_t *timing);
bool can_set_data_timing(uint8_t ch, const struct can_timing_t *timing);
bool can_start(uint8_t ch, uint32_t mode);
void can_stop(uint8_t ch);

/* Основной цикл: восстановление после bus-off по истечении CAN_BUSOFF_RESTART_MS */
void can_poll(void);

/* 0 - кадр поставлен в TX FIFO, -1 - FIFO занят или канал остановлен.
marker возвращается в can_tx_done_cb() */
int can_tx(uint8_t ch, const struct can_frame_t *frame, uint8_t marker);

enum can_state_t can_get_state(uint8_t ch, uint8_t *tec, uint8_t *rec);

uint8_t can_dlc2len(uint8_t dlc, bool fd);

/*
Колбэки вызываются из прерываний FDCANx_IT0 (одинаковый приоритет,
друг друга не вытесняют). Слабые реализации по умолчанию пустые.
*/
void can_rx_cb(uint8_t ch, const struct can_frame_t *frame);
void can_rx_lost_cb(uint8_t ch);
void can_tx_done_cb(uint8_t ch, uint8_t marker, bool sent);
void can_state_cb(uint8_t ch, enum can_state_t state, enum can_state_t prev, uint8_t tec, uint8_t rec);
void can_bus_error_cb(uint8_t ch, enum can_lec_t lec, bool data_phase, uint8_t tec, uint8_t rec);

#endif
