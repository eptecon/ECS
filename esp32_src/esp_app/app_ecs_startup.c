#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "driver/gpio.h"

#include "bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include "app_ble_gattc.h"
#include "app_ble_ibeacon.h"

#include "app_flash.h"
#include "app_settings.h"
#include "app_spi.h"
#include "app_aws_iot.h"
#include "app_ble_task.h"
#include "app_ble_gap.h"
#include "http_server.h"
#include "led_blinker.h"

#define BTN_RELEASED_BIT BIT0

#define KEY_GPIO_NUM 14
#define KEY_ACTIVE 0

/*
 * Consider that device is "booting" for ~1 second.
 * This way, if you want to provide the delay period
 * for N seconds in fact, you should decrease it for 1 second.
 */
#define LONG_PRESS_PERIOD_MS 1000 //actually, 2 seconds
#define GLITCH_TIMEOUT_MS 200

static const char *tag = "device_startup";

EventGroupHandle_t wifi_event_group;
EventGroupHandle_t button_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

const int AP_CONNECTED_BIT = BIT1;

uint8_t aws_root_ca_pem[2048];
uint8_t certificate_pem_crt[2048];
uint8_t private_pem_key[2048];

static void IRAM_ATTR gpio_isr_handler_up(void* arg) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xEventGroupSetBitsFromISR(button_event_group, BTN_RELEASED_BIT, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR();
}

void get_aws_certs(void) {
	ESP_ERROR_CHECK(app_settings_get_linefeeded_string(SETTINGS_ROOT_CA_PEM, (char*)aws_root_ca_pem));
	ESP_ERROR_CHECK(app_settings_get_linefeeded_string(SETTINGS_CERT_PEM, (char*)certificate_pem_crt));
	ESP_ERROR_CHECK(app_settings_get_linefeeded_string(SETTIGNS_PRIV_KEY, (char*)private_pem_key));
}



static esp_err_t event_handler(void *ctx, system_event_t *event) {
	switch(event->event_id) {
	case SYSTEM_EVENT_STA_START:
		esp_wifi_connect();
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
		xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		/* This is a workaround as ESP32 WiFi libs don't currently
		   auto-reassociate. */
		esp_wifi_connect();
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
		//esp_restart();
		break;
	case SYSTEM_EVENT_AP_STACONNECTED:
		xEventGroupSetBits(wifi_event_group, AP_CONNECTED_BIT);
		break;
	default:
		break;
	}
	return ESP_OK;
}

static void initialise_wifi(bool start_http_server) {

	char buf[128];

	esp_err_t rc;

	wifi_config_t wifi_config = {0};

	tcpip_adapter_init();

	wifi_event_group = xEventGroupCreate();

	esp_event_loop_init(event_handler, NULL);
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

	ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

#if 0
	rc = app_settings_get_value(SETTINGS_WIFI_SSID, (char *) wifi_config_settings.sta.ssid);

	if (rc != ESP_OK || strlen((char *)wifi_config_settings.sta.ssid) == 0)
		start_http_server = true;

	rc = app_settings_get_value(SETTINGS_WIFI_PASSWD, (char *) wifi_config_settings.sta.password);

	if (rc != ESP_OK || strlen((char *)wifi_config_settings.sta.password) == 0)
		start_http_server = true;
#endif

	/*
	 * Has the wi-fi credentials been read successfully AND we don't need to launch HTTP server ?
	 */
	if ( (app_flash_load_wifi_sta_settings(&wifi_config.sta) == ESP_OK) && !start_http_server ) {
		app_flash_load_wifi_sta_settings(&wifi_config.sta);
		ESP_LOGI(tag, "Connecting to %s...", wifi_config.sta.ssid);
		ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
		ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
		ESP_ERROR_CHECK( esp_wifi_start() );
	}
	else {
		rc = app_settings_get_value(SETTINGS_AP_SSID, buf);
		ESP_LOGI(tag, "Creating AP \"%s\"", buf);

		rc = app_settings_get_value(SETTINGS_AP_SSID, (char*)wifi_config.ap.ssid);

		/*
		 * Default name if we have failed to read from settings
		 */
		if (rc != ESP_OK)
			strcpy((char*)wifi_config.ap.ssid, "ECS setup AP");
		
		wifi_config.ap.ssid_len = 0;
		wifi_config.ap.max_connection = 1;
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;

		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
		ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
		ESP_ERROR_CHECK(esp_wifi_start());
		ESP_LOGI(tag, "Starting HTTP server...\n");
		led_blinker_set_mode(BLINK_MODE_HTTP_SERVER);
		xTaskCreatePinnedToCore(&http_server_task, "http_server", 4 * 1024, NULL, 5, NULL, 1);
	}
}

void app_ecs_startup(void) {

	app_flash_init();
	ESP_ERROR_CHECK( app_settings_init() );

	/*
	 * Setup button GPIO
	 */
	gpio_config_t io_conf = {
		.pin_bit_mask = 1 << KEY_GPIO_NUM,
		.mode = GPIO_MODE_INPUT,
		.pull_down_en = 0,
		.pull_up_en = 1,
		.intr_type = GPIO_INTR_POSEDGE,
	};
	gpio_config(&io_conf);
	gpio_intr_enable(KEY_GPIO_NUM);
	gpio_install_isr_service(0);
	gpio_isr_handler_add(KEY_GPIO_NUM, gpio_isr_handler_up, (void*) KEY_GPIO_NUM);

	
	led_blinker_set_mode(BLINK_MODE_STARTED);

	button_event_group = xEventGroupCreate();

	bool start_http_server = false;

	/*
	 * if user has pressed the button before device reboot,
	 * then we need to wait for 3 sec to start the http server
	 */
	if (gpio_get_level(KEY_GPIO_NUM) == KEY_ACTIVE) {
		vTaskDelay(GLITCH_TIMEOUT_MS / portTICK_RATE_MS);

		if (gpio_get_level(KEY_GPIO_NUM) == KEY_ACTIVE) {

			EventBits_t bits = xEventGroupWaitBits(button_event_group, 
					BTN_RELEASED_BIT, 
					pdTRUE, 
					pdTRUE, 
					LONG_PRESS_PERIOD_MS / portTICK_RATE_MS);
			/*
			 * button hasn't beed released ?
			 */
			if (!(bits & BTN_RELEASED_BIT)) {
				/*
				 * button has been pressed during 3 sec, 
				 * starting http server
				 */
				start_http_server = true;
			}
		}
	}

	initialise_wifi(start_http_server);

	EventBits_t wifi_bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
				false, true, 5000 / portTICK_RATE_MS);

	if ((wifi_bits & CONNECTED_BIT) != CONNECTED_BIT) {
		start_http_server = true;
		initialise_wifi(start_http_server);

		vTaskSuspend(NULL);
	}

	get_aws_certs();

	ESP_ERROR_CHECK( app_spi_init() );

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	esp_bt_controller_init(&bt_cfg);

	if (esp_bt_controller_enable(ESP_BT_MODE_BTDM) != ESP_OK)
		ESP_LOGE(tag, "esp_bt_controller_enable() fails");

	if (esp_bluedroid_init() != ESP_OK)
		ESP_LOGE(tag, "esp_bluedroid_init() fails");

	if (esp_bluedroid_enable() != ESP_OK)
		ESP_LOGE(tag, "esp_bluedroid_enable() fails");

	app_ble_gap_init(NULL);
	ble_client_init();

	ibeacon_adv_data_set_default();

	if (aws_iot_init() == SUCCESS) {
		led_blinker_set_mode(BLINK_MODE_CONNECTED);
	}
}
