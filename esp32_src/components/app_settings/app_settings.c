#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "app_settings.h"
#include "app_flash.h"

#include "jsmn.h"
#include "json_utils.h"

#define SETTINGS_JSMN_TOK_MAX_COUNT 150
#define SETTINGS_BUF_SIZE (1024 * 6)

static jsmn_parser jsmn;
static jsmntok_t jsmn_tok[SETTINGS_JSMN_TOK_MAX_COUNT];
static char settings_buf[SETTINGS_BUF_SIZE];

esp_err_t app_settings_init(void) {
	esp_err_t rc;
	int jsmn_ret;
	int i = 0;
	rc = string_read_from_flash("settings", settings_buf, sizeof(settings_buf));

	if (rc != ESP_OK)
		return rc;
	
	jsmn_init(&jsmn);

	/* find where settings ends in flash */
	while (settings_buf[i++] != 0xff); 

	/* the last character will be 0xff, we don't need it */
	i--;

	jsmn_ret = jsmn_parse(&jsmn, settings_buf, i, jsmn_tok, sizeof(jsmn_tok) / sizeof(jsmn_tok[0]));
	
	if (jsmn_ret < 0) {
		ESP_LOGI("", "jsmn err = %d", jsmn_ret);
		return ESP_FAIL;
	}
	
	return ESP_OK;
}

#define APP_JSMN_INDEX_ERR(INDEX, JSON_KEY) do { \
		if ((INDEX = jsmn_get_key_index(JSON_KEY, INDEX, jsmn_tok, settings_buf)) < 0)	\
			return INDEX;				\
		INDEX++;					\
		} while (0)

esp_err_t app_settings_index_key(app_settings_json_token_t t, int *index) {

	int i = 0;

	switch (t) {
	case SETTINGS_WIFI_SSID:

		APP_JSMN_INDEX_ERR(i, "connection_parameters");
		APP_JSMN_INDEX_ERR(i, "wifi");
		APP_JSMN_INDEX_ERR(i, "ssid");

		break;

	case SETTINGS_WIFI_PASSWD:

		APP_JSMN_INDEX_ERR(i, "connection_parameters");
		APP_JSMN_INDEX_ERR(i, "wifi");
		APP_JSMN_INDEX_ERR(i, "passwd");

		break;

	case SETTINGS_AP_SSID:

		APP_JSMN_INDEX_ERR(i, "setup_AP");
		APP_JSMN_INDEX_ERR(i, "ssid");

		break;
	
	case SETTINGS_AP_IP_ADDR:

		APP_JSMN_INDEX_ERR(i, "setup_AP");
		APP_JSMN_INDEX_ERR(i, "ip_addr");

		break;

	case SETTINGS_MQTT_CLIENT_HOST_ADDR:

		APP_JSMN_INDEX_ERR(i, "connection_parameters");
		APP_JSMN_INDEX_ERR(i, "mqtt_client");
		APP_JSMN_INDEX_ERR(i, "host_addr");

		break;

	case SETTINGS_MQTT_CLIENT_PORT:

		APP_JSMN_INDEX_ERR(i, "connection_parameters");
		APP_JSMN_INDEX_ERR(i, "mqtt_client");
		APP_JSMN_INDEX_ERR(i, "port");

		break;

	case SETTINGS_ROOT_CA_PEM:
		
		APP_JSMN_INDEX_ERR(i, "SSL_certificates");
		APP_JSMN_INDEX_ERR(i, "root_CA");

		break;

	case SETTINGS_CERT_PEM:

		APP_JSMN_INDEX_ERR(i, "SSL_certificates");
		APP_JSMN_INDEX_ERR(i, "certificate_pem");
		
		break;

	case SETTIGNS_PRIV_KEY:

		APP_JSMN_INDEX_ERR(i, "SSL_certificates");
		APP_JSMN_INDEX_ERR(i, "private_key");
		
		break;

	default:
		return ESP_FAIL;
	}

	*index = i;
	return ESP_OK;
}

/*
 * Copy token value into out_buf
 */
esp_err_t app_settings_get_value(app_settings_json_token_t t, char *out_buf) {

	int i = 0;

	/*
	 * Not there, bro.
	 * Use app_settings_get_multiline() instead.
	 */
	if (	t == SETTINGS_ROOT_CA_PEM || 
		t == SETTINGS_CERT_PEM ||
		t == SETTIGNS_PRIV_KEY)
		return ESP_FAIL;
	
	ESP_ERROR_CHECK(app_settings_index_key(t, &i));
	
	memcpy(out_buf, settings_buf + jsmn_tok[i].start, jsmn_tok[i].end - jsmn_tok[i].start);
	out_buf[ jsmn_tok[i].end - jsmn_tok[i].start] = '\0';

	return ESP_OK;
}

/*
 * Return the pointer to token
 */
void *app_settings_get_value_p(app_settings_json_token_t t, size_t *size) {
	int i = 0;

#if 0	
	/*
	 * Not there, bro.
	 * Use app_settings_get_multiline() instead.
	 */
	if (	t == SETTINGS_ROOT_CA_PEM || 
		t == SETTINGS_CERT_PEM ||
		t == SETTIGNS_PRIV_KEY)
		return ESP_FAIL;
#endif
	ESP_ERROR_CHECK(app_settings_index_key(t, &i));
	*size = jsmn_tok[i].end - jsmn_tok[i].start;
	return (void*)(settings_buf + jsmn_tok[i].start);
}

/*
 * Get the JSON strings array terminated by newlines as a single char array;
 */
esp_err_t app_settings_get_multiline(app_settings_json_token_t t, char *out_buf) {

	int i = 0;

	APP_JSMN_INDEX_ERR(i, "SSL_certificates");

	switch (t) {
	case SETTINGS_ROOT_CA_PEM:
		APP_JSMN_INDEX_ERR(i, "root_CA");
		break;

	case SETTINGS_CERT_PEM:
		APP_JSMN_INDEX_ERR(i, "certificate_pem");
		break;

	case SETTIGNS_PRIV_KEY:
		APP_JSMN_INDEX_ERR(i, "private_key");
		break;

	default:
		return ESP_FAIL;
	}

	jsmn_array_join_lines(i, jsmn_tok, settings_buf, out_buf);
	return ESP_OK;
}

esp_err_t app_settings_get_linefeeded_string(app_settings_json_token_t t, char *out_buf) {

	int i = 0;
	size_t size;

	APP_JSMN_INDEX_ERR(i, "SSL_certificates");

	switch (t) {
	case SETTINGS_ROOT_CA_PEM:
		APP_JSMN_INDEX_ERR(i, "root_CA");
		break;

	case SETTINGS_CERT_PEM:
		APP_JSMN_INDEX_ERR(i, "certificate_pem");
		break;

	case SETTIGNS_PRIV_KEY:
		APP_JSMN_INDEX_ERR(i, "private_key");
		break;

	default:
		return ESP_FAIL;
	}

	jsmn_convert_line_breaks(out_buf, settings_buf + jsmn_tok[i].start, jsmn_tok[i].end - jsmn_tok[i].start);

	return ESP_OK;

}
