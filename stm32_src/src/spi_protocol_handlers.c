#include <string.h>
#include <stdbool.h>

#include "main.h"

#include "FreeRTOS.h"
#include "task.h"

#include "tinyprintf.h"

#include "spi_protocol.h"
#include "spi_protocol_definitions.h"

#include "flash.h"

#include "rfid.h"

int rfid_scan(uint8_t rfid_scanned[64]);
int rfid_set(const uint8_t rfid[64]);
void power_on_handler(void);
void power_off_handler(void);
bool switch_handler(uint8_t switch_num);
void temperature_sensor_handler(uint8_t sensor_num, uint8_t temperature[4]);

struct spi_handlers_str spi_handlers = {
	.scan_rfid_cb = rfid_scan,
	.set_rfid_cb = rfid_set,
	.device_on_cb = power_on_handler,
	.device_off_cb = power_off_handler,
	.switch_handler_cb = switch_handler,
	.temperature_sensor_handler_cb = temperature_sensor_handler,
};


static struct {
	bool flag_reading;
	
	/* device info */
	char device_name[256];
	uint8_t rfid_scan[256];
	uint8_t error_code[6];
	uint8_t total_op_time[6];
	char fw_version[255];

	/* bool sensors */
	uint8_t device_status;
	uint8_t sw1_status;
	uint8_t sw2_status;
	uint8_t sw3_status;
	uint8_t sw4_status;
	uint8_t sw5_status;

	/* analog sensors */
	union {
		float f;
		uint8_t u8[4];
	} temperature_0, temperature_1;

	union {
		float f;
		uint8_t u8[4];
	} motor_rot_speed;

	/* controls */
	uint8_t device_off;
	uint8_t device_on;
	uint8_t device_start;
	uint8_t remote_rfid[64];

} item_data = {
	.total_op_time = {0x00, 0x00, 0x00, 0x00, 0x10, 0x00},
	.device_name = "STM32F0DISCOVERY",
	.error_code = {0xa5, 0x00, 0x5a, 0x00, 0x7e, 0xe7},
	.fw_version = "0.1.1",
	.device_status = 0x01,

	.flag_reading = false,
};




extern TaskHandle_t blinker_task_handle;


static union {
	float f;
	uint8_t u8[4];
} temperature_0, temperature_1;

int spi_protocol_get_data_item(uint8_t item, void *out_buf) {

	int len;

	item_data.flag_reading = true;

	switch (item) {

	case DEVICE_FW_VERSION:
		len = strlen(item_data.fw_version);
		memcpy(out_buf, item_data.fw_version, len);
		break;
	
	case DEVICE_NAME:
		len = strlen(item_data.device_name);
		memcpy(out_buf, item_data.device_name, len);
		break;
	
	case ERROR_CODE:
		len = 6; //only 6 bytes are allowed
		memcpy(out_buf, item_data.error_code, len);
		break;

	case TOT_OP_TIME:
		len = 6;
		memcpy(out_buf, item_data.total_op_time, len);
		break;

	case DEVICE_STATUS:
		((uint8_t *)out_buf)[0] = item_data.device_status;
		len = 1;
		break;

	case DEVICE_ON:
		((uint8_t *)out_buf)[0] = 0x01;
		spi_handlers.device_on_cb();
		len = 1;
		break;

	case DEVICE_OFF:
		((uint8_t *)out_buf)[0] = 0x01;
		spi_handlers.device_off_cb();
		len = 1;
		break;

	case SWITCH_1_STATUS:
		memcpy(out_buf, &item_data.sw1_status, 1);
		len = 1;
		break;


	case SWITCH_2_STATUS:
		memcpy(out_buf, &item_data.sw2_status, 1);
		len = 1;
		break;


	case SWITCH_3_STATUS:
		memcpy(out_buf, &item_data.sw3_status, 1);
		len = 1;
		break;


	case SWITCH_4_STATUS:
		memcpy(out_buf, &item_data.sw4_status, 1);
		len = 1;
		break;


	case SWITCH_5_STATUS:
		memcpy(out_buf, &item_data.sw5_status, 1);
		len = 1;
		break;
	
	case TEMP_SENSOR_1:
		memcpy(out_buf, item_data.temperature_0.u8, 4);
		len = 4;
		break;

	case TEMP_SENSOR_2:
		memcpy(out_buf, item_data.temperature_1.u8, 4);
		len = 4;
		break;

	case MOTOR_ROT_SPEED:
		memcpy(out_buf, item_data.motor_rot_speed.u8, 4);
		len = 4;
		break;

	default: 
		len = -1;
		break;
	}

	item_data.flag_reading = false;

	return len;
}


void power_on_handler(void) {
	vTaskResume(blinker_task_handle);
	item_data.device_status = 0x01;

	nv_blob_t *nv = flash_get_nv_blob();
	nv->device_state = 0x01;
	flash_write_nv_blob();
}

void power_off_handler(void) {
	vTaskSuspend(blinker_task_handle);
	STM_EVAL_LEDOff(LED3);
	item_data.device_status = 0x00;

	nv_blob_t *nv = flash_get_nv_blob();
	nv->device_state = 0x00;
	flash_write_nv_blob();
}

bool switch_handler(uint8_t switch_num) {
	(void)switch_num;
	return STM_EVAL_PBGetState(BUTTON_USER);
}

void temperature_sensor_handler(uint8_t sensor_num, uint8_t temperature[4]) {

	if (sensor_num == 0) {
		temperature_0.f += 0.5;
		memcpy(temperature, temperature_0.u8,4);
	}

	if (sensor_num == 1) {
		temperature_1.f += 0.5;
		memcpy(temperature, temperature_1.u8,4);
	}
}

void device_peripherals_poll_task(void *arg) {
	(void)arg;

	while (true) {
		vTaskDelay(1000 / portTICK_RATE_MS);

		while (item_data.flag_reading == 0x01);

		item_data.sw1_status = (uint8_t)STM_EVAL_PBGetState(BUTTON_USER);
		item_data.temperature_0.f += 0.5;
		item_data.temperature_1.f += 0.5;
		item_data.motor_rot_speed.f += 1;
	}
}

#if 0
int spi_protocol_write_data_item(spi_prot_data_item_t, const void *data) {

	if (item_data.flag_reading)
		return -1;

	switch (item) {
	
	case TEMPERATURE_0:
		return item_data.temperature_0.u8;
	case TEMPERATURE_1:
		return item_data.temperature_1.u8;
	}
	
}
#endif



