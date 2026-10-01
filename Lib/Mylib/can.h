#ifndef CAN_INIT_H
#define CAN_INIT_H
#include "common_defs.h"
#include "common_data.h"

#define CAN_DEBUG

#define CAN1_FILTERS

#define 	CAN1_ENABLE_IO	GPIOB->BSRR = GPIO_BSRR_BS7
#define 	CAN1_DISABLE_IO	GPIOB->BSRR = GPIO_BSRR_BR7
#define 	CAN1_SILENT_ON	GPIOC->BSRR = GPIO_BSRR_BS13
#define 	CAN1_SILENT_OFF	GPIOC->BSRR = GPIO_BSRR_BR13

#define 	CAN2_SILENT_ON	GPIOB->BSRR = GPIO_BSRR_BS14
#define 	CAN2_SILENT_OFF	GPIOB->BSRR = GPIO_BSRR_BR14

//https://phryniszak.github.io/stm32g-fdcan/
/* 
Частота 80Mhz, 80 квантов на бит, точка выборки 80%
Для изменения скорости достаточно изменить PSC.
Для 1000Kb/s 1
Для 500Kb/s 2
Для 250Kb/s 4
Для 125Kb/s 8
*/
#define CAN_PSC 2
#define CAN_JW  16
#define CAN_SEG1 63
#define CAN_SEG2 16

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

#define CAN_29BIT_FLAG		(uint32_t)(1 << 30)

enum can_bauderate_list_t{
	CAN_BD_125 = 0,
	CAN_BD_250 = 1,
	CAN_BD_500 = 2,
	CAN_BD_1000 = 3
};

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

struct can_rx_fifo_element_t{
	uint32_t b[18];
};

struct can_tx_fifo_element_t{
	uint32_t b[18];
};

struct can_tx_event_element_t{
	uint32_t b[2];
};

struct can_ext_filter_element_t{
	union
	{
		uint32_t b1_value;
		struct{
			unsigned int efid2:29;
			unsigned int padding:1;
			unsigned int eft:2;
		};
	};
	union
	{
		uint32_t b0_value;
		struct{
			unsigned int efid1:29;
			unsigned int efec:3;
		};
	};

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
	struct fdcan_mem_t fdcan1;
	struct fdcan_mem_t fdcan2;
	struct fdcan_mem_t fdcan3;
};

void can_hw_init(void);
int can_send(FDCAN_GlobalTypeDef *CAN, struct can_message_t *msg);

#endif
