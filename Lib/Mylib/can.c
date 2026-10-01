#include "can.h"
#include "systime.h"

#define CAN_TX_BUFFERS		3
#define CAN_INIT_TIMEOUT	100000

struct can_channel_hw_t{
	FDCAN_GlobalTypeDef *can;
	IRQn_Type irq;
	GPIO_TypeDef *s_port;
	uint32_t s_pin;
};

struct can_ch_ctx_t{
	struct can_timing_t timing;
	struct can_timing_t data_timing;
	volatile enum can_state_t state;
	volatile uint8_t tx_inflight;			//Маска TX буферов, ожидающих завершения
	volatile bool busoff_pending;			//Ждёт восстановления из can_poll()
	volatile uint32_t busoff_time;
	uint8_t tx_marker[CAN_TX_BUFFERS];
};

static volatile struct fdcan_sram_t * const fdcan_sram = (volatile struct fdcan_sram_t *)SRAMCAN_BASE;

static const struct can_channel_hw_t can_hw[CAN_CH_COUNT] = {
	{FDCAN1, FDCAN1_IT0_IRQn, GPIOC, 13},
	{FDCAN2, FDCAN2_IT0_IRQn, GPIOB, 14},
	{FDCAN3, FDCAN3_IT0_IRQn, GPIOB, 4},
};

static struct can_ch_ctx_t can_ch[CAN_CH_COUNT];

static const uint8_t dlc2len[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

static void can_gpio_init(void);
static bool can_enter_init(FDCAN_GlobalTypeDef *CAN);
static void can_silent(uint8_t ch, bool on);
static void can_irq(uint8_t ch);
static void can_read_rx(uint8_t ch);
static void can_read_tx_event(uint8_t ch);
static void can_check_tx_cancel(uint8_t ch);
static enum can_state_t can_state_from_psr(uint32_t psr);
static void can_update_state(uint8_t ch, uint32_t psr);
static void can_bus_error(uint8_t ch, uint32_t ir, uint32_t psr);

void can_hw_init(void){
	can_gpio_init();

	RCC->CCIPR &= ~RCC_CCIPR_FDCANSEL;
	//PLLQ, 80Mhz
	RCC->CCIPR |= (1 << RCC_CCIPR_FDCANSEL_Pos);

	RCC->APB1ENR1 |= RCC_APB1ENR1_FDCANEN;
	(void)RCC->APB1ENR1;

	for(uint8_t ch = 0; ch < CAN_CH_COUNT; ch++){
		FDCAN_GlobalTypeDef *CAN = can_hw[ch].can;

		can_ch[ch].timing = (struct can_timing_t){CAN_DEFAULT_BRP, CAN_DEFAULT_TSEG1, CAN_DEFAULT_TSEG2, CAN_DEFAULT_SJW};
		can_ch[ch].data_timing = can_ch[ch].timing;
		can_ch[ch].state = CAN_STATE_STOPPED;
		can_ch[ch].tx_inflight = 0;

		if(!can_enter_init(CAN)){
			ERROR("FDCAN%d init timeout", ch + 1);
			continue;
		}

		/* Clean-up message RAM */
		volatile uint32_t *mem = (volatile uint32_t *)&fdcan_sram->fdcan[ch];
		for(uint32_t i = 0; i < sizeof(struct fdcan_mem_t) / sizeof(uint32_t); i++){
			mem[i] = 0;
		}

		CAN->IE = 0;
		CAN->ILS = 0;			//Все прерывания на линию 0
		CAN->ILE = FDCAN_ILE_EINT0;

		NVIC_SetPriority(can_hw[ch].irq, 5);
		NVIC_EnableIRQ(can_hw[ch].irq);
	}

	CAN1_ENABLE_IO;

	DEBUG("Can init done");
}

static void can_gpio_init(void){
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN|RCC_AHB2ENR_GPIOBEN|RCC_AHB2ENR_GPIOCEN;
	(void)RCC->AHB2ENR;

	/*********************************** FDCAN1 *****************************/
	/* PB8 RX, PB9 TX, AF9 */
	GPIOB->MODER &= ~(GPIO_MODER_MODER8|GPIO_MODER_MODER9);
	GPIOB->MODER |= (GPIO_MODER_MODER8_1|GPIO_MODER_MODER9_1);
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT_8|GPIO_OTYPER_OT_9);
	GPIOB->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR8|GPIO_OSPEEDER_OSPEEDR9);
	GPIOB->OSPEEDR |= (S_VH << GPIO_OSPEEDR_OSPEED8_Pos)|(S_VH << GPIO_OSPEEDR_OSPEED9_Pos);
	GPIOB->AFR[1] &= ~(GPIO_AFRH_AFSEL8|GPIO_AFRH_AFSEL9);
	GPIOB->AFR[1] |= (9 << GPIO_AFRH_AFSEL8_Pos)|(9 << GPIO_AFRH_AFSEL9_Pos);

	//VIO трансивера PB7
	CAN1_DISABLE_IO;
	GPIOB->MODER &= ~(GPIO_MODER_MODE7);
	GPIOB->MODER |= (GPIO_MODER_MODE7_0);
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT7);
	GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD7);

	/*********************************** FDCAN2 *****************************/
	/* PB12 RX, PB13 TX, AF9 */
	GPIOB->MODER &= ~(GPIO_MODER_MODER12|GPIO_MODER_MODER13);
	GPIOB->MODER |= (GPIO_MODER_MODER12_1|GPIO_MODER_MODER13_1);
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT_12|GPIO_OTYPER_OT_13);
	GPIOB->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR12|GPIO_OSPEEDER_OSPEEDR13);
	GPIOB->OSPEEDR |= (S_VH << GPIO_OSPEEDR_OSPEED12_Pos)|(S_VH << GPIO_OSPEEDR_OSPEED13_Pos);
	GPIOB->AFR[1] &= ~(GPIO_AFRH_AFSEL12|GPIO_AFRH_AFSEL13);
	GPIOB->AFR[1] |= (9 << GPIO_AFRH_AFSEL12_Pos)|(9 << GPIO_AFRH_AFSEL13_Pos);

	/*********************************** FDCAN3 *****************************/
	/* PB3 RX, AF11 */
	GPIOB->MODER &= ~(GPIO_MODER_MODER3);
	GPIOB->MODER |= (GPIO_MODER_MODER3_1);
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT_3);
	GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD3);
	GPIOB->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR3);
	GPIOB->OSPEEDR |= (S_VH << GPIO_OSPEEDR_OSPEED3_Pos);
	GPIOB->AFR[0] &= ~(GPIO_AFRL_AFSEL3);
	GPIOB->AFR[0] |= (11 << GPIO_AFRL_AFSEL3_Pos);

	/* PA15 TX, AF11 */
	GPIOA->MODER &= ~(GPIO_MODER_MODER15);
	GPIOA->MODER |= (GPIO_MODER_MODER15_1);
	GPIOA->OTYPER &= ~(GPIO_OTYPER_OT_15);
	GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD15);
	GPIOA->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR15);
	GPIOA->OSPEEDR |= (S_VH << GPIO_OSPEEDR_OSPEED15_Pos);
	GPIOA->AFR[1] &= ~(GPIO_AFRH_AFSEL15);
	GPIOA->AFR[1] |= (11 << GPIO_AFRH_AFSEL15_Pos);

	/* Silent: PC13, PB14, PB4. Push-pull, по умолчанию трансиверы в silent */
	for(uint8_t ch = 0; ch < CAN_CH_COUNT; ch++){
		GPIO_TypeDef *port = can_hw[ch].s_port;
		uint32_t pin = can_hw[ch].s_pin;

		can_silent(ch, true);
		port->MODER &= ~(3UL << (pin * 2));
		port->MODER |= (1UL << (pin * 2));
		port->OTYPER &= ~(1UL << pin);
		port->PUPDR &= ~(3UL << (pin * 2));
	}
}

static void can_silent(uint8_t ch, bool on){
	can_hw[ch].s_port->BSRR = on ? (1UL << can_hw[ch].s_pin) : (1UL << (can_hw[ch].s_pin + 16));
}

static bool can_enter_init(FDCAN_GlobalTypeDef *CAN){
	CAN->CCCR |= FDCAN_CCCR_INIT;
	for(uint32_t i = 0; i < CAN_INIT_TIMEOUT; i++){
		if(CAN->CCCR & FDCAN_CCCR_INIT){
			CAN->CCCR |= FDCAN_CCCR_CCE;
			return true;
		}
	}
	return false;
}

uint8_t can_dlc2len(uint8_t dlc, bool fd){
	dlc &= 0x0F;
	if(!fd && dlc > 8){
		return 8;
	}
	return dlc2len[dlc];
}

bool can_set_timing(uint8_t ch, const struct can_timing_t *timing){
	if(ch >= CAN_CH_COUNT){
		return false;
	}
	if(timing->brp < CAN_NBT_BRP_MIN || timing->brp > CAN_NBT_BRP_MAX ||
	   timing->tseg1 < CAN_NBT_TSEG1_MIN || timing->tseg1 > CAN_NBT_TSEG1_MAX ||
	   timing->tseg2 < CAN_NBT_TSEG2_MIN || timing->tseg2 > CAN_NBT_TSEG2_MAX ||
	   timing->sjw < 1 || timing->sjw > CAN_NBT_SJW_MAX || timing->sjw > timing->tseg2){
		ERROR("CAN%d bad timing brp %d tseg1 %d tseg2 %d sjw %d", ch + 1, timing->brp, timing->tseg1, timing->tseg2, timing->sjw);
		return false;
	}
	can_ch[ch].timing = *timing;
	return true;
}

bool can_set_data_timing(uint8_t ch, const struct can_timing_t *timing){
	if(ch >= CAN_CH_COUNT){
		return false;
	}
	if(timing->brp < CAN_DBT_BRP_MIN || timing->brp > CAN_DBT_BRP_MAX ||
	   timing->tseg1 < CAN_DBT_TSEG1_MIN || timing->tseg1 > CAN_DBT_TSEG1_MAX ||
	   timing->tseg2 < CAN_DBT_TSEG2_MIN || timing->tseg2 > CAN_DBT_TSEG2_MAX ||
	   timing->sjw < 1 || timing->sjw > CAN_DBT_SJW_MAX || timing->sjw > timing->tseg2){
		ERROR("CAN%d bad data timing brp %d tseg1 %d tseg2 %d sjw %d", ch + 1, timing->brp, timing->tseg1, timing->tseg2, timing->sjw);
		return false;
	}
	can_ch[ch].data_timing = *timing;
	return true;
}

bool can_start(uint8_t ch, uint32_t mode){
	if(ch >= CAN_CH_COUNT){
		return false;
	}

	FDCAN_GlobalTypeDef *CAN = can_hw[ch].can;
	const struct can_timing_t *nt = &can_ch[ch].timing;
	const struct can_timing_t *dt = &can_ch[ch].data_timing;

	can_stop(ch);

	/* CCE=1 сбрасывает RXF0S, TXFQS, TXBRP, TXBTO, TXBCF, TXEFS */
	if(!can_enter_init(CAN)){
		ERROR("FDCAN%d init timeout", ch + 1);
		return false;
	}

	uint32_t cccr = CAN->CCCR;
	cccr &= ~(FDCAN_CCCR_MON|FDCAN_CCCR_DAR|FDCAN_CCCR_TEST|FDCAN_CCCR_FDOE|FDCAN_CCCR_BRSE|
			  FDCAN_CCCR_ASM|FDCAN_CCCR_PXHD|FDCAN_CCCR_NISO|FDCAN_CCCR_TXP|FDCAN_CCCR_EFBI|FDCAN_CCCR_CSR);

	if(mode & CAN_MODE_LISTEN_ONLY){
		cccr |= FDCAN_CCCR_MON;
	}
	if(mode & CAN_MODE_LOOPBACK){
		cccr |= FDCAN_CCCR_TEST|FDCAN_CCCR_MON;
	}
	if(mode & CAN_MODE_ONE_SHOT){
		cccr |= FDCAN_CCCR_DAR;
	}
	if(mode & CAN_MODE_FD){
		cccr |= FDCAN_CCCR_FDOE|FDCAN_CCCR_BRSE;
	}
	CAN->CCCR = cccr;
	CAN->TEST = (mode & CAN_MODE_LOOPBACK) ? FDCAN_TEST_LBCK : 0;

	CAN->NBTP = ((uint32_t)(nt->sjw - 1) << FDCAN_NBTP_NSJW_Pos)|
				((uint32_t)(nt->brp - 1) << FDCAN_NBTP_NBRP_Pos)|
				((uint32_t)(nt->tseg1 - 1) << FDCAN_NBTP_NTSEG1_Pos)|
				((uint32_t)(nt->tseg2 - 1) << FDCAN_NBTP_NTSEG2_Pos);

	uint32_t dbtp = ((uint32_t)(dt->sjw - 1) << FDCAN_DBTP_DSJW_Pos)|
					((uint32_t)(dt->brp - 1) << FDCAN_DBTP_DBRP_Pos)|
					((uint32_t)(dt->tseg1 - 1) << FDCAN_DBTP_DTSEG1_Pos)|
					((uint32_t)(dt->tseg2 - 1) << FDCAN_DBTP_DTSEG2_Pos);

	/* Компенсация задержки трансивера нужна на высоких скоростях фазы данных (AN5348) */
	if((mode & CAN_MODE_FD) && dt->brp <= 2){
		uint32_t tdco = (uint32_t)dt->brp * dt->tseg1;
		if(tdco > 127){
			tdco = 127;
		}
		CAN->TDCR = (tdco << FDCAN_TDCR_TDCO_Pos);
		dbtp |= FDCAN_DBTP_TDC;
	}else{
		CAN->TDCR = 0;
	}
	CAN->DBTP = dbtp;

	/* Без фильтров: все кадры (std/ext/remote) в FIFO0, FIFO в блокирующем режиме */
	CAN->RXGFC = 0;
	/* TX FIFO */
	CAN->TXBC = 0;
	CAN->TXBCIE = (1UL << CAN_TX_BUFFERS) - 1;

	CAN->IR = 0x00FFFFFF;
	CAN->IE = FDCAN_IE_RF0NE|FDCAN_IE_RF0LE|FDCAN_IE_TEFNE|FDCAN_IE_TCFE|
			  FDCAN_IE_EWE|FDCAN_IE_EPE|FDCAN_IE_BOE;
	if(mode & CAN_MODE_BERR){
		CAN->IE |= FDCAN_IE_PEAE|FDCAN_IE_PEDE;
	}

	can_ch[ch].tx_inflight = 0;
	can_ch[ch].state = CAN_STATE_ACTIVE;

	can_silent(ch, (mode & (CAN_MODE_LISTEN_ONLY|CAN_MODE_LOOPBACK)) != 0);

	NVIC_DisableIRQ(can_hw[ch].irq);

	CAN->CCCR &= ~FDCAN_CCCR_INIT;
	for(uint32_t i = 0; i < CAN_INIT_TIMEOUT && (CAN->CCCR & FDCAN_CCCR_INIT); i++){};

	/* INIT не сбрасывает счётчики ошибок: после старта из bus-off идёт восстановление */
	can_update_state(ch, CAN->PSR);

	NVIC_EnableIRQ(can_hw[ch].irq);

	#ifdef CAN_DEBUG
	DEBUG("CAN%d start mode 0x%X NBTP 0x%08X DBTP 0x%08X", ch + 1, mode, CAN->NBTP, CAN->DBTP);
	#endif

	return true;
}

void can_stop(uint8_t ch){
	if(ch >= CAN_CH_COUNT){
		return;
	}

	FDCAN_GlobalTypeDef *CAN = can_hw[ch].can;

	NVIC_DisableIRQ(can_hw[ch].irq);

	CAN->CCCR |= FDCAN_CCCR_INIT;
	for(uint32_t i = 0; i < CAN_INIT_TIMEOUT && !(CAN->CCCR & FDCAN_CCCR_INIT); i++){};

	CAN->IE = 0;
	CAN->IR = 0x00FFFFFF;
	NVIC_ClearPendingIRQ(can_hw[ch].irq);

	can_ch[ch].tx_inflight = 0;
	can_ch[ch].busoff_pending = false;
	can_ch[ch].state = CAN_STATE_STOPPED;
	can_silent(ch, true);

	NVIC_EnableIRQ(can_hw[ch].irq);
}

int can_tx(uint8_t ch, const struct can_frame_t *frame, uint8_t marker){
	if(ch >= CAN_CH_COUNT || can_ch[ch].state == CAN_STATE_STOPPED){
		return -1;
	}

	FDCAN_GlobalTypeDef *CAN = can_hw[ch].can;
	const bool fd = (frame->flags & CAN_FLAG_FD) && (CAN->CCCR & FDCAN_CCCR_FDOE);
	int result = -1;

	uint32_t t0;
	if(frame->id & CAN_ID_EFF){
		t0 = CAN_ELEM_XTD | (frame->id & CAN_ID_EFF_MASK);
	}else{
		t0 = (frame->id & CAN_ID_SFF_MASK) << 18;
	}

	uint32_t t1 = ((uint32_t)marker << CAN_ELEM_MM_Pos) | CAN_ELEM_EFC;
	if(fd){
		t1 |= CAN_ELEM_FDF;
		if(frame->flags & CAN_FLAG_BRS){
			t1 |= CAN_ELEM_BRS;
		}
		if(frame->flags & CAN_FLAG_ESI){
			t0 |= CAN_ELEM_ESI;
		}
		t1 |= (uint32_t)(frame->dlc & 0x0F) << CAN_ELEM_DLC_Pos;
	}else{
		if(frame->id & CAN_ID_RTR){
			t0 |= CAN_ELEM_RTR;
		}
		t1 |= (uint32_t)(frame->dlc & 0x0F) << CAN_ELEM_DLC_Pos;
	}

	const uint32_t words = (can_dlc2len(frame->dlc, fd) + 3) / 4;
	const uint32_t *data = (const uint32_t *)frame->data;

	uint32_t primask = __get_PRIMASK();
	__disable_irq();

	if((CAN->TXFQS & FDCAN_TXFQS_TFQF) == 0){
		uint32_t idx = (CAN->TXFQS & FDCAN_TXFQS_TFQPI) >> FDCAN_TXFQS_TFQPI_Pos;

		if(idx < CAN_TX_BUFFERS){
			volatile uint32_t *el = fdcan_sram->fdcan[ch].can_tx_fifo[idx].b;

			el[0] = t0;
			el[1] = t1;
			for(uint32_t i = 0; i < words; i++){
				el[2 + i] = data[i];
			}

			can_ch[ch].tx_marker[idx] = marker;
			can_ch[ch].tx_inflight |= (1U << idx);
			CAN->TXBAR = (1UL << idx);
			result = 0;
		}
	}

	__set_PRIMASK(primask);
	return result;
}

enum can_state_t can_get_state(uint8_t ch, uint8_t *tec, uint8_t *rec){
	*tec = 0;
	*rec = 0;
	if(ch >= CAN_CH_COUNT){
		return CAN_STATE_STOPPED;
	}
	uint32_t ecr = can_hw[ch].can->ECR;
	*tec = (ecr & FDCAN_ECR_TEC) >> FDCAN_ECR_TEC_Pos;
	*rec = (ecr & FDCAN_ECR_REC) >> FDCAN_ECR_REC_Pos;
	return can_ch[ch].state;
}

/*********************** IRQ ***********************/

static void can_irq(uint8_t ch){
	FDCAN_GlobalTypeDef *CAN = can_hw[ch].can;
	TRACE_ENTER_ISR;

	uint32_t ir = CAN->IR & CAN->IE;
	CAN->IR = ir;

	if(ir & (FDCAN_IR_RF0N|FDCAN_IR_RF0F)){
		can_read_rx(ch);
	}

	if(ir & FDCAN_IR_RF0L){
		can_rx_lost_cb(ch);
	}

	if(ir & FDCAN_IR_TEFN){
		can_read_tx_event(ch);
	}

	if(ir & FDCAN_IR_TCF){
		can_check_tx_cancel(ch);
	}

	if(ir & (FDCAN_IR_EW|FDCAN_IR_EP|FDCAN_IR_BO|FDCAN_IR_PEA|FDCAN_IR_PED)){
		/* Чтение PSR сбрасывает LEC/DLEC, поэтому читается один раз */
		uint32_t psr = CAN->PSR;

		if(ir & (FDCAN_IR_PEA|FDCAN_IR_PED)){
			can_bus_error(ch, ir, psr);
		}
		if(ir & (FDCAN_IR_EW|FDCAN_IR_EP|FDCAN_IR_BO)){
			can_update_state(ch, psr);
		}
	}

	TRACE_EXIT_ISR;
}

static void can_read_rx(uint8_t ch){
	FDCAN_GlobalTypeDef *CAN = can_hw[ch].can;
	struct can_frame_t frame;

	while(CAN->RXF0S & FDCAN_RXF0S_F0FL){
		uint32_t idx = (CAN->RXF0S & FDCAN_RXF0S_F0GI) >> FDCAN_RXF0S_F0GI_Pos;
		volatile const uint32_t *el = fdcan_sram->fdcan[ch].rx0_fifo[idx].b;
		uint32_t r0 = el[0];
		uint32_t r1 = el[1];

		if(r0 & CAN_ELEM_XTD){
			frame.id = CAN_ID_EFF | (r0 & CAN_ID_EFF_MASK);
		}else{
			frame.id = (r0 >> 18) & CAN_ID_SFF_MASK;
		}

		frame.flags = 0;
		frame.dlc = (r1 >> CAN_ELEM_DLC_Pos) & 0x0F;

		const bool fd = (r1 & CAN_ELEM_FDF) != 0;
		if(fd){
			frame.flags |= CAN_FLAG_FD;
			if(r1 & CAN_ELEM_BRS){
				frame.flags |= CAN_FLAG_BRS;
			}
			if(r0 & CAN_ELEM_ESI){
				frame.flags |= CAN_FLAG_ESI;
			}
		}else if(r0 & CAN_ELEM_RTR){
			frame.id |= CAN_ID_RTR;
		}

		uint32_t *data = (uint32_t *)frame.data;
		const uint32_t words = (can_dlc2len(frame.dlc, fd) + 3) / 4;
		for(uint32_t i = 0; i < words; i++){
			data[i] = el[2 + i];
		}
		for(uint32_t i = words; i < (fd ? 16U : 2U); i++){
			data[i] = 0;
		}

		CAN->RXF0A = idx;

		can_rx_cb(ch, &frame);
	}
}

static void can_release_tx_buffer(uint8_t ch, uint8_t marker){
	for(uint8_t idx = 0; idx < CAN_TX_BUFFERS; idx++){
		if((can_ch[ch].tx_inflight & (1U << idx)) && can_ch[ch].tx_marker[idx] == marker){
			can_ch[ch].tx_inflight &= ~(1U << idx);
			return;
		}
	}
}

static void can_read_tx_event(uint8_t ch){
	FDCAN_GlobalTypeDef *CAN = can_hw[ch].can;

	while(CAN->TXEFS & FDCAN_TXEFS_EFFL){
		uint32_t idx = (CAN->TXEFS & FDCAN_TXEFS_EFGI) >> FDCAN_TXEFS_EFGI_Pos;
		uint8_t marker = (fdcan_sram->fdcan[ch].tx_event[idx].b[1] >> CAN_ELEM_MM_Pos) & 0xFF;

		CAN->TXEFA = idx;

		can_release_tx_buffer(ch, marker);
		can_tx_done_cb(ch, marker, true);
	}
}

/* Отменённые кадры (one-shot без ACK, bus-off) не попадают в TX event FIFO */
static void can_check_tx_cancel(uint8_t ch){
	FDCAN_GlobalTypeDef *CAN = can_hw[ch].can;
	uint32_t pending = CAN->TXBRP;
	uint32_t cancelled = CAN->TXBCF;
	uint32_t sent = CAN->TXBTO;

	for(uint8_t idx = 0; idx < CAN_TX_BUFFERS; idx++){
		uint32_t bit = 1UL << idx;

		if((can_ch[ch].tx_inflight & bit) && !(pending & bit) && (cancelled & bit) && !(sent & bit)){
			can_ch[ch].tx_inflight &= ~bit;
			can_tx_done_cb(ch, can_ch[ch].tx_marker[idx], false);
		}
	}
}

static enum can_state_t can_state_from_psr(uint32_t psr){
	if(psr & FDCAN_PSR_BO){
		return CAN_STATE_BUS_OFF;
	}
	if(psr & FDCAN_PSR_EP){
		return CAN_STATE_PASSIVE;
	}
	if(psr & FDCAN_PSR_EW){
		return CAN_STATE_WARNING;
	}
	return CAN_STATE_ACTIVE;
}

static void can_update_state(uint8_t ch, uint32_t psr){
	FDCAN_GlobalTypeDef *CAN = can_hw[ch].can;
	enum can_state_t state = can_state_from_psr(psr);

	if(state == CAN_STATE_BUS_OFF && (CAN->CCCR & FDCAN_CCCR_INIT) && !can_ch[ch].busoff_pending){
		/* Контроллер остановлен аппаратно, восстановление через CAN_BUSOFF_RESTART_MS в can_poll() */
		#ifdef CAN_DEBUG
		ERROR("CAN%d bus-off", ch + 1);
		#endif
		can_ch[ch].busoff_time = systime_ms();
		can_ch[ch].busoff_pending = true;
	}

	enum can_state_t prev = can_ch[ch].state;
	if(state != prev){
		uint32_t ecr = CAN->ECR;
		can_ch[ch].state = state;
		can_state_cb(ch, state, prev, (ecr & FDCAN_ECR_TEC) >> FDCAN_ECR_TEC_Pos, (ecr & FDCAN_ECR_REC) >> FDCAN_ECR_REC_Pos);
	}
}

void can_poll(void){
	for(uint8_t ch = 0; ch < CAN_CH_COUNT; ch++){
		if(!can_ch[ch].busoff_pending || (systime_ms() - can_ch[ch].busoff_time) < CAN_BUSOFF_RESTART_MS){
			continue;
		}

		NVIC_DisableIRQ(can_hw[ch].irq);
		/* can_stop() мог успеть сбросить флаг */
		if(can_ch[ch].busoff_pending){
			can_ch[ch].busoff_pending = false;
			#ifdef CAN_DEBUG
			DEBUG("CAN%d bus-off recovery", ch + 1);
			#endif
			/* После 129x11 рецессивных бит BO сбросится, придёт прерывание BO */
			can_hw[ch].can->CCCR &= ~FDCAN_CCCR_INIT;
		}
		NVIC_EnableIRQ(can_hw[ch].irq);
	}
}

static void can_bus_error(uint8_t ch, uint32_t ir, uint32_t psr){
	uint32_t ecr = can_hw[ch].can->ECR;
	uint8_t tec = (ecr & FDCAN_ECR_TEC) >> FDCAN_ECR_TEC_Pos;
	uint8_t rec = (ecr & FDCAN_ECR_REC) >> FDCAN_ECR_REC_Pos;

	if(ir & FDCAN_IR_PEA){
		enum can_lec_t lec = (enum can_lec_t)((psr & FDCAN_PSR_LEC) >> FDCAN_PSR_LEC_Pos);
		if(lec != CAN_LEC_NONE && lec != CAN_LEC_NO_CHANGE){
			can_bus_error_cb(ch, lec, false, tec, rec);
		}
	}
	if(ir & FDCAN_IR_PED){
		enum can_lec_t lec = (enum can_lec_t)((psr & FDCAN_PSR_DLEC) >> FDCAN_PSR_DLEC_Pos);
		if(lec != CAN_LEC_NONE && lec != CAN_LEC_NO_CHANGE){
			can_bus_error_cb(ch, lec, true, tec, rec);
		}
	}
}

void FDCAN1_IT0_IRQHandler(void){
	can_irq(CAN_CH1);
}

void FDCAN2_IT0_IRQHandler(void){
	can_irq(CAN_CH2);
}

void FDCAN3_IT0_IRQHandler(void){
	can_irq(CAN_CH3);
}

/*********************** Default callbacks ***********************/

__attribute__((weak)) void can_rx_cb(uint8_t UNUSED(ch), const struct can_frame_t *UNUSED(frame)){
}

__attribute__((weak)) void can_rx_lost_cb(uint8_t UNUSED(ch)){
}

__attribute__((weak)) void can_tx_done_cb(uint8_t UNUSED(ch), uint8_t UNUSED(marker), bool UNUSED(sent)){
}

__attribute__((weak)) void can_state_cb(uint8_t UNUSED(ch), enum can_state_t UNUSED(state), enum can_state_t UNUSED(prev), uint8_t UNUSED(tec), uint8_t UNUSED(rec)){
}

__attribute__((weak)) void can_bus_error_cb(uint8_t UNUSED(ch), enum can_lec_t UNUSED(lec), bool UNUSED(data_phase), uint8_t UNUSED(tec), uint8_t UNUSED(rec)){
}
