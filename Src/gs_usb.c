#include <string.h>
#include "gs_usb.h"
#include "can.h"
#include "led.h"
#include "rcc.h"
#include "systime.h"
#include "tusb.h"
#include "device/usbd_pvt.h"

/*
Поток данных:
  FDCAN IRQ -> can_rx_cb()/can_tx_done_cb() -> gs_ring -> gs_in_send() -> EP IN
  EP OUT -> gs_out_process() -> слот канала -> очередь канала -> gs_tx_kick() -> TX FIFO FDCAN
Кольцо gs_ring заполняют только прерывания FDCAN (один приоритет, друг друга не вытесняют),
читает только основной цикл (tud_task/gs_usb_poll).
*/

#define GS_TX_SLOTS			12			//Слотов на канал, не меньше GS_USB_HOST_MAX_TX
#define GS_RING_SIZE		128			//Степень двойки
#define GS_ECHO_RESERVE		(CAN_CH_COUNT * GS_TX_SLOTS)	//Место в кольце, которое RX кадры не занимают

#define GS_FEATURES			(GS_CAN_FEATURE_LISTEN_ONLY|GS_CAN_FEATURE_LOOP_BACK|GS_CAN_FEATURE_ONE_SHOT|\
							 GS_CAN_FEATURE_HW_TIMESTAMP|GS_CAN_FEATURE_IDENTIFY|GS_CAN_FEATURE_FD|\
							 GS_CAN_FEATURE_BT_CONST_EXT|GS_CAN_FEATURE_BERR_REPORTING|GS_CAN_FEATURE_GET_STATE)

_Static_assert((GS_RING_SIZE & (GS_RING_SIZE - 1)) == 0, "GS_RING_SIZE must be power of 2");
_Static_assert(GS_TX_SLOTS >= GS_USB_HOST_MAX_TX, "GS_TX_SLOTS too small");
_Static_assert(GS_TX_SLOTS <= 256, "slot index is 8 bit marker");
_Static_assert(GS_RING_SIZE > GS_ECHO_RESERVE, "GS_RING_SIZE too small");

enum gs_slot_state_t{
	GS_SLOT_FREE = 0,
	GS_SLOT_QUEUED,
	GS_SLOT_INFLIGHT
};

struct gs_ring_entry_t{
	struct gs_host_frame hf;
	uint16_t len;
};

struct gs_tx_slot_t{
	struct gs_host_frame hf;
	volatile uint8_t state;
};

struct gs_channel_t{
	struct gs_tx_slot_t slot[GS_TX_SLOTS];
	uint8_t queue[GS_TX_SLOTS];		//Индексы слотов в порядке прихода от хоста
	uint8_t q_head;
	uint8_t q_tail;
	uint8_t q_count;
	volatile bool started;
	volatile uint32_t flags;		//GS_CAN_MODE_* активного режима
	volatile bool rx_overflow;
};

static struct gs_channel_t gs_ch[CAN_CH_COUNT];

static struct gs_ring_entry_t gs_ring[GS_RING_SIZE];
static volatile uint32_t ring_head = 0;
static volatile uint32_t ring_tail = 0;

static uint8_t ep_in = 0;
static uint8_t ep_out = 0;
static bool in_busy = false;
static bool out_pending = false;
static uint32_t out_len = 0;

CFG_TUSB_MEM_ALIGN static struct gs_host_frame out_buf;
CFG_TUSB_MEM_ALIGN static uint8_t ctrl_buf[80];

static void gs_channel_reset(uint8_t ch);
static void gs_tx_kick(uint8_t ch);
static void gs_out_arm(void);
static void gs_out_process(void);
static void gs_in_send(void);

/*********************** Кольцо кадров к хосту (сторона прерываний) ***********************/

static struct gs_ring_entry_t *gs_ring_alloc(uint32_t reserve){
	if(GS_RING_SIZE - (ring_head - ring_tail) <= reserve){
		return NULL;
	}
	return &gs_ring[ring_head & (GS_RING_SIZE - 1)];
}

static inline uint16_t gs_frame_len(bool fd, bool ts){
	return GS_HOST_FRAME_HDR_SIZE + (fd ? GS_HOST_FRAME_FD_DATA : GS_HOST_FRAME_CLASSIC_DATA) + (ts ? GS_HOST_FRAME_TS_SIZE : 0);
}

static void gs_ring_commit(uint8_t ch, struct gs_ring_entry_t *e, bool fd){
	const bool ts = (gs_ch[ch].flags & GS_CAN_MODE_HW_TIMESTAMP) != 0;

	if(ts){
		uint32_t timestamp = systime_us();
		memcpy(&e->hf.data[fd ? GS_HOST_FRAME_FD_DATA : GS_HOST_FRAME_CLASSIC_DATA], &timestamp, sizeof(timestamp));
	}
	e->len = gs_frame_len(fd, ts);

	__DMB();
	ring_head++;
}

static struct gs_ring_entry_t *gs_err_frame_alloc(uint8_t ch, uint32_t can_id, uint8_t tec, uint8_t rec){
	struct gs_ring_entry_t *e = gs_ring_alloc(GS_ECHO_RESERVE);
	if(e == NULL){
		return NULL;
	}

	struct gs_host_frame *hf = &e->hf;
	hf->echo_id = GS_HOST_FRAME_ECHO_ID_RX;
	hf->can_id = CAN_ID_ERR | CAN_ERR_CNT | can_id;
	hf->can_dlc = CAN_ERR_DLC;
	hf->channel = ch;
	hf->flags = 0;
	hf->reserved = 0;
	memset(hf->data, 0, GS_HOST_FRAME_CLASSIC_DATA);
	hf->data[6] = tec;
	hf->data[7] = rec;
	return e;
}

/*********************** Колбэки CAN (прерывания FDCAN) ***********************/

void can_rx_cb(uint8_t ch, const struct can_frame_t *frame){
	struct gs_channel_t *c = &gs_ch[ch];

	if(!c->started){
		return;
	}

	led_channel_activity(ch);

	struct gs_ring_entry_t *e = gs_ring_alloc(GS_ECHO_RESERVE);
	if(e == NULL){
		c->rx_overflow = true;
		return;
	}

	const bool fd = (frame->flags & CAN_FLAG_FD) != 0;
	struct gs_host_frame *hf = &e->hf;

	hf->echo_id = GS_HOST_FRAME_ECHO_ID_RX;
	hf->can_id = frame->id;
	hf->can_dlc = frame->dlc;
	hf->channel = ch;
	hf->flags = 0;
	hf->reserved = 0;

	if(fd){
		hf->flags |= GS_CAN_FLAG_FD;
		if(frame->flags & CAN_FLAG_BRS){
			hf->flags |= GS_CAN_FLAG_BRS;
		}
		if(frame->flags & CAN_FLAG_ESI){
			hf->flags |= GS_CAN_FLAG_ESI;
		}
	}

	if(c->rx_overflow){
		hf->flags |= GS_CAN_FLAG_OVERFLOW;
		c->rx_overflow = false;
	}

	memcpy(hf->data, frame->data, fd ? GS_HOST_FRAME_FD_DATA : GS_HOST_FRAME_CLASSIC_DATA);

	gs_ring_commit(ch, e, fd);
}

void can_rx_lost_cb(uint8_t ch){
	gs_ch[ch].rx_overflow = true;
}

void can_tx_done_cb(uint8_t ch, uint8_t marker, bool sent){
	if(marker >= GS_TX_SLOTS){
		return;
	}

	struct gs_tx_slot_t *s = &gs_ch[ch].slot[marker];
	if(s->state != GS_SLOT_INFLIGHT){
		return;
	}

	if(sent){
		led_channel_activity(ch);
	}else{
		WARNING("CAN%d frame 0x%X not sent", ch + 1, s->hf.can_id);
	}

	/* Эхо возвращается и для отменённых кадров, иначе драйвер Linux не освободит контекст */
	struct gs_ring_entry_t *e = gs_ring_alloc(0);
	if(e != NULL){
		const bool fd = (s->hf.flags & GS_CAN_FLAG_FD) != 0;
		memcpy(&e->hf, &s->hf, GS_HOST_FRAME_HDR_SIZE + (fd ? GS_HOST_FRAME_FD_DATA : GS_HOST_FRAME_CLASSIC_DATA));
		gs_ring_commit(ch, e, fd);
	}else{
		ERROR("CAN%d echo lost", ch + 1);
	}

	s->state = GS_SLOT_FREE;
}

void can_state_cb(uint8_t ch, enum can_state_t state, enum can_state_t prev, uint8_t tec, uint8_t rec){
	if(!gs_ch[ch].started){
		return;
	}

	switch(state){
	case CAN_STATE_ACTIVE:
		led_channel_state(ch, LED_CH_OK);
		break;
	case CAN_STATE_WARNING:
	case CAN_STATE_PASSIVE:
		led_channel_state(ch, LED_CH_WARNING);
		break;
	case CAN_STATE_BUS_OFF:
		led_channel_state(ch, LED_CH_ERROR);
		break;
	default:
		break;
	}

	uint32_t can_id = 0;
	uint8_t crtl = 0;

	switch(state){
	case CAN_STATE_BUS_OFF:
		can_id = CAN_ERR_BUSOFF;
		break;
	case CAN_STATE_PASSIVE:
		can_id = CAN_ERR_CRTL;
		crtl = (tec >= 128) ? CAN_ERR_CRTL_TX_PASSIVE : CAN_ERR_CRTL_RX_PASSIVE;
		break;
	case CAN_STATE_WARNING:
		can_id = CAN_ERR_CRTL;
		crtl = (tec >= rec) ? CAN_ERR_CRTL_TX_WARNING : CAN_ERR_CRTL_RX_WARNING;
		break;
	case CAN_STATE_ACTIVE:
		if(prev == CAN_STATE_BUS_OFF){
			can_id = CAN_ERR_RESTARTED;
		}else{
			can_id = CAN_ERR_CRTL;
			crtl = CAN_ERR_CRTL_ACTIVE;
		}
		break;
	default:
		return;
	}

	struct gs_ring_entry_t *e = gs_err_frame_alloc(ch, can_id, tec, rec);
	if(e == NULL){
		return;
	}
	e->hf.data[1] = crtl;
	gs_ring_commit(ch, e, false);
}

void can_bus_error_cb(uint8_t ch, enum can_lec_t lec, bool UNUSED(data_phase), uint8_t tec, uint8_t rec){
	if(!gs_ch[ch].started || !(gs_ch[ch].flags & GS_CAN_MODE_BERR_REPORTING)){
		return;
	}

	uint32_t can_id = CAN_ERR_PROT|CAN_ERR_BUSERROR;
	uint8_t type = 0;
	uint8_t location = 0;

	switch(lec){
	case CAN_LEC_STUFF:
		type = CAN_ERR_PROT_STUFF;
		break;
	case CAN_LEC_FORM:
		type = CAN_ERR_PROT_FORM;
		break;
	case CAN_LEC_ACK:
		can_id |= CAN_ERR_ACK;
		location = CAN_ERR_PROT_LOC_ACK;
		break;
	case CAN_LEC_BIT1:
		type = CAN_ERR_PROT_BIT1;
		break;
	case CAN_LEC_BIT0:
		type = CAN_ERR_PROT_BIT0;
		break;
	case CAN_LEC_CRC:
		location = CAN_ERR_PROT_LOC_CRC_SEQ;
		break;
	default:
		return;
	}

	struct gs_ring_entry_t *e = gs_err_frame_alloc(ch, can_id, tec, rec);
	if(e == NULL){
		return;
	}
	e->hf.data[2] = type;
	e->hf.data[3] = location;
	gs_ring_commit(ch, e, false);
}

/*********************** Основной цикл ***********************/

static void gs_channel_reset(uint8_t ch){
	struct gs_channel_t *c = &gs_ch[ch];

	c->started = false;
	can_stop(ch);
	led_channel_state(ch, LED_CH_OFF);

	for(uint8_t i = 0; i < GS_TX_SLOTS; i++){
		c->slot[i].state = GS_SLOT_FREE;
	}
	c->q_head = 0;
	c->q_tail = 0;
	c->q_count = 0;
	c->flags = 0;
	c->rx_overflow = false;
}

static void gs_tx_kick(uint8_t ch){
	struct gs_channel_t *c = &gs_ch[ch];
	struct can_frame_t frame;

	while(c->q_count > 0){
		uint8_t idx = c->queue[c->q_tail];
		struct gs_tx_slot_t *s = &c->slot[idx];
		const bool fd = (s->hf.flags & GS_CAN_FLAG_FD) != 0;

		frame.id = s->hf.can_id;
		frame.dlc = s->hf.can_dlc;
		frame.flags = 0;
		if(fd){
			frame.flags |= CAN_FLAG_FD;
			if(s->hf.flags & GS_CAN_FLAG_BRS){
				frame.flags |= CAN_FLAG_BRS;
			}
			if(s->hf.flags & GS_CAN_FLAG_ESI){
				frame.flags |= CAN_FLAG_ESI;
			}
		}
		memcpy(frame.data, s->hf.data, fd ? GS_HOST_FRAME_FD_DATA : GS_HOST_FRAME_CLASSIC_DATA);

		/* INFLIGHT до can_tx(): прерывание может завершить передачу раньше возврата */
		s->state = GS_SLOT_INFLIGHT;
		if(can_tx(ch, &frame, idx) != 0){
			s->state = GS_SLOT_QUEUED;
			break;
		}

		c->q_tail = (c->q_tail + 1) % GS_TX_SLOTS;
		c->q_count--;
	}
}

static void gs_out_arm(void){
	if(ep_out != 0){
		usbd_edpt_xfer(0, ep_out, (uint8_t *)&out_buf, sizeof(out_buf), false);
	}
}

static void gs_out_process(void){
	if(!out_pending){
		return;
	}

	const struct gs_host_frame *hf = &out_buf;
	const uint8_t ch = hf->channel;
	const bool fd = (hf->flags & GS_CAN_FLAG_FD) != 0;
	const uint32_t need = GS_HOST_FRAME_HDR_SIZE + (fd ? GS_HOST_FRAME_FD_DATA : GS_HOST_FRAME_CLASSIC_DATA);

	if(ch >= CAN_CH_COUNT || !gs_ch[ch].started || out_len < need){
		WARNING("Drop host frame ch %d len %d", ch, out_len);
		out_pending = false;
		gs_out_arm();
		return;
	}

	struct gs_channel_t *c = &gs_ch[ch];
	uint8_t idx;
	for(idx = 0; idx < GS_TX_SLOTS; idx++){
		if(c->slot[idx].state == GS_SLOT_FREE){
			break;
		}
	}
	if(idx == GS_TX_SLOTS){
		/* Нет свободных слотов: OUT не перевзводится, хост получает NAK до освобождения */
		return;
	}

	memcpy(&c->slot[idx].hf, hf, need);
	c->slot[idx].state = GS_SLOT_QUEUED;
	c->queue[c->q_head] = idx;
	c->q_head = (c->q_head + 1) % GS_TX_SLOTS;
	c->q_count++;

	out_pending = false;
	gs_out_arm();

	gs_tx_kick(ch);
}

static void gs_in_send(void){
	if(in_busy || ep_in == 0 || !tud_ready()){
		return;
	}
	if(ring_head == ring_tail){
		return;
	}
	__DMB();

	struct gs_ring_entry_t *e = &gs_ring[ring_tail & (GS_RING_SIZE - 1)];
	if(usbd_edpt_xfer(0, ep_in, (uint8_t *)&e->hf, e->len, false)){
		in_busy = true;
	}
}

void gs_usb_poll(void){
	gs_out_process();

	for(uint8_t ch = 0; ch < CAN_CH_COUNT; ch++){
		if(gs_ch[ch].started){
			gs_tx_kick(ch);
		}
	}

	gs_in_send();
}

/*********************** Управляющие запросы ***********************/

static bool gs_set_mode(uint8_t ch, const struct gs_device_mode *mode){
	struct gs_channel_t *c = &gs_ch[ch];

	gs_channel_reset(ch);

	if(mode->mode == GS_CAN_MODE_RESET){
		DEBUG("CAN%d stop", ch + 1);
		return true;
	}

	if(mode->mode != GS_CAN_MODE_START){
		return false;
	}

	uint32_t can_mode = 0;
	if(mode->flags & GS_CAN_MODE_LISTEN_ONLY){
		can_mode |= CAN_MODE_LISTEN_ONLY;
	}
	if(mode->flags & GS_CAN_MODE_LOOP_BACK){
		can_mode |= CAN_MODE_LOOPBACK;
	}
	if(mode->flags & GS_CAN_MODE_ONE_SHOT){
		can_mode |= CAN_MODE_ONE_SHOT;
	}
	if(mode->flags & GS_CAN_MODE_FD){
		can_mode |= CAN_MODE_FD;
	}
	if(mode->flags & GS_CAN_MODE_BERR_REPORTING){
		can_mode |= CAN_MODE_BERR;
	}

	c->flags = mode->flags;
	c->started = true;

	if(!can_start(ch, can_mode)){
		gs_channel_reset(ch);
		return false;
	}
	led_channel_state(ch, LED_CH_OK);
	return true;
}

static bool gs_bittiming(const struct gs_device_bittiming *bt, struct can_timing_t *timing){
	/* Защита от переполнения uint16_t, точные границы проверяет can_set_timing() */
	if(bt->prop_seg > 1024 || bt->phase_seg1 > 1024 || bt->phase_seg2 > 1024 || bt->sjw > 1024 || bt->brp > 1024){
		return false;
	}
	timing->brp = bt->brp;
	timing->tseg1 = bt->prop_seg + bt->phase_seg1;
	timing->tseg2 = bt->phase_seg2;
	timing->sjw = bt->sjw;
	return true;
}

static bool gs_req_has_channel(uint8_t breq){
	switch(breq){
	case GS_USB_BREQ_BITTIMING:
	case GS_USB_BREQ_MODE:
	case GS_USB_BREQ_BERR:
	case GS_USB_BREQ_BT_CONST:
	case GS_USB_BREQ_IDENTIFY:
	case GS_USB_BREQ_DATA_BITTIMING:
	case GS_USB_BREQ_BT_CONST_EXT:
	case GS_USB_BREQ_GET_STATE:
		return true;
	default:
		return false;
	}
}

static bool gs_ctrl_setup(uint8_t rhport, tusb_control_request_t const *req){
	const uint8_t ch = (uint8_t)req->wValue;

	if(gs_req_has_channel(req->bRequest) && req->wValue >= CAN_CH_COUNT){
		return false;
	}

	if(req->bmRequestType_bit.direction == TUSB_DIR_OUT){
		switch(req->bRequest){
		case GS_USB_BREQ_HOST_FORMAT:
		case GS_USB_BREQ_BITTIMING:
		case GS_USB_BREQ_MODE:
		case GS_USB_BREQ_BERR:
		case GS_USB_BREQ_IDENTIFY:
		case GS_USB_BREQ_DATA_BITTIMING:
			break;
		default:
			return false;
		}
		if(req->wLength > sizeof(ctrl_buf)){
			return false;
		}
		return tud_control_xfer(rhport, req, ctrl_buf, req->wLength);
	}

	uint16_t len;
	memset(ctrl_buf, 0, sizeof(ctrl_buf));

	switch(req->bRequest){
	case GS_USB_BREQ_DEVICE_CONFIG:{
		struct gs_device_config *dc = (struct gs_device_config *)ctrl_buf;
		dc->icount = CAN_CH_COUNT - 1;
		dc->sw_version = GS_USB_SW_VERSION;
		dc->hw_version = GS_USB_HW_VERSION;
		len = sizeof(*dc);
		break;
	}
	case GS_USB_BREQ_BT_CONST:{
		struct gs_device_bt_const *bc = (struct gs_device_bt_const *)ctrl_buf;
		bc->feature = GS_FEATURES;
		bc->fclk_can = FDCAN_CLK_FREQ;
		bc->tseg1_min = CAN_NBT_TSEG1_MIN;
		bc->tseg1_max = CAN_NBT_TSEG1_MAX;
		bc->tseg2_min = CAN_NBT_TSEG2_MIN;
		bc->tseg2_max = CAN_NBT_TSEG2_MAX;
		bc->sjw_max = CAN_NBT_SJW_MAX;
		bc->brp_min = CAN_NBT_BRP_MIN;
		bc->brp_max = CAN_NBT_BRP_MAX;
		bc->brp_inc = 1;
		len = sizeof(*bc);
		break;
	}
	case GS_USB_BREQ_BT_CONST_EXT:{
		struct gs_device_bt_const_extended *bc = (struct gs_device_bt_const_extended *)ctrl_buf;
		bc->feature = GS_FEATURES;
		bc->fclk_can = FDCAN_CLK_FREQ;
		bc->tseg1_min = CAN_NBT_TSEG1_MIN;
		bc->tseg1_max = CAN_NBT_TSEG1_MAX;
		bc->tseg2_min = CAN_NBT_TSEG2_MIN;
		bc->tseg2_max = CAN_NBT_TSEG2_MAX;
		bc->sjw_max = CAN_NBT_SJW_MAX;
		bc->brp_min = CAN_NBT_BRP_MIN;
		bc->brp_max = CAN_NBT_BRP_MAX;
		bc->brp_inc = 1;
		bc->dtseg1_min = CAN_DBT_TSEG1_MIN;
		bc->dtseg1_max = CAN_DBT_TSEG1_MAX;
		bc->dtseg2_min = CAN_DBT_TSEG2_MIN;
		bc->dtseg2_max = CAN_DBT_TSEG2_MAX;
		bc->dsjw_max = CAN_DBT_SJW_MAX;
		bc->dbrp_min = CAN_DBT_BRP_MIN;
		bc->dbrp_max = CAN_DBT_BRP_MAX;
		bc->dbrp_inc = 1;
		len = sizeof(*bc);
		break;
	}
	case GS_USB_BREQ_TIMESTAMP:{
		uint32_t ts = systime_us();
		memcpy(ctrl_buf, &ts, sizeof(ts));
		len = sizeof(ts);
		break;
	}
	case GS_USB_BREQ_GET_STATE:{
		struct gs_device_state *st = (struct gs_device_state *)ctrl_buf;
		uint8_t tec, rec;
		/* enum can_state_t совпадает с enum gs_can_state */
		st->state = can_get_state(ch, &tec, &rec);
		st->txerr = tec;
		st->rxerr = rec;
		len = sizeof(*st);
		break;
	}
	default:
		return false;
	}

	return tud_control_xfer(rhport, req, ctrl_buf, TU_MIN(len, req->wLength));
}

static bool gs_ctrl_data(tusb_control_request_t const *req){
	const uint8_t ch = (uint8_t)req->wValue;

	switch(req->bRequest){
	case GS_USB_BREQ_HOST_FORMAT:
	case GS_USB_BREQ_BERR:
		return true;

	case GS_USB_BREQ_BITTIMING:
	case GS_USB_BREQ_DATA_BITTIMING:{
		struct gs_device_bittiming bt;
		struct can_timing_t timing;
		if(req->wLength < sizeof(bt)){
			return false;
		}
		memcpy(&bt, ctrl_buf, sizeof(bt));
		if(!gs_bittiming(&bt, &timing)){
			return false;
		}
		if(req->bRequest == GS_USB_BREQ_BITTIMING){
			return can_set_timing(ch, &timing);
		}
		return can_set_data_timing(ch, &timing);
	}

	case GS_USB_BREQ_MODE:{
		struct gs_device_mode mode;
		if(req->wLength < sizeof(mode)){
			return false;
		}
		memcpy(&mode, ctrl_buf, sizeof(mode));
		return gs_set_mode(ch, &mode);
	}

	case GS_USB_BREQ_IDENTIFY:{
		struct gs_identify_mode im;
		if(req->wLength < sizeof(im)){
			return false;
		}
		memcpy(&im, ctrl_buf, sizeof(im));
		led_identify(ch, im.mode != 0);
		return true;
	}

	default:
		return false;
	}
}

/* Все вендорские запросы TinyUSB передаёт сюда, минуя драйвер класса */
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request){
	if(request->bmRequestType_bit.recipient != TUSB_REQ_RCPT_INTERFACE ||
	   tu_u16_low(request->wIndex) != GS_USB_ITF_NUM){
		return false;
	}

	switch(stage){
	case CONTROL_STAGE_SETUP:
		return gs_ctrl_setup(rhport, request);
	case CONTROL_STAGE_DATA:
		/* Стадия DATA приходит и для IN запросов: данные уже отправлены, false даст STALL статуса */
		if(request->bmRequestType_bit.direction == TUSB_DIR_IN){
			return true;
		}
		return gs_ctrl_data(request);
	default:
		return true;
	}
}

/*********************** Драйвер класса TinyUSB ***********************/

static void gs_drv_init(void){
	for(uint8_t ch = 0; ch < CAN_CH_COUNT; ch++){
		gs_channel_reset(ch);
	}
	ring_tail = ring_head;
}

static bool gs_drv_deinit(void){
	return true;
}

static void gs_drv_reset(uint8_t UNUSED(rhport)){
	for(uint8_t ch = 0; ch < CAN_CH_COUNT; ch++){
		gs_channel_reset(ch);
		led_identify(ch, false);
	}
	/* Каналы остановлены, прерывания FDCAN в кольцо больше не пишут */
	ring_tail = ring_head;

	ep_in = 0;
	ep_out = 0;
	in_busy = false;
	out_pending = false;
}

static uint16_t gs_drv_open(uint8_t rhport, tusb_desc_interface_t const *desc_itf, uint16_t max_len){
	TU_VERIFY(desc_itf->bInterfaceClass == TUSB_CLASS_VENDOR_SPECIFIC && desc_itf->bInterfaceNumber == GS_USB_ITF_NUM, 0);
	TU_VERIFY(desc_itf->bNumEndpoints == 2, 0);

	const uint16_t len = sizeof(tusb_desc_interface_t) + 2 * sizeof(tusb_desc_endpoint_t);
	TU_VERIFY(max_len >= len, 0);

	TU_ASSERT(usbd_open_edpt_pair(rhport, tu_desc_next(desc_itf), 2, TUSB_XFER_BULK, &ep_out, &ep_in), 0);

	in_busy = false;
	out_pending = false;
	gs_out_arm();

	DEBUG("gs_usb open, EP IN 0x%02X OUT 0x%02X", ep_in, ep_out);
	return len;
}

static bool gs_drv_control_xfer_cb(uint8_t UNUSED(rhport), uint8_t UNUSED(stage), tusb_control_request_t const *UNUSED(request)){
	return false;
}

static bool gs_drv_xfer_cb(uint8_t UNUSED(rhport), uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes){
	if(ep_addr == ep_out){
		if(result == XFER_RESULT_SUCCESS){
			out_len = xferred_bytes;
			out_pending = true;
			gs_out_process();
		}else{
			gs_out_arm();
		}
	}else if(ep_addr == ep_in){
		ring_tail++;
		in_busy = false;
		gs_in_send();
	}
	return true;
}

static const usbd_class_driver_t gs_driver = {
	.name = "gs_usb",
	.init = gs_drv_init,
	.deinit = gs_drv_deinit,
	.reset = gs_drv_reset,
	.open = gs_drv_open,
	.control_xfer_cb = gs_drv_control_xfer_cb,
	.xfer_cb = gs_drv_xfer_cb,
	.xfer_isr = NULL,
	.sof = NULL
};

usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count){
	*driver_count = 1;
	return &gs_driver;
}
