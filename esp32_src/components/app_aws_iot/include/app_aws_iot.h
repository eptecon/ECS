#ifndef APP_AWS_IOT_H
#define APP_AWS_IOT_H

struct mqtt_message {
#define AWS_IOT_MAX_PAYLOAD_LEN 128
#define AWS_IOT_MAX_TOPIC_LEN 32
	char payload[AWS_IOT_MAX_PAYLOAD_LEN];
	size_t payload_len;
	char topic[AWS_IOT_MAX_TOPIC_LEN];
	uint8_t qos;
};

void aws_iot_task(void *param);
void aws_iot_test_task(void *param);

bool app_aws_mqtt_pub_is_allowed(void);
esp_err_t app_aws_mqtt_pub(struct mqtt_message *msg, uint32_t timeout);

#include "aws_iot_shadow_interface.h"
void app_aws_init_token(uint16_t n, jsonStruct_t *js);
void aws_init_delta(void);

IoT_Error_t aws_update_reported(uint8_t timeout_sec);
IoT_Error_t aws_update_reported_new(uint8_t timeout_sec, uint8_t count, ...);
IoT_Error_t aws_iot_init(void);

IoT_Error_t app_aws_shadow_update(char *json_doc_buf, uint8_t timeout_sec);

#endif /* APP_AWS_IOT_H */
