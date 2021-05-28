#include <string.h>
#include <stdarg.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_log.h"

#include "aws_iot_config.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"

#include "app_aws_iot.h"
#include "app_settings.h"
#include "app_spi.h"

#define AWS_ERR(x) do {									\
			IoT_Error_t err;						\
			if ((err = (x)) != SUCCESS) {					\
				ESP_LOGE(tag, "%s %s: %d, retcode = %d", 		\
						__FILE__, __FUNCTION__, __LINE__, err);	\
			}								\
		} while (0)

AWS_IoT_Client client;

/* Time to wait between a publish series to prevent processor occupation */
#define MQTT_PUBLISH_RELAX_PERIOD_MS 200

#define KEEPALIVE_INTERVAL_MS 10000

#define JSON_BUF_MAX_LEN 600
#define JSON_TOKENS_MAX_COUNT 8

static int cycles_between_keepalives  = ((KEEPALIVE_INTERVAL_MS) / 5) / MQTT_PUBLISH_RELAX_PERIOD_MS;

static const char *tag = "mqtt_task";

extern uint8_t aws_root_ca_pem[2048];
extern uint8_t certificate_pem_crt[2048];
extern uint8_t private_pem_key[2048];

static jsonStruct_t js_tokens[JSON_TOKENS_MAX_COUNT];
static char json_doc_buf[JSON_BUF_MAX_LEN];

bool is_connected;

bool shadow_need_update = false;

static QueueHandle_t aws_mqtt_pub_queue = NULL;
SemaphoreHandle_t aws_mqtt_pub_mutex = NULL;


static void aws_disconnect_handler(AWS_IoT_Client *pClient, void *data);

void aws_iot_task(void *param) {

	IoT_Publish_Message_Params msg_params;

	struct mqtt_message msg;

	msg_params.qos = QOS0;
	msg_params.isRetained = 0;

	int cycles = 0;
	
	IoT_Error_t rc = SUCCESS;

	while ((NETWORK_ATTEMPTING_RECONNECT == rc || 
		NETWORK_RECONNECTED == rc || 
		SUCCESS == rc ||
		MQTT_CLIENT_NOT_IDLE_ERROR == rc)) {
		
		vTaskDelay(MQTT_PUBLISH_RELAX_PERIOD_MS / portTICK_RATE_MS);
		
		if (!is_connected)
			continue;

		if (NETWORK_ATTEMPTING_RECONNECT == rc) {

			xSemaphoreTake(aws_mqtt_pub_mutex, 1000 / portTICK_RATE_MS);
			rc = aws_iot_shadow_yield(&client, 1000);
			// If the client is attempting to reconnect, 
			// or already waiting on a shadow update,
			// we will skip the rest of the loop.
			continue;
		}

		if (NETWORK_RECONNECTED == rc) {
			ESP_LOGI(tag, "Network reconnected");
			rc = SUCCESS;
		}

		if (shadow_need_update) {

			ESP_LOGI(tag, "updating reported");
			rc = aws_update_reported(8);
			if (rc != SUCCESS) {
				ESP_LOGE(tag, "delta update fails, rc = %d", rc);
				continue;
			}
			shadow_need_update = false;
		}

		if (cycles >= cycles_between_keepalives) {
			cycles = 0;
			rc = aws_iot_shadow_yield(&client, 200);

			if (rc != SUCCESS) {
				AWS_ERR(rc);
				continue;
			}
		}
		
		while (xQueueReceive(aws_mqtt_pub_queue, &msg, 0)) {
			msg_params.payload = msg.payload;
			msg_params.payloadLen = msg.payload_len;
			msg_params.qos = msg.qos;
			aws_iot_mqtt_publish(&client, msg.topic, strlen(msg.topic), &msg_params);
		}
		
		rc = aws_iot_shadow_yield(&client, MQTT_PUBLISH_RELAX_PERIOD_MS);
	}

	AWS_ERR(rc);
	esp_restart();
}

esp_err_t app_aws_mqtt_pub(struct mqtt_message *msg, uint32_t timeout) {

	if (aws_mqtt_pub_queue == NULL) {
		ESP_LOGE(tag, "MQTT queue is not iniaitlized");
		return ESP_ERR_INVALID_STATE;
	}

	if (!xSemaphoreTake(aws_mqtt_pub_mutex, 200 / portTICK_RATE_MS)) {
		return ESP_ERR_INVALID_STATE;
	}

	if (!xQueueSend(aws_mqtt_pub_queue, msg, timeout)) {
		return ESP_FAIL;
	}

	xSemaphoreGive(aws_mqtt_pub_mutex);

	return ESP_OK;
}

void aws_iot_test_task(void *param) {

	struct mqtt_message msg;
	uint32_t t;
	esp_err_t rc;
	
	for (;;) {

		vTaskDelay(500 / portTICK_RATE_MS);

		t = xTaskGetTickCount() * portTICK_RATE_MS;
		sprintf(msg.payload, "%d ms", t);
		msg.payload_len = strlen(msg.payload);

		strcpy(msg.topic, (char*)param);

		if ((rc = app_aws_mqtt_pub(&msg, 200 / portTICK_RATE_MS) != ESP_OK)) {
			ESP_LOGI(tag, "MQTT pub fail, rc = 0x%02X", rc);
			continue;
		}
	}
}

IoT_Error_t aws_update_reported_new(uint8_t timeout_sec, uint8_t count, ...) {

	IoT_Error_t rc;

	size_t json_size = sizeof(json_doc_buf) / sizeof(json_doc_buf[0]);

	rc = aws_iot_shadow_init_json_document(json_doc_buf, json_size);
	if (rc != SUCCESS) {
		AWS_ERR(rc);
		return rc;
	}
	
	va_list argptr;	
	va_start(argptr, count);

	rc = aws_iot_shadow_add_reported(json_doc_buf, json_size, count, argptr);

	va_end(argptr);
	
	if (rc != SUCCESS) {
		AWS_ERR(rc);
		return rc;
	}

	rc = aws_iot_finalize_json_document(json_doc_buf, json_size);
	if (SUCCESS != rc) {
		ESP_LOGE(tag, "aws_iot_finalize_json_document() fails (%d)", rc);
		return rc;
	}

	rc = aws_iot_shadow_update(&client, "ESP32_test", json_doc_buf,
		NULL, NULL, timeout_sec, true);

	if (SUCCESS != rc) {
		ESP_LOGE(tag, "aws_iot_shadow_update() fails (%d)", rc);
		return rc;
	}

	return rc;
}


IoT_Error_t aws_update_reported(uint8_t timeout_sec) {

	IoT_Error_t rc;

	rc = aws_iot_shadow_init_json_document(json_doc_buf, JSON_BUF_MAX_LEN);
	if (rc != SUCCESS) {
		AWS_ERR(rc);
		return rc;
	}

	rc = aws_iot_shadow_add_reported(json_doc_buf, JSON_BUF_MAX_LEN, 5,
		&js_tokens[0],
		&js_tokens[1],
		&js_tokens[2],
		&js_tokens[3],
		&js_tokens[4]);
	
	if (rc != SUCCESS) {
		AWS_ERR(rc);
		return rc;
	}

	rc = aws_iot_finalize_json_document(json_doc_buf, JSON_BUF_MAX_LEN);
	if (SUCCESS != rc) {
		ESP_LOGE(tag, "aws_iot_finalize_json_document() fails (%d)", rc);
		return rc;
	}

	rc = aws_iot_shadow_update(&client, "ESP32_test", json_doc_buf,
		NULL, NULL, timeout_sec, true);

	if (SUCCESS != rc) {
		ESP_LOGE(tag, "aws_iot_shadow_update() fails (%d)", rc);
		return rc;
	}

	return rc;
}

IoT_Error_t app_aws_shadow_update(char *json_doc_buf_p, uint8_t timeout_sec) {
	IoT_Error_t rc;
//	xSemaphoreTake(aws_mqtt_pub_mutex, portMAX_DELAY);
	rc = aws_iot_shadow_update(&client, "ESP32_test", json_doc_buf_p,
		NULL, NULL, timeout_sec, true);
//	xSemaphoreGive(aws_mqtt_pub_mutex);
	return rc;
}

void app_aws_init_token(uint16_t n, jsonStruct_t *js) {
	js_tokens[n].cb = js->cb;
	js_tokens[n].pData = js->pData;
	js_tokens[n].pKey = js->pKey;
	js_tokens[n].type = js->type;
}

void aws_init_delta(void) {

	IoT_Error_t rc;
	int i;

	for (i = 0; i < JSON_TOKENS_MAX_COUNT; i++) {

		if (js_tokens[i].cb == 0)
			break;

		rc = aws_iot_shadow_register_delta(&client, &js_tokens[i]);
		if (rc != SUCCESS) {
			ESP_LOGE(tag, "%s: error registering \"%s\" delta, rc = %d", 
					__FUNCTION__, js_tokens[i].pKey, rc);
			abort();
		}
	}
}

IoT_Error_t aws_iot_init(void) {
	char port_num_string[8];
	char host_string[64];
	IoT_Error_t rc = FAILURE;

	ShadowInitParameters_t sp = ShadowInitParametersDefault;

	ESP_ERROR_CHECK( app_settings_get_value(SETTINGS_MQTT_CLIENT_HOST_ADDR, host_string) );
	ESP_ERROR_CHECK( app_settings_get_value(SETTINGS_MQTT_CLIENT_PORT, port_num_string) );

	sp.pHost = host_string;
	sp.port = atoi(port_num_string);

	sp.pClientCRT = (const char *)certificate_pem_crt;
	sp.pClientKey = (const char *)private_pem_key;
	sp.pRootCA = (const char *)aws_root_ca_pem;
	sp.enableAutoReconnect = false;
	sp.disconnectHandler = aws_disconnect_handler;

	ESP_LOGI(tag, "Shadow Init");
	rc = aws_iot_shadow_init(&client, &sp);
	if(SUCCESS != rc) {
		AWS_ERR(rc);
		abort();
	}

	ShadowConnectParameters_t scp = ShadowConnectParametersDefault;
	scp.pMyThingName = "ESP32_test";
	scp.pMqttClientId = "esp32";
	scp.mqttClientIdLen = (uint16_t) strlen("esp32");

	int attempts = 5;
	ESP_LOGI(tag, "Thing name: %s", scp.pMyThingName);
	do {
		rc = aws_iot_shadow_connect(&client, &scp);
		if(SUCCESS != rc) {
			AWS_ERR(rc);
			vTaskDelay(1000 / portTICK_RATE_MS);
		}
	} while((SUCCESS != rc) && (attempts--));

	if (attempts == 0)
		esp_restart();
	/*
	 * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
	 *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
	 *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
	 */
	rc = aws_iot_shadow_set_autoreconnect_status(&client, true);
	if(SUCCESS != rc) {
		AWS_ERR(rc);
		abort();
	}

	aws_mqtt_pub_queue = xQueueCreate(20, sizeof(struct mqtt_message));
	aws_mqtt_pub_mutex = xSemaphoreCreateBinary();

	is_connected = true;

	return rc;
}

static void aws_disconnect_handler(AWS_IoT_Client *pClient, void *data) {
	ESP_LOGW(tag, "MQTT Disconnect");
	IoT_Error_t rc = FAILURE;

	if(NULL == pClient) {
		return;
	}

	if(aws_iot_is_autoreconnect_enabled(pClient)) {
		ESP_LOGI(tag, "Auto Reconnect is enabled, Reconnecting attempt will start now");
	} else {
		ESP_LOGW(tag, "Auto Reconnect not enabled. Starting manual reconnect...");
		rc = aws_iot_mqtt_attempt_reconnect(pClient);
		if(NETWORK_RECONNECTED == rc) {
			ESP_LOGW(tag, "Manual Reconnect Successful");
			xSemaphoreGive(aws_mqtt_pub_mutex);
		} else {
			ESP_LOGW(tag, "Manual Reconnect Failed - %d", rc);
		}
	}
}

