#include "can.h"
#include "common_data.h"

const struct fdcan_sram_t *fdcan_sram = (const struct fdcan_sram_t *)(APB1PERIPH_BASE + 0xA400UL);

static void get_msg(FDCAN_GlobalTypeDef *CAN, struct can_message_t* msg, uint8_t idx);
static const struct fdcan_mem_t *get_mem_base_addr(FDCAN_GlobalTypeDef *CAN);
static void fdcan_init(FDCAN_GlobalTypeDef *CAN, uint8_t can_bauderate);

static void fdcan1_filters(void);

void can_hw_init(void){
/*
FDCAN1:
PB7 - CAN1 IO
PB8 - RX
PB9 - TX
PC13 - Silent


FDCAN2:
PB12 - RX
PB13 - TX
PB14 - Silent
*/
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN|RCC_AHB2ENR_GPIOCEN;
	
	/*********************************** FDCAN1 *****************************/
	/* PB8 RX, PB9 TX*/
	GPIOB->MODER &= ~(GPIO_MODER_MODER8|GPIO_MODER_MODER9);
    GPIOB->MODER |= (GPIO_MODER_MODER8_1|GPIO_MODER_MODER9_1);
	//Push-pull mode 0
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT_8|GPIO_OTYPER_OT_9);
	
    GPIOB->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR8|GPIO_OSPEEDER_OSPEEDR9);
    GPIOB->OSPEEDR |= (S_VH << GPIO_OSPEEDR_OSPEED8_Pos)|(S_VH << GPIO_OSPEEDR_OSPEED9_Pos);
	
	GPIOB->AFR[1] &= ~(GPIO_AFRH_AFSEL8|GPIO_AFRH_AFSEL9);
	GPIOB->AFR[1] |= (9 << GPIO_AFRH_AFSEL8_Pos)|(9 << GPIO_AFRH_AFSEL9_Pos);
	
	//IO PB7
	GPIOB->MODER &= ~(GPIO_MODER_MODE7);
	GPIOB->MODER |= (GPIO_MODER_MODE7_0);
	CAN1_DISABLE_IO;
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT7);
	GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD7);

	//Silent PC13
	GPIOC->MODER &= ~(GPIO_MODER_MODE13);
	GPIOC->MODER |= (GPIO_MODER_MODE13_0);
	CAN1_SILENT_ON;
	GPIOC->OTYPER &= ~(GPIO_OTYPER_OT13);
	GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPD13);
	GPIOC->PUPDR |= (GPIO_PUPDR_PUPD13_1);

	/*********************************** FDCAN2 *****************************/
	/* PB12 RX, PB13 TX*/
	GPIOB->MODER &= ~(GPIO_MODER_MODER12|GPIO_MODER_MODER13);
    GPIOB->MODER |= (GPIO_MODER_MODER12_1|GPIO_MODER_MODER13_1);
	//Push-pull mode 0
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT_12|GPIO_OTYPER_OT_13);
	
    GPIOB->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR12|GPIO_OSPEEDER_OSPEEDR13);
    GPIOB->OSPEEDR |= (S_VH << GPIO_OSPEEDR_OSPEED12_Pos)|(S_VH << GPIO_OSPEEDR_OSPEED13_Pos);
	
	GPIOB->AFR[1] &= ~(GPIO_AFRH_AFSEL12|GPIO_AFRH_AFSEL13);
	GPIOB->AFR[1] |= (9 << GPIO_AFRH_AFSEL12_Pos)|(9 << GPIO_AFRH_AFSEL13_Pos);
	
	//Silent PB14
	GPIOB->MODER &= ~(GPIO_MODER_MODE14);
	GPIOB->MODER |= (GPIO_MODER_MODE14_0);
	CAN2_SILENT_ON;
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT14);
	GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD14);
	GPIOB->PUPDR |= (GPIO_PUPDR_PUPD14_1);

	RCC->CCIPR &= ~RCC_CCIPR_FDCANSEL;
	//PLLQ, 80Mhz
	RCC->CCIPR |= (1 << RCC_CCIPR_FDCANSEL_Pos);

	RCC->APB1ENR1 |= RCC_APB1ENR1_FDCANEN;

	fdcan_init(FDCAN1,CAN_BD_500);
	fdcan_init(FDCAN2,CAN_BD_500);

	DEBUG("Can init done ");
}

static void fdcan_init(FDCAN_GlobalTypeDef *CAN, uint8_t can_bauderate){
	uint32_t can_psc = 0;

	DEBUG("Init FDCAN at 0x%0X",(uint32_t)CAN);

	const struct fdcan_mem_t *fdcan_mem = get_mem_base_addr(CAN);

	if(fdcan_mem == NULL){
		ERROR("Failed FDCAN mem addr");
		return;
	}

    CAN->CCCR |= FDCAN_CCCR_INIT;     //Initialize the CAN module
	while(!(CAN->CCCR & FDCAN_CCCR_INIT)){__NOP();};         // wait 
    
	CAN->CCCR   |= FDCAN_CCCR_CCE;     //CCE bit enabled to start configuration
    CAN->CCCR &= ~ (FDCAN_CCCR_FDOE);  //Classic CAN

	// /* Clean-up memory */
	for(uint32_t i=(uint32_t)fdcan_mem; i < (uint32_t)fdcan_mem + sizeof(struct fdcan_mem_t); i++){
		*(uint32_t *)i=0;
	}

	switch (can_bauderate)
	{
	case CAN_BD_125:
		can_psc = 8;
		break;
	case CAN_BD_250:
		can_psc = 4;
		break;
	case CAN_BD_500:
		can_psc = 2;
		break;
	case CAN_BD_1000:
		can_psc = 1;
		break;
	
	default:
		ERROR("Unknown CAN BUDRATE value, set 500k");
		can_psc = 2;
		break;
	}

	/* Timings */
	CAN->NBTP = ((CAN_JW-1) << FDCAN_NBTP_NSJW_Pos)|\
				((can_psc-1) << FDCAN_NBTP_NBRP_Pos)|\
				((CAN_SEG1-1) << FDCAN_NBTP_NTSEG1_Pos)|\
				((CAN_SEG2-1) << FDCAN_NBTP_NTSEG2_Pos);


	if(CAN == FDCAN1){
		fdcan1_filters();
	}

	/* TX FIFO mode */
	CAN->TXBC  = 0;
					
	CAN->IE |= FDCAN_IE_RF0NE|FDCAN_IE_RF0FE|FDCAN_IE_BOE;
	CAN->ILE |= FDCAN_ILE_EINT0;

	CAN->CCCR   &= ~(FDCAN_CCCR_INIT);
	
	if(CAN == FDCAN1){
		CAN1_ENABLE_IO;
		CAN1_SILENT_OFF;
		NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
		NVIC_SetPriority(FDCAN1_IT0_IRQn,5);
	}else if(CAN == FDCAN2){
		CAN2_SILENT_OFF;
		NVIC_EnableIRQ(FDCAN2_IT0_IRQn);
		NVIC_SetPriority(FDCAN2_IT0_IRQn,5);
	}else if(CAN == FDCAN3){
		NVIC_EnableIRQ(FDCAN3_IT0_IRQn);
		NVIC_SetPriority(FDCAN3_IT0_IRQn,5);
	}else{
		ERROR("Unknow FDCAN interface 0x%0X",(uint32_t)CAN);
	}
}

static const struct fdcan_mem_t *get_mem_base_addr(FDCAN_GlobalTypeDef *CAN){
	if(CAN == FDCAN1){
		return &fdcan_sram->fdcan1;
	}else if(CAN == FDCAN2){
		return &fdcan_sram->fdcan2;
	}else if(CAN == FDCAN3){
		return &fdcan_sram->fdcan3;
	}else{
		ERROR("Unknow FDCAN interface 0x%0X",(uint32_t)CAN);
		return NULL;
	}
}

static void get_msg(FDCAN_GlobalTypeDef *CAN, struct can_message_t* msg, uint8_t idx){
	const struct fdcan_mem_t *fdcan_mem = get_mem_base_addr(CAN);

	if(fdcan_mem == NULL){
		ERROR("Failed FDCAN mem addr");
		return;
	}

	if(idx > 2){
		ERROR("Index out ouf buffer");
		return;
	}

	struct can_rx_fifo_element_t *fifo = (struct can_rx_fifo_element_t *)&fdcan_mem->rx0_fifo[idx];

	if(fifo->b[0] & CAN_29BIT_FLAG){//29bit ID
		msg->ext_id = 1;
		msg->id = fifo->b[0] & 0x1FFFFFFF;
	}else{
		msg->ext_id = 0;
		msg->id  = (fifo->b[0] >> 18) & 0x7FF;
	}
	msg->padding = 0;
	msg->len = (fifo->b[1] >> 16) & 0xF;

	uint32_t *ptr = (uint32_t *)msg->msg;
	
	*ptr++ = fifo->b[2];
	*ptr = fifo->b[3];
}

int can_send(FDCAN_GlobalTypeDef *CAN, struct can_message_t *msg){
	const struct fdcan_mem_t *fdcan_mem = get_mem_base_addr(CAN);

	if(fdcan_mem == NULL){
		ERROR("Failed FDCAN mem addr");
		return -1;
	}

	#ifdef CAN_DEBUG
	//DEBUG("Count %d TFGI: %d TFQPI: %d TFFL: %d TFQF: %d",msg->msg[0],(CAN->TXFQS >> FDCAN_TXFQS_TFGI_Pos) & 0x3,(CAN->TXFQS >> FDCAN_TXFQS_TFQPI_Pos) & 0x3, CAN->TXFQS & 0x03,(CAN->TXFQS >> FDCAN_TXFQS_TFQF_Pos) & 0x1);
	#endif

	if ((CAN->TXFQS & FDCAN_TXFQS_TFQF) != 0) {
		#ifdef CAN_DEBUG
		ERROR("TX FIFO full");
		#endif
		return -1;
	};

	uint8_t tx_index= (CAN->TXFQS >> FDCAN_TXFQS_TFQPI_Pos) & 0x3;

	if(tx_index > 2){
		ERROR("TX index out of buffer");
		return -1;
	}

	struct can_tx_fifo_element_t *fifo = (struct can_tx_fifo_element_t *)&fdcan_mem->can_tx_fifo[tx_index];

	if(msg->ext_id == 1){//29bit
		fifo->b[0] = CAN_29BIT_FLAG|(msg->id & 0x1FFFFFFF);
	}else{//11bit
		fifo->b[0] = ((msg->id & 0x7ff) << 18);
	}

	fifo->b[1] = ((msg->len & 0xF) << 16);  //Data size

	uint32_t *ptr = (uint32_t *)msg->msg;

	fifo->b[2] = *ptr++;
	fifo->b[3] = *ptr;

	CAN->TXBAR |= (1 << tx_index);
	return 0;   
}

static void fdcan1_filters(void){

	#ifdef CAN1_FILTERS

	struct std_filter_t filter_element = {0};
	uint32_t *filter = (uint32_t *)&fdcan_sram->fdcan1.std_filter;

	filter_element.sfid1 = 0;
	filter_element.sfid2 =  0x7FF;
	filter_element.sfec = CAN_STD_FILTER_SFEC_REJECT;
	filter_element.sft = CAN_STD_FILTER_SFT_CLASSIC;
	#ifdef CAN_DEBUG
	DEBUG("Filter at 0x%0X val 0x%0X",filter,filter_element.value);
	#endif
	*filter++ = filter_element.value;

	uint32_t std_filters_count = ((filter - (uint32_t *)&fdcan_sram->fdcan1.std_filter) * sizeof(filter)) / sizeof(struct std_filter_t);
	DEBUG("Total std filters: %d",std_filters_count);

	if(std_filters_count > 28){
		ERROR("To many std filters: %d of 28",std_filters_count);
		std_filters_count = 28;
	};

	if(std_filters_count > 0){
		DEBUG("Apply %d std filters",std_filters_count);
		FDCAN1->RXGFC = (std_filters_count << FDCAN_RXGFC_LSS_Pos)|FDCAN_RXGFC_ANFS;
	}
	
	#else
	FDCAN1->RXGFC = 0;
	#endif
}

void FDCAN1_IT0_IRQHandler(void) {
	FDCAN_GlobalTypeDef *CAN = FDCAN1;
	uint8_t rx_index;
	struct can_message_t msg;
	TRACE_ENTER_ISR;

	if((CAN->IR & FDCAN_IR_BO) != 0){
		if(CAN->PSR & FDCAN_PSR_BO){
			#ifdef CAN_DEBUG
			ERROR("FDCAN1 Enter Bus-off state");
			#endif

			if( (CAN->CCCR & FDCAN_CCCR_INIT) != 0){
				CAN->CCCR   &= ~(FDCAN_CCCR_INIT);
				return;
			}
		}
		CAN->IR = FDCAN_IR_BO;
		return;
	}

	if((CAN->IR & FDCAN_IR_RF0N) != 0){
		CAN->IR = FDCAN_IR_RF0N;
		rx_index= (uint8_t)((CAN->RXF0S >> 8) & 0x3);
	
		get_msg(CAN, &msg, rx_index);
		#ifdef CAN_DEBUG
		//DEBUG("CAN1 msg ID(%d): 0x%0X  %0X:%0X:%0X:%0X:%0X:%0X:%0X:%0X",msg.ext_id,msg.id,msg.msg[0],msg.msg[1],msg.msg[2],msg.msg[3],msg.msg[4],msg.msg[5],msg.msg[6],msg.msg[7]);
		#endif
		CAN->RXF0A = rx_index;
	};
	
	if((CAN->IR & FDCAN_IR_RF0L) != 0){
			CAN->IR = FDCAN_IR_RF0L;
			WARNING("CAN1 RX IRQ, lost message");
	};

	if((CAN->IR & FDCAN_IR_RF0F) != 0){
			WARNING("CAN1 RX IRQ FIFO full");
			while(CAN->RXF0S & FDCAN_RXF0S_F0FL_Msk){
				rx_index= (uint8_t)((CAN->RXF0S >> 8) & 0x03);
				get_msg(CAN,&msg,rx_index);
				#ifdef CAN_DEBUG
				DEBUG("CAN1 msg ID(%d): 0x%0X  %0X:%0X:%0X:%0X:%0X:%0X:%0X:%0X",msg.ext_id,msg.id,msg.msg[0],msg.msg[1],msg.msg[2],msg.msg[3],msg.msg[4],msg.msg[5],msg.msg[6],msg.msg[7]);
				#endif
				CAN->RXF0A = rx_index;
			}
			CAN->IR = FDCAN_IR_RF0F;
	};

	TRACE_EXIT_ISR;
}

void FDCAN2_IT0_IRQHandler(void) {
	FDCAN_GlobalTypeDef *CAN = FDCAN2;
	uint8_t rx_index;
	struct can_message_t msg;
	TRACE_ENTER_ISR;

	if((CAN->IR & FDCAN_IR_BO) != 0){
		if(CAN->PSR & FDCAN_PSR_BO){
			#ifdef CAN_DEBUG
			ERROR("FDCAN2 Enter Bus-off state");
			#endif

			if( (CAN->CCCR & FDCAN_CCCR_INIT) != 0){
				CAN->CCCR   &= ~(FDCAN_CCCR_INIT);
				return;
			}
		}
		CAN->IR = FDCAN_IR_BO;
		return;
	}

	if((CAN->IR & FDCAN_IR_RF0N) != 0){
		CAN->IR = FDCAN_IR_RF0N;
		rx_index= (uint8_t)((CAN->RXF0S >> 8) & 0x3);
	
		get_msg(CAN, &msg, rx_index);
		#ifdef CAN_DEBUG
		//DEBUG("CAN2 msg ID(%d): 0x%0X  %0X:%0X:%0X:%0X:%0X:%0X:%0X:%0X",msg.ext_id,msg.id,msg.msg[0],msg.msg[1],msg.msg[2],msg.msg[3],msg.msg[4],msg.msg[5],msg.msg[6],msg.msg[7]);
		#endif
		CAN->RXF0A = rx_index;
	};
	
	if((CAN->IR & FDCAN_IR_RF0L) != 0){
			CAN->IR = FDCAN_IR_RF0L;
			WARNING("CAN2 RX IRQ, lost message");
	};

	if((CAN->IR & FDCAN_IR_RF0F) != 0){
			WARNING("CAN2 RX IRQ FIFO full");
			while(CAN->RXF0S & FDCAN_RXF0S_F0FL_Msk){
				rx_index= (uint8_t)((CAN->RXF0S >> 8) & 0x03);
				get_msg(CAN,&msg,rx_index);
				#ifdef CAN_DEBUG
				DEBUG("CAN2 msg ID(%d): 0x%0X  %0X:%0X:%0X:%0X:%0X:%0X:%0X:%0X",msg.ext_id,msg.id,msg.msg[0],msg.msg[1],msg.msg[2],msg.msg[3],msg.msg[4],msg.msg[5],msg.msg[6],msg.msg[7]);
				#endif
				CAN->RXF0A = rx_index;
			}
			CAN->IR = FDCAN_IR_RF0F;
	};

	TRACE_EXIT_ISR;
}

void FDCAN3_IT0_IRQHandler(void) {
	FDCAN_GlobalTypeDef *CAN = FDCAN3;
	uint8_t rx_index;
	struct can_message_t msg = {0};
	TRACE_ENTER_ISR;

	if((CAN->IR & FDCAN_IR_BO) != 0){
		if(CAN->PSR & FDCAN_PSR_BO){
			#ifdef CAN_DEBUG
			ERROR("FDCAN3 Enter Bus-off state");
			#endif

			if( (CAN->CCCR & FDCAN_CCCR_INIT) != 0){
				CAN->CCCR   &= ~(FDCAN_CCCR_INIT);
				return;
			}
		}
		CAN->IR = FDCAN_IR_BO;
		return;
	}

	if((CAN->IR & FDCAN_IR_RF0N) != 0){
		CAN->IR = FDCAN_IR_RF0N;
		rx_index= (uint8_t)((CAN->RXF0S >> 8) & 0x3);
	
		get_msg(CAN, &msg, rx_index);
		#ifdef CAN_DEBUG
		DEBUG("CAN3 msg ID(%d): 0x%0X  %0X:%0X:%0X:%0X:%0X:%0X:%0X:%0X",msg.ext_id,msg.id,msg.msg[0],msg.msg[1],msg.msg[2],msg.msg[3],msg.msg[4],msg.msg[5],msg.msg[6],msg.msg[7]);
		#endif
		CAN->RXF0A = rx_index;
	};
	
	if((CAN->IR & FDCAN_IR_RF0L) != 0){
			CAN->IR = FDCAN_IR_RF0L;
			WARNING("CAN3 RX IRQ, lost message");
	};

	if((CAN->IR & FDCAN_IR_RF0F) != 0){
			WARNING("CAN3 RX IRQ FIFO full");
			while(CAN->RXF0S & FDCAN_RXF0S_F0FL_Msk){
				rx_index= (uint8_t)((CAN->RXF0S >> 8) & 0x03);
				get_msg(CAN,&msg,rx_index);
				#ifdef CAN_DEBUG
				DEBUG("CAN3 msg ID(%d): 0x%0X  %0X:%0X:%0X:%0X:%0X:%0X:%0X:%0X",msg.ext_id,msg.id,msg.msg[0],msg.msg[1],msg.msg[2],msg.msg[3],msg.msg[4],msg.msg[5],msg.msg[6],msg.msg[7]);
				#endif
				CAN->RXF0A = rx_index;
			}
			CAN->IR = FDCAN_IR_RF0F;
	};

	TRACE_EXIT_ISR;
}
