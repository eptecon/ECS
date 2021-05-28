#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

typedef enum {
	SETTINGS_WIFI_SSID,
	SETTINGS_WIFI_PASSWD,
	SETTINGS_AP_SSID,
	SETTINGS_AP_IP_ADDR,
	SETTINGS_MQTT_CLIENT_HOST_ADDR,
	SETTINGS_MQTT_CLIENT_PORT,
	SETTINGS_ROOT_CA_PEM,
	SETTINGS_CERT_PEM,
	SETTIGNS_PRIV_KEY
} app_settings_json_token_t;

esp_err_t app_settings_read(char *buf, size_t buflen);
esp_err_t app_settings_init(void);
esp_err_t app_settings_get_value(app_settings_json_token_t t, char *out_buf);
void *app_settings_get_value_p(app_settings_json_token_t t, size_t *size);
esp_err_t app_settings_get_multiline(app_settings_json_token_t t, char *out_buf);
esp_err_t app_settings_get_linefeeded_string(app_settings_json_token_t t, char *out_buf);

#endif /* APP_SETTINGS_H */
