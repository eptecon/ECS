#include <string.h>
#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"

#include "app_spi.h"

#include "driver/spi_master.h"

#include "soc/gpio_struct.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "spi_protocol_definitions.h"
#include "spi_protocol_crc8.h"

#define PIN_NUM_MISO 25
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  19
#define PIN_NUM_CS   22

#define PIN_NUM_DC   21
#define PIN_NUM_RST  18
#define PIN_NUM_BCKL 5

#define APP_SPI_DBG_LINE() printf("%s: %d\n", __FILE__, __LINE__);
#define APP_SPI_DBG_RC(rc) printf("rc = 0x%02X\n");

static const char *tag = "spi_master";

static spi_device_handle_t spi = NULL;


uint8_t miso_data[1024];
uint8_t mosi_data[128];

//This function is called (in irq context!) just before a transmission starts
//static void spi_pre_transfer_callback(spi_transaction_t *t) { 
//	gpio_set_level(PIN_NUM_DC, 0);
//}

//static void spi_post_transfer_callback(spi_transaction_t *t) { 
//	gpio_set_level(PIN_NUM_DC, 1);
//}

enum {
	TEMPERATURE_SENSOR_1 = 0,
	TEMPERATURE_SENSOR_2
};

esp_err_t app_spi_init(void) {

	esp_err_t ret;

	spi_bus_config_t buscfg = {
		.miso_io_num=PIN_NUM_MISO,
		.mosi_io_num=PIN_NUM_MOSI,
		.sclk_io_num=PIN_NUM_CLK,
		.quadwp_io_num=-1,
		.quadhd_io_num=-1
	};

	spi_device_interface_config_t devcfg = {
		.clock_speed_hz=187500,
		.mode=0,
		.spics_io_num=PIN_NUM_CS,
		.queue_size=1,
	};
	
	ESP_ERROR_CHECK(gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT));
	ESP_ERROR_CHECK(ret = spi_bus_initialize(HSPI_HOST, &buscfg, 1));
	ESP_ERROR_CHECK(ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spi));

	return ret;
}

esp_err_t app_spi_write(uint8_t *data, size_t length) {
	esp_err_t ret;
	spi_transaction_t t;

	uint8_t miso_data[8];
	memset(miso_data, 0, sizeof(miso_data));

	memset(&t, 0, sizeof(t));
	t.length = length;
	t.rxlength = 0;
	t.tx_buffer = data;
	t.rx_buffer = miso_data;
	t.user = (void*)(NULL);


	if (spi == NULL) {
		ESP_LOGE(tag, "SPI device not initialized");
		return ESP_ERR_INVALID_STATE;
	}

	ESP_ERROR_CHECK(ret = spi_device_transmit(spi, &t));

	return ret;
}

esp_err_t app_spi_write_read(uint8_t *data, size_t length, uint8_t *out_buf, size_t rx_length) {

	esp_err_t ret;
	spi_transaction_t t;

	memset(&t, 0, sizeof(t));
	t.length = length;
	t.rxlength = rx_length;
	t.tx_buffer = data;
	t.rx_buffer = out_buf;
	t.user = (void*)(NULL);

	if (spi == NULL) {
		ESP_LOGE(tag, "SPI device not initialized");
		return ESP_ERR_INVALID_STATE;
	}
	
	ESP_ERROR_CHECK(ret = spi_device_transmit(spi, &t));

	return ret;
}

static esp_err_t app_spi_get_response(uint8_t cmd, size_t *out_size) {

	uint16_t data_len, left_to_read = 0;
	uint16_t miso_index = 0;

	memset(miso_data, 0, sizeof(miso_data));

	/*
	 * Yes, it's an odd byte. Somehow only this way it works.
	 * TODO: make an STM32 code to work without it.
	 */
	uint8_t dummy = 0;
	app_spi_write_read(&dummy, 8 * 1, NULL, 0);

	/*
	 * Read first response bytes (start byte, command and len)
	 * and check it.
	 */
	app_spi_write_read(NULL, 8 * 3, miso_data, 0);

	/*
	 * [0] start byte (0x7e)
	 * [1] command
	 * [2] data length
	 */
	if (	miso_data[miso_index++] != START_BYTE || 	// miso_index = 1
		miso_data[miso_index++] != cmd + 1 ||		// miso_index = 2
		miso_data[miso_index] == 0)			// miso_index = 2

		return ESP_ERR_INVALID_RESPONSE;

	/*
	 * Read data length
	 */
	data_len = miso_data[miso_index++];			// miso_index = 3
	if (data_len == 0)
		return ESP_ERR_INVALID_RESPONSE;

	app_spi_write_read(NULL, (data_len + 2) * 8, miso_data + miso_index, 0); //read data + crc + stop byte

	miso_index += data_len;

	/*
	 * Remaining fields check
	 */
	if (miso_data[miso_index++] != spi_protocol_crc8(&miso_data[1], data_len + 2))
		return ESP_ERR_INVALID_CRC;

	if (miso_data[miso_index] != STOP_BYTE)
		return ESP_ERR_INVALID_RESPONSE;

	*out_size = data_len - 2;

	return ESP_OK;
}

esp_err_t app_spi_rfid_scan(uint8_t rfid[64]) {

	esp_err_t rc;
	uint8_t out_buf[66];
	size_t size;

	/*
	 * Compose request
	 */
	uint8_t req_buf[] = {
		START_BYTE,
		RFID_REQ,
		0x00, 		// data item
		0x00,		// place for crc8
		STOP_BYTE};

	req_buf[3] = spi_protocol_crc8(&req_buf[1], 3);

	/*
	 * Send request
	 */
	app_spi_write_read(req_buf, 5 * 8, NULL, 0);

	/*
	 * Give a time to our lil bro
	 */
	vTaskDelay(400 / portTICK_RATE_MS);

	rc = app_spi_get_response(RFID_REQ, &size);

	if (rc != ESP_OK)
		return rc;

	if (miso_data[3] != RFID_SCAN)
		return ESP_ERR_INVALID_RESPONSE;

	//only 64 bytes are allowed
	if (miso_data[4] != 0x40)
		return ESP_ERR_INVALID_RESPONSE;

	memcpy(rfid, &miso_data[5], 64);
	
	return ESP_OK;
}

esp_err_t app_spi_device_ctrl_remote_rfid(const uint8_t remote_rfid[64], uint8_t *ret_state) {

	esp_err_t rc;

	uint8_t out_buf[8];
	size_t size;

	/*
	 * Compose request
	 */
	uint8_t req_buf[] = {
		START_BYTE,
		DEVICE_CTRL_REQ,
		REMOTE_RFID,
		[3 ... 66] = 0x00,
		0x00, // place for crc8
		STOP_BYTE};

	memcpy(&req_buf[3], remote_rfid, 64);
	
	req_buf[67] = spi_protocol_crc8(&req_buf[1], 64 + 3);

	/*
	 * Send request
	 */
	app_spi_write_read(req_buf, 8 * 69, NULL, 0);

	/*
	 * Give a time to our lil bro
	 */
	vTaskDelay(400 / portTICK_RATE_MS);

	rc = app_spi_get_response(DEVICE_CTRL_REQ, &size);

	if (rc != ESP_OK)
		return rc;

	/*
	 * Check the data fields in response
	 */
	if (	miso_data[3] != REMOTE_RFID ||
		miso_data[4] != 0x01)

		return ESP_ERR_INVALID_RESPONSE;

	*ret_state = *(uint8_t*)(&out_buf[5]);

	return ESP_OK;
}

#if 0
esp_err_t app_spi_device_info_req(uint8_t *out_buf, size_t *size) {

	/*
	 * Compose request
	 */
	uint8_t req_buf[] = {
		START_BYTE,
		DEVICE_INFO_REQ,
		0x00,		// data item
		0x00,		// place for crc8
		STOP_BYTE};

	req_buf[3] = spi_protocol_crc8(&req_buf[1], 3);

	/*
	 * Send request
	 */
	app_spi_write_read(req_buf, 8 * 5, NULL, 0);

	/*
	 * Give a time to our lil bro
	 */
	vTaskDelay(400 / portTICK_RATE_MS);

	if ( (rc = app_spi_get_response(DEVICE_INFO_REQ, size)) != ESP_OK)
		return rc;

}
#endif

esp_err_t app_spi_device_ctrl_req(uint8_t item, uint8_t *out_buf, size_t *size) {

	esp_err_t rc;

	/* 
	 * parameter check
	 */
	switch(item) {
	
	case DEVICE_NAME:
	case RFID_SCAN:
	case ERROR_CODE:
	case TOT_OP_TIME:
	case DEVICE_FW_VERSION:	
	case DEVICE_STATUS:	
	case SWITCH_1_STATUS:	
	case SWITCH_2_STATUS:	
	case SWITCH_3_STATUS:	
	case SWITCH_4_STATUS:	
	case SWITCH_5_STATUS:	
	case TEMP_SENSOR_1:	
	case TEMP_SENSOR_2:	
	case RESERVED_1:
	case RESERVED_2:
	case MOTOR_ROT_SPEED:	
	case DEVICE_OFF:	
	case DEVICE_ON:	
	case DEVICE_START:
       		break;
	default:
		return ESP_ERR_INVALID_ARG;
	}

	/*
	 * Compose request
	 */
	uint8_t req_buf[] = {
		START_BYTE,
		DEVICE_CTRL_REQ,
		item,
		0x00, // place for crc8
		STOP_BYTE};

	req_buf[3] = spi_protocol_crc8(&req_buf[1], 3);
	
	/*
	 * Send request
	 */
	app_spi_write_read(req_buf, 8 * 5, NULL, 0);

	/*
	 * Give a time to our lil bro
	 */
	vTaskDelay(400 / portTICK_RATE_MS);

	if ( (rc = app_spi_get_response(DEVICE_CTRL_REQ, size)) != ESP_OK)
		return rc;

	if (miso_data[3] != item)
		return ESP_ERR_INVALID_RESPONSE;

	memcpy(out_buf, &miso_data[5], *size);

	return ESP_OK;
}

void app_spi_test(void) {
	uint8_t buf[255];
	size_t size;

	uint8_t state = 0;

	uint8_t rfid[64];
	int i = 0;

	app_spi_init();

	memset(buf, 0, sizeof(buf));

	ESP_ERROR_CHECK(app_spi_device_ctrl_req(DEVICE_NAME, buf, &size));
	printf("Device name: %.*s\n", size, (char *)buf);
	
	ESP_ERROR_CHECK(app_spi_device_ctrl_req(DEVICE_FW_VERSION, buf, &size));
	printf("Device fw version: %.*s\n", size, (char*)buf);

	ESP_ERROR_CHECK(app_spi_device_ctrl_req(ERROR_CODE, buf, &size));
	printf("Device error code: %02X %02X %02X %02X %02X %02X\n", 
			buf[0],buf[1],buf[2],buf[3],buf[4],buf[5]);

	ESP_ERROR_CHECK(app_spi_device_ctrl_req(TOT_OP_TIME, buf, &size));
	printf("Device total operating time: %d monthes %d weeks %d days %d hours %d minutes %d seconds\n", 
			buf[0],buf[1],buf[2],buf[3],buf[4],buf[5]);

	ESP_ERROR_CHECK(app_spi_rfid_scan(rfid));
	printf("Scanned rfid:\n");

	for (i = 0; i < 64; i++) {
		if (i % 8 == 0)
			printf("\n");
		printf("[0x%02X] ", rfid[i]);
	}
	printf("\n");


	vTaskDelay(500 / portTICK_RATE_MS);


	ESP_ERROR_CHECK(app_spi_device_ctrl_req(TEMP_SENSOR_1, buf, &size));

	printf("temp sensor 1: %f\n", *(float*)buf);

	vTaskDelay(500 / portTICK_RATE_MS);

	
	ESP_ERROR_CHECK(app_spi_device_ctrl_req(TEMP_SENSOR_2, buf, &size));

	printf("temp sensor 2: %f\n", *(float*)buf);


	printf("Switch 1 status: ");
	vTaskDelay(1000 / portTICK_RATE_MS);
	ESP_ERROR_CHECK(app_spi_device_ctrl_req(SWITCH_1_STATUS, buf, &size));
	printf("%s\n", *(uint8_t*)buf == 0x01 ? "closed" : "opened");


	printf("Switch 2 status: ");
	vTaskDelay(1000 / portTICK_RATE_MS);
	ESP_ERROR_CHECK(app_spi_device_ctrl_req(SWITCH_2_STATUS, buf, &size));
	printf("%s\n", *(uint8_t*)buf == 0x01 ? "closed" : "opened");

	printf("Switch 3 status: ");
	vTaskDelay(1000 / portTICK_RATE_MS);
	ESP_ERROR_CHECK(app_spi_device_ctrl_req(SWITCH_3_STATUS, buf, &size));
	printf("%s\n", *(uint8_t*)buf == 0x01 ? "closed" : "opened");

	printf("Switch 4 status: ");
	vTaskDelay(1000 / portTICK_RATE_MS);
	ESP_ERROR_CHECK(app_spi_device_ctrl_req(SWITCH_4_STATUS, buf, &size));
	printf("%s\n", *(uint8_t*)buf == 0x01 ? "closed" : "opened");

	printf("Switch 5 status: ");
	vTaskDelay(1000 / portTICK_RATE_MS);
	ESP_ERROR_CHECK(app_spi_device_ctrl_req(SWITCH_5_STATUS, buf, &size));
	printf("%s\n", *(uint8_t*)buf == 0x01 ? "closed" : "opened");

	vTaskDelay(500 / portTICK_RATE_MS);
	ESP_ERROR_CHECK(app_spi_device_ctrl_req(MOTOR_ROT_SPEED, buf, &size));
	printf("Motor rotating speed: %f\n", *(float*)buf);


	vTaskDelay(2000 / portTICK_RATE_MS);
	ESP_ERROR_CHECK(app_spi_device_ctrl_req(DEVICE_OFF, buf, &size));
	printf("%s\n", *(uint8_t*)buf == 0x01 ? "Device turned off" : "can't turn off device");


	vTaskDelay(1000 / portTICK_RATE_MS);
	ESP_ERROR_CHECK(app_spi_device_ctrl_req(DEVICE_STATUS, buf, &size));
	printf("%s\n", *(uint8_t*)buf == 0x01 ? "Device state: on" : "Device state: off");


	vTaskDelay(2000 / portTICK_RATE_MS);
	ESP_ERROR_CHECK(app_spi_device_ctrl_req(DEVICE_ON, buf, &size));
	printf("%s\n", *(uint8_t*)buf == 0x01 ? "Device turned on" : "can't turn on device");

	vTaskDelay(1000 / portTICK_RATE_MS);
	ESP_ERROR_CHECK(app_spi_device_ctrl_req(DEVICE_STATUS, buf, &size));
	printf("%s\n", *(uint8_t*)buf == 0x01 ? "Device state: on" : "Device state: off");

	vTaskDelay(1000 / portTICK_RATE_MS);

	memset(rfid, 0xa5, 64);
	printf("RFID 0xa5: ");
	ESP_ERROR_CHECK(app_spi_device_ctrl_remote_rfid(rfid, &state));
	printf("successfull\n");

	vTaskDelay(500 / portTICK_RATE_MS);

	memset(rfid, 0x5a, 64);
	printf("RFID 0x5a: ");
	ESP_ERROR_CHECK(app_spi_device_ctrl_remote_rfid(rfid, &state));
	printf("successfull\n");


	while (true);
}


