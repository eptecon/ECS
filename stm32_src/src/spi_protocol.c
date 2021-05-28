#include <string.h>
#include <stdbool.h>

#include "main.h"
#include "spi.h"
#include "spi_protocol.h"
#include "stm32f0xx_spi.h"

#include "ringbuf.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "spi_protocol_definitions.h"
#include "spi_protocol_slave_internals.h"
#include "spi_protocol_handlers.h"
#include "spi_protocol_crc8.h"

#include "tinyprintf.h"

/*
 * Receive ring buffer size
 */
#define SPI_RX_RINGBUF_DATA_SIZE 256


#define CC_ACCESS_NOW(type, variable) (*(volatile type *)&(variable))

/*
 * Ringbuf definitions
 */
static struct ringbuf spi_rx_ringbuf;
static uint8_t spi_rx_ringbuf_data[SPI_RX_RINGBUF_DATA_SIZE];

static TaskHandle_t spi_task_handle;

/*
 * Semaphore is used to sync spi task and IRQ.
 * Initialized as a counting semaphore with a zero initial count value
 * with portMAX_DELAY as wait time.
 *
 * Used in spi_read_byte_from_buffer and spi_buffer_mosi.
 *
 * When spi task that tries to read the next byte is unavailable to 
 * take this semaphore due to zero resources available, it suspends (takes off from scheduling).
 * When a new byte is arrived, it gives a semaphore and task reads the new delivered byte.
 *
 * TODO switch to statically allocated semaphore
 */
static SemaphoreHandle_t spi_rx_semphr = NULL;


static spi_slave_state_t slave_state = SPI_SLAVE_RECEIVE;

static spi_prot_state_slave_t spi_prot_state;

static uint8_t rq_data[SPI_PROT_RQ_BUF_LEN];
static uint8_t resp_buf[SPI_PROT_RESP_BUF_LEN];

extern struct spi_handlers_str spi_handlers;

static spi_prot_slave_ctx_t spi_prot_ctx = {
	.rq_data = rq_data,
	.resp_buf = resp_buf,
	.rq_index = 0,
	.rq_len = 0,
	.resp_index = 0,
	.resp_len = 0,
};

void spi_task_init(void) {

#ifndef SPI_PROT_TEST
	spi_init();
#endif 
	ringbuf_init(&spi_rx_ringbuf, spi_rx_ringbuf_data, (uint8_t)sizeof(spi_rx_ringbuf_data));
	spi_rx_semphr = xSemaphoreCreateCounting(SPI_RX_RINGBUF_DATA_SIZE, 0);

#ifdef SPI_PROT_TEST
	xTaskCreate(spi_irq_emulation_task,
			"spi_irq_task",
			configMINIMAL_STACK_SIZE,
			NULL,
			tskIDLE_PRIORITY + 2,
			NULL);

#endif /* SPI_PROT_TEST */

	xTaskCreate(spi_task,
                "spi_task",
                configMINIMAL_STACK_SIZE,
                NULL,                 /* pvParameters */
                tskIDLE_PRIORITY + 1, /* uxPriority */
                &spi_task_handle      /* pvCreatedTask */);
}

void spi_task(void *arg) {
	(void)arg;

	uint8_t b = 0;
	int ret;

	while (true) {
		while ((ret = spi_read_byte_from_buffer(&b) == 0)) {
			spi_protocol_handler(b);
		}
	}

#if 0
	while (1) {

		vTaskDelay(50 / portTICK_PERIOD_MS);

		while ((ret = spi_read_byte_from_buffer(&b) != -1)) {
			spi_protocol_handler(b);
		}

		spi_clear_rx_rbuf_err();
    	}
#endif
}

void spi_protocol_handler(uint8_t byte) {

	PRINT_CURRENT_STATE();
	
	switch(spi_prot_state) {

	case SPI_PROT_STATE_IDLE:
		PRINT_LINE();
		STATE_MACHINE_PRINT_BYTE();
		if (byte == START_BYTE) {
			spi_prot_state = SPI_PROT_COMMAND;

			memset(spi_prot_ctx.rq_data, 0, SPI_PROT_RQ_BUF_LEN);
			spi_prot_ctx.rq_len = 0;
			spi_prot_ctx.rq_index = 0;
		}
		break;

	case SPI_PROT_COMMAND:
		PRINT_LINE();
		STATE_MACHINE_PRINT_BYTE();
		spi_prot_ctx.rq_data[spi_prot_ctx.rq_index++] = byte;
		spi_prot_state = SPI_PROT_DATA_ITEM;
		break;
	
	case SPI_PROT_DATA_ITEM:
		PRINT_LINE();
		STATE_MACHINE_PRINT_BYTE();
		spi_prot_ctx.rq_data[spi_prot_ctx.rq_index++] = byte;

		switch (byte) {
		case REMOTE_RFID:
			PRINT_LINE();
			spi_prot_state = SPI_PROT_DATA;
			/* We should expect for 64 bytes of RFID + start byte + comand byte */
			spi_prot_ctx.rq_len = 64 + 2;
			break;
		default:
			PRINT_LINE();
			spi_prot_state = SPI_PROT_CHECKSUM;
			break;
		}

		break;

	case SPI_PROT_DATA:

		PRINT_LINE();
		STATE_MACHINE_PRINT_BYTE();

		spi_prot_ctx.rq_data[spi_prot_ctx.rq_index++] = byte;

		if (spi_prot_ctx.rq_index == spi_prot_ctx.rq_len)
			spi_prot_state = SPI_PROT_CHECKSUM;
		break;

	case SPI_PROT_CHECKSUM:

		PRINT_LINE();
		STATE_MACHINE_PRINT_BYTE();

		spi_prot_ctx.checksum = byte;
		spi_prot_state = SPI_PROT_STOP;

		break;

	case SPI_PROT_STOP:
		PRINT_LINE();
		STATE_MACHINE_PRINT_BYTE();

		uint8_t crc_calculated = spi_protocol_crc8(&spi_prot_ctx.rq_data[0], spi_prot_ctx.rq_index + 1);

		if (byte == STOP_BYTE) {
			if (spi_prot_ctx.checksum == crc_calculated) {
				PRINT_LINE();
				spi_protocol_bufferize_response(&spi_prot_ctx);
				PRINT_LINE();
				spi_switch_state(SPI_SLAVE_RSP);
			}
			else {
				PRINT_LINE();
				spi_prot_state = SPI_PROT_STATE_IDLE;
			}
		}

		else spi_prot_state = SPI_PROT_STATE_IDLE;

		break;
	}
}

void spi_protocol_bufferize_response(spi_prot_slave_ctx_t *ctx) {
	PRINT_LINE();
	LIST_REQ();

	int data_len = 0;

	uint8_t command = ctx->rq_data[0];
	uint8_t data_item = ctx->rq_data[1];

	uint16_t i = 0; //response buffer index

	memset(ctx->resp_buf, 0, SPI_PROT_RESP_BUF_LEN);
	ctx->resp_index = 0;

	ctx->resp_buf[i++] = START_BYTE;

	switch (command) {

	case DEVICE_INFO_REQ:
		PRINT_LINE();
		ctx->resp_buf[i++] = DEVICE_INFO_RES;
		break;




	case RFID_REQ:
		PRINT_LINE();
		ctx->resp_buf[i++] = RFID_RES;
		ctx->resp_buf[i++] = 0x42;
		ctx->resp_buf[i++] = RFID_SCAN;
		ctx->resp_buf[i++] = 0x40;

		spi_handlers.scan_rfid_cb(&(ctx->resp_buf[i]));

		i += 64;

		ctx->resp_buf[i++] = spi_protocol_crc8(&(ctx->resp_buf[1]), 68);

		break;




	case DEVICE_CTRL_REQ:


		PRINT_LINE();
		ctx->resp_buf[i++] = DEVICE_CTRL_RES;

		switch (data_item) {
		case REMOTE_RFID:
			PRINT_LINE();
			ctx->resp_buf[i++] = 0x03; // data length
			ctx->resp_buf[i++] = REMOTE_RFID; //item
			ctx->resp_buf[i++] = 0x01;  // item length
			ctx->resp_buf[i++] = 0x01; // item state TODO: get actual state
			ctx->resp_buf[i++] = spi_protocol_crc8(&(ctx->resp_buf[1]), 5);

			spi_handlers.set_rfid_cb(spi_prot_ctx.rq_data + 2);

			break;

		default:
			PRINT_LINE();

			data_len = spi_protocol_get_data_item(data_item, &ctx->resp_buf[i + 3]);

			ctx->resp_buf[i++] = data_len + 2; //data_length (plus item and item length fields)
			ctx->resp_buf[i++] = data_item; // item
			ctx->resp_buf[i++] = data_len; //item length

			//...item data

			ctx->resp_buf[data_len + i] = spi_protocol_crc8(&(ctx->resp_buf[1]), data_len + i - 1);

			i += data_len + 1;

			break;
		}
		break;
	}

	ctx->resp_buf[i++] = STOP_BYTE;
	ctx->resp_len = i;
}

static void spi_protocol_slave_do_response(void) {

	spi_write_byte( spi_prot_ctx.resp_buf[spi_prot_ctx.resp_index++] );

	if (spi_prot_ctx.resp_index == spi_prot_ctx.resp_len) {
		spi_switch_state(SPI_SLAVE_RECEIVE);
		spi_prot_state = SPI_PROT_STATE_IDLE;
		spi_prot_ctx.resp_index = 0;
		spi_prot_ctx.resp_len = 0;
	}
}

int spi_read_byte_from_buffer(uint8_t *data) {

	xSemaphoreTake(spi_rx_semphr, portMAX_DELAY);

	if (( *data = ringbuf_get(&spi_rx_ringbuf)) == -1) {
		return -1;
	}

	return 0;
}

void spi_buffer_mosi(uint8_t byte) {

	ringbuf_put(&spi_rx_ringbuf, byte);

#ifdef SPI_PROT_TEST
	xSemaphoreGive(spi_rx_semphr);
#else
	xSemaphoreGiveFromISR(spi_rx_semphr, NULL);
#endif /* SPI_PROT_TEST */

#if 0
	BaseType_t xHigherPriorityTaskWoken;

#ifdef SPI_PROT_TEST
	xTaskNotifyGive(spi_task_handle);
#else
	vTaskNotifyGiveFromISR( spi_task_handle, &xHigherPriorityTaskWoken );

	/* Force a context switch if xHigherPriorityTaskWoken is now set to pdTRUE.
	*     The macro used to do this is dependent on the port and may be called
	*         portEND_SWITCHING_ISR. */
	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
#endif
#endif
}


extern void spi_rx_irq_handler(void) {
		uint8_t b;
		switch(slave_state) {

		case SPI_SLAVE_RECEIVE:
			b = SPI_ReceiveData8(SPI2);
			spi_buffer_mosi(b);
			break;

		case SPI_SLAVE_RSP:
			spi_protocol_slave_do_response();
			break;
		}
}



void spi_switch_state(spi_slave_state_t state) {
	slave_state = state;
}

spi_slave_state_t spi_slave_get_state(void) {
	return slave_state;
}

#ifdef SPI_PROT_DBG

uint8_t mosi_emulation_data[256];

void decode_state(spi_prot_state_slave_t state, char *out_str) {
	switch(state) {
	case SPI_PROT_STATE_IDLE:
		strcpy(out_str, "SPI_PROT_STATE_IDLE");
		break;
	case SPI_PROT_COMMAND:
		strcpy(out_str, "SPI_PROT_COMMAND");
		break;
	case SPI_PROT_DATA_ITEM:
		strcpy(out_str, "SPI_PROT_DATA_ITEM");
		break;
	case SPI_PROT_DATA:
		strcpy(out_str, "SPI_PROT_DATA");
		break;
	case SPI_PROT_CHECKSUM:
		strcpy(out_str, "SPI_PROT_CHECKSUM");
		break;
	case SPI_PROT_STOP:
		strcpy(out_str, "SPI_PROT_STOP");
		break;
	}
}
#endif /* SPI_PROT_DBG */

#ifdef SPI_PROT_TEST
static void generate_request_rfid(void) {

	int i = 0;

	mosi_emulation_data[i++] = START_BYTE;
	mosi_emulation_data[i++] = DEVICE_CTRL_REQ;
	mosi_emulation_data[i++] = REMOTE_RFID;

	for (; i < 3 + 64; i++) {
		mosi_emulation_data[i] = i - 3;
	}
	uint8_t crc;

	crc = spi_protocol_crc8(&mosi_emulation_data[1], i);

	mosi_emulation_data[i++] = crc;

	mosi_emulation_data[i++] = STOP_BYTE;
}

static bool is_button_pressed = false;


void spi_irq_emulation_task(void *arg) {

	(void)arg;

	uint8_t i = 0, b;

	ringbuf_init(&spi_rx_ringbuf, spi_rx_ringbuf_data, sizeof(spi_rx_ringbuf_data));
	spi_rx_semphr = xSemaphoreCreateCounting(SPI_RX_RINGBUF_DATA_SIZE, 0);
	spi_is_rx_err_flag = false;



	generate_request_rfid();

	while (true) {

		vTaskDelay(20);

 		if (!is_button_pressed) {
			if (!STM_EVAL_PBGetState(BUTTON_USER))
				continue;
			else
				is_button_pressed = true;
		}


		spi_slave_state_t slave_state = spi_slave_get_state();

		switch(slave_state) {

		case SPI_SLAVE_RECEIVE:

			b = mosi_emulation_data[i++];
			spi_buffer_mosi(b);
			break;

		case SPI_SLAVE_RSP:
			spi_protocol_slave_do_response_emulation();
			i = 0;
			break;
		}
	}
}

void spi_protocol_slave_do_response_emulation(void) {

	printf("tx: 0x%02X\n", spi_prot_ctx.resp_buf[spi_prot_ctx.resp_index++]);

	if (spi_prot_ctx.resp_index == spi_prot_ctx.resp_len) {
		spi_switch_state(SPI_SLAVE_RECEIVE);
		spi_prot_state = SPI_PROT_STATE_IDLE;
		spi_prot_ctx.resp_index = 0;
		spi_prot_ctx.resp_len = 0;
		is_button_pressed = false;
	}
}


#endif /* SPI_PROT_TEST */
