#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "aws_iot_shadow_interface.h"

#include "app_spi.h"
#include "app_aws_iot.h"

#include "spi_protocol_definitions.h"

#define ECS_IOT_ERR(x) 	do {								\
				IoT_Error_t err;					\
				if ((err = (x)) != SUCCESS) {				\
					ESP_LOGE(tag, "%s %s: %d, IoT rc = %d", 	\
						__FILE__, __FUNCTION__, __LINE__, err);	\
				}							\
			} while (0)

#define ECS_ESP_ERR(x) 	do {								\
				esp_err_t rc;						\
				if ((rc = (x)) != ESP_OK) {				\
					ESP_LOGE(tag, "%s %s: %d, ESP rc = 0x%02X",	\
						__FILE__, __FUNCTION__, __LINE__, rc);	\
				}							\
			} while (0)

#define ECS_CTRL_TASK_DELAY_MS 2000


static void aws_shadow_delta_cb(const char *pJsonValueBuffer, uint32_t valueLength, jsonStruct_t *pJsonStruct);

static const char *tag = "ecs_ctrl";

/*
 * TODO: refactor AWS IoT API to avoid such symbol throw
 */
extern SemaphoreHandle_t aws_mqtt_pub_mutex;

enum {
	JS_SWITCH_1_STATE = 0,
	JS_DEVICE_IS_ON,
	JS_TEMP_SENSOR_1,
	JS_TEMP_SENSOR_2,
	JS_MOTOR_ROT_SPEED,
	JS_REMOTE_RFID,
	JS_TOKEN_MAX
};

enum {
	TEMPERATURE_SENSOR_1_N = 0,
	TEMPERATURE_SENSOR_2_N
};

#pragma pack(push, 1)
/* 
 * The "reported" only section that is not
 * manageable from AWS cloud.
 *
 * The reason of "in_cloud" and "actual" split is
 * that we always need to compare this data to avoid:
 * 1. the constant pushing SPI device for "desired" AWS section
 * 2. the constant updating AWS shadow for "reported" AWS section.
 *
 * This way, the order of values update is following:
 * "reported":
 *	"actual" -> "in_cloud"
 * 
 * "desired":
 * 	"in_cloud" -> "actual"
 */
static struct {
	struct {
		float temperature_sensor_1,
			 temperature_sensor_2,
			 motor_rotating_speed;
	} analog_sensors;

	struct {
		uint8_t device_name[255];
		uint8_t rfid_scan[255];
		uint8_t error_code[6];
		uint8_t total_op_time[6];
		uint8_t fw_version[255];

	} device_info;

	struct {
		uint8_t device_status, 
		     	switch1_status,
		     	switch2_status,
		     	switch3_status,
		     	switch4_status,
		     	switch5_status;
	} boolean_sensors;
}

ecs_aws_reported_on_device,
ecs_aws_reported_in_cloud;

static struct {
	bool device_off;
	bool device_on;
	bool device_start;
	char remote_rfid[129];
} 

ecs_aws_desired_on_device,
ecs_aws_desired_in_cloud;

#define JSON_BUF_MAX_LEN 600
static char json_doc_buf[JSON_BUF_MAX_LEN];
#pragma pack(pop)

jsonStruct_t js_tokens[JS_TOKEN_MAX] = {

	[JS_SWITCH_1_STATE] = {
		.cb = aws_shadow_delta_cb,
		.pData =  &ecs_aws_reported_in_cloud.boolean_sensors.switch1_status,
		.pKey = "switch_1",
		.type = SHADOW_JSON_BOOL,
	},
	[JS_DEVICE_IS_ON] = {
		.cb = aws_shadow_delta_cb,
		.pData = &ecs_aws_desired_in_cloud.device_on,
		.pKey = "device_is_on",
		.type = SHADOW_JSON_BOOL,
	},
	[JS_TEMP_SENSOR_1] = {
		.cb = aws_shadow_delta_cb,
		.pData = &ecs_aws_reported_in_cloud.analog_sensors.temperature_sensor_1,
		.pKey = "temp_sensor_1",
		.type = SHADOW_JSON_FLOAT,
	},
	[JS_TEMP_SENSOR_2] = {
		.cb = aws_shadow_delta_cb,
		.pData = &ecs_aws_reported_in_cloud.analog_sensors.temperature_sensor_2,
		.pKey = "temp_sensor_2",
		.type = SHADOW_JSON_FLOAT,
	},
	[JS_MOTOR_ROT_SPEED] = {
		.cb = aws_shadow_delta_cb,
		.pData = &ecs_aws_reported_in_cloud.analog_sensors.motor_rotating_speed,
		.pKey = "motor_rotating_speed",
		.type = SHADOW_JSON_FLOAT,
	},
	[JS_REMOTE_RFID] = {
		.cb = aws_shadow_delta_cb,
		.pData = ecs_aws_desired_in_cloud.remote_rfid,
		.pKey = "remote_rfid",
		.type = SHADOW_JSON_STRING,
	}
};


static void ecs_init_aws_shadow(void) {
	
	int i;

	for (i = 0; i < JS_TOKEN_MAX; i++) {
		app_aws_init_token(i, &js_tokens[i]);
	}
	aws_init_delta();

	/*
	 * Send this empty values to shadow file to produce delta containing
	 * values has been stored in a shadow file (on a server).
	 * After update, all the values will be in a servers shadow state.
	 */
	ESP_LOGI(tag, "sending empty shadow");

	ECS_IOT_ERR(aws_iot_shadow_init_json_document(json_doc_buf, JSON_BUF_MAX_LEN));
	ECS_IOT_ERR(aws_iot_shadow_add_reported(json_doc_buf, JSON_BUF_MAX_LEN, 6,
						&js_tokens[JS_SWITCH_1_STATE],
						&js_tokens[JS_TEMP_SENSOR_1],
						&js_tokens[JS_TEMP_SENSOR_2],
						&js_tokens[JS_REMOTE_RFID],
						&js_tokens[JS_MOTOR_ROT_SPEED],
						&js_tokens[JS_DEVICE_IS_ON]) );
	ECS_IOT_ERR(aws_iot_finalize_json_document(json_doc_buf, JSON_BUF_MAX_LEN));
	ECS_IOT_ERR(app_aws_shadow_update(json_doc_buf, 4));

	vTaskDelay(500 / portTICK_RATE_MS);

	/*
	 * Send shadow values again to update the actual state.
	 */
	ESP_LOGI(tag, "sending updated shadow");
	ECS_IOT_ERR(aws_iot_shadow_init_json_document(json_doc_buf, JSON_BUF_MAX_LEN));
	ECS_IOT_ERR(aws_iot_shadow_add_reported(json_doc_buf, JSON_BUF_MAX_LEN, 6,
						&js_tokens[JS_SWITCH_1_STATE],
						&js_tokens[JS_TEMP_SENSOR_1],
						&js_tokens[JS_TEMP_SENSOR_2],
						&js_tokens[JS_REMOTE_RFID],
						&js_tokens[JS_MOTOR_ROT_SPEED],
						&js_tokens[JS_DEVICE_IS_ON]) );
	ECS_IOT_ERR(aws_iot_finalize_json_document(json_doc_buf, JSON_BUF_MAX_LEN));
	ECS_IOT_ERR(app_aws_shadow_update(json_doc_buf, 4));

	vTaskDelay(500 / portTICK_RATE_MS);

}

void ecs_ctrl_task(void *arg) {

	size_t size;
	esp_err_t rc;
	uint8_t buf[256];
	uint8_t remote_rfid_buf[64];
	uint8_t remote_rfid_rc;

	uint32_t i;
	char *pos;

	ecs_init_aws_shadow();

	xSemaphoreGive(aws_mqtt_pub_mutex);

	while (true) {

		vTaskDelay(ECS_CTRL_TASK_DELAY_MS / portTICK_RATE_MS);

		/*
		 * Poll the "device" values
		 */
		ECS_ESP_ERR(app_spi_device_ctrl_req(SWITCH_1_STATUS, buf, &size));
		ecs_aws_reported_on_device.boolean_sensors.switch1_status = *(bool*)buf;

		ECS_ESP_ERR(app_spi_device_ctrl_req(TEMP_SENSOR_1, buf, &size));
		ecs_aws_reported_on_device.analog_sensors.temperature_sensor_1 = *(float*)buf;

		ECS_ESP_ERR(app_spi_device_ctrl_req(TEMP_SENSOR_2, buf, &size));
		ecs_aws_reported_on_device.analog_sensors.temperature_sensor_2 = *(float*)buf;

		ECS_ESP_ERR(app_spi_device_ctrl_req(MOTOR_ROT_SPEED, buf, &size));
		ecs_aws_reported_on_device.analog_sensors.motor_rotating_speed = *(float*)buf;

		ECS_ESP_ERR(app_spi_device_ctrl_req(DEVICE_STATUS, buf, &size));
		ecs_aws_desired_on_device.device_on = *(bool*)buf;

		/*
		 * If it differs from those which has been sent last time,
		 * then update "in_cloud" ones and trigger shadow update
		 */

		if ( memcmp(&ecs_aws_reported_on_device, &ecs_aws_reported_in_cloud, 
					sizeof(ecs_aws_reported_on_device)) != 0)
		{

			ecs_aws_reported_in_cloud.boolean_sensors.switch1_status = 
				ecs_aws_reported_on_device.boolean_sensors.switch1_status;
	
			ecs_aws_reported_in_cloud.analog_sensors.temperature_sensor_1 = 
				ecs_aws_reported_on_device.analog_sensors.temperature_sensor_1;

			ecs_aws_reported_in_cloud.analog_sensors.temperature_sensor_2 = 
				ecs_aws_reported_on_device.analog_sensors.temperature_sensor_2;

			ecs_aws_reported_in_cloud.analog_sensors.motor_rotating_speed =
				ecs_aws_reported_on_device.analog_sensors.motor_rotating_speed;

			ECS_IOT_ERR(aws_iot_shadow_init_json_document(json_doc_buf, JSON_BUF_MAX_LEN));
			ECS_IOT_ERR(aws_iot_shadow_add_reported(json_doc_buf, JSON_BUF_MAX_LEN, 6,
						&js_tokens[JS_SWITCH_1_STATE],
						&js_tokens[JS_TEMP_SENSOR_1],
						&js_tokens[JS_TEMP_SENSOR_2],
						&js_tokens[JS_REMOTE_RFID],
						&js_tokens[JS_MOTOR_ROT_SPEED],
						&js_tokens[JS_DEVICE_IS_ON]) );
			ECS_IOT_ERR(aws_iot_finalize_json_document(json_doc_buf, JSON_BUF_MAX_LEN));
			ECS_IOT_ERR(app_aws_shadow_update(json_doc_buf, 4));
		}
	

		/*
		 * Send the values to device if needed
		 */
		if ( memcmp(&ecs_aws_desired_on_device, &ecs_aws_desired_in_cloud, 
					sizeof(ecs_aws_desired_on_device)) != 0)
		{

			if (ecs_aws_desired_on_device.device_on != ecs_aws_desired_in_cloud.device_on) {
				if (ecs_aws_desired_in_cloud.device_on) {

					rc = app_spi_device_ctrl_req(DEVICE_ON, buf, &size);

					if (rc != ESP_OK) {
						ESP_LOGE(tag, "Error setting device state");
						continue;
					}

					if (*(uint8_t*)buf == 0x01)
						ecs_aws_desired_on_device.device_on = true;
			
					ECS_IOT_ERR(aws_iot_shadow_init_json_document(json_doc_buf, JSON_BUF_MAX_LEN));
					ECS_IOT_ERR(aws_iot_shadow_add_reported(json_doc_buf, JSON_BUF_MAX_LEN, 6,
								&js_tokens[JS_SWITCH_1_STATE],
								&js_tokens[JS_TEMP_SENSOR_1],
								&js_tokens[JS_TEMP_SENSOR_2],
								&js_tokens[JS_REMOTE_RFID],
								&js_tokens[JS_MOTOR_ROT_SPEED],
								&js_tokens[JS_DEVICE_IS_ON]) );
					ECS_IOT_ERR(aws_iot_finalize_json_document(json_doc_buf, JSON_BUF_MAX_LEN));
					ECS_IOT_ERR(app_aws_shadow_update(json_doc_buf, 4));
				}
				else {
					rc = app_spi_device_ctrl_req(DEVICE_OFF, buf, &size);

					if (rc != ESP_OK) {
						ESP_LOGE(tag, "Error setting device state");
						continue;
					}

					if (*(uint8_t*)buf == 0x01)
						ecs_aws_desired_on_device.device_on = false;

					ECS_IOT_ERR(aws_iot_shadow_init_json_document(json_doc_buf, JSON_BUF_MAX_LEN));
					ECS_IOT_ERR(aws_iot_shadow_add_reported(json_doc_buf, JSON_BUF_MAX_LEN, 6,
								&js_tokens[JS_SWITCH_1_STATE],
								&js_tokens[JS_TEMP_SENSOR_1],
								&js_tokens[JS_TEMP_SENSOR_2],
								&js_tokens[JS_REMOTE_RFID],
								&js_tokens[JS_MOTOR_ROT_SPEED],
								&js_tokens[JS_DEVICE_IS_ON]) );
					ECS_IOT_ERR(aws_iot_finalize_json_document(json_doc_buf, JSON_BUF_MAX_LEN));
					ECS_IOT_ERR(app_aws_shadow_update(json_doc_buf, 4));
				}
			}

			if ( strncmp (ecs_aws_desired_on_device.remote_rfid, 
						ecs_aws_desired_in_cloud.remote_rfid, 128) != 0) {

				pos = ecs_aws_desired_in_cloud.remote_rfid;

				for (i = 0; i < 64; i++) {
					sscanf(pos, "%2hhx", &remote_rfid_buf[i]);
					pos += 2;
				}				
				
				ECS_ESP_ERR(app_spi_device_ctrl_remote_rfid(remote_rfid_buf, &remote_rfid_rc));
			}


			if (ecs_aws_desired_on_device.device_start != ecs_aws_desired_in_cloud.device_start) {
				ecs_aws_desired_on_device.device_start = ecs_aws_desired_in_cloud.device_start;
				ECS_IOT_ERR(aws_iot_shadow_init_json_document(json_doc_buf, JSON_BUF_MAX_LEN));
				ECS_IOT_ERR(aws_iot_shadow_add_reported(json_doc_buf, JSON_BUF_MAX_LEN, 6,
							&js_tokens[JS_SWITCH_1_STATE],
							&js_tokens[JS_TEMP_SENSOR_1],
							&js_tokens[JS_TEMP_SENSOR_2],
							&js_tokens[JS_REMOTE_RFID],
							&js_tokens[JS_MOTOR_ROT_SPEED],
							&js_tokens[JS_DEVICE_IS_ON]) );
				ECS_IOT_ERR(aws_iot_finalize_json_document(json_doc_buf, JSON_BUF_MAX_LEN));
				ECS_IOT_ERR(app_aws_shadow_update(json_doc_buf, 4));
			}
		}
	}
}

void aws_shadow_delta_cb(const char *pJsonValueBuffer, uint32_t valueLength, jsonStruct_t *pJsonStruct) {

	if (pJsonStruct == NULL)
		return;

	ESP_LOGI(tag, "%s: %.*s\n", pJsonStruct->pKey, valueLength, pJsonValueBuffer);
	
	xSemaphoreTake(aws_mqtt_pub_mutex, 1000 / portTICK_RATE_MS);

	/*
	 * Somehow, the string value is not copying into destination automatically.
	 * I don't want to dive into API internals.
	 */
	if (pJsonStruct->type == SHADOW_JSON_STRING) {
		memcpy(pJsonStruct->pData, pJsonValueBuffer, valueLength);
	}

	xSemaphoreGive(aws_mqtt_pub_mutex);
}

