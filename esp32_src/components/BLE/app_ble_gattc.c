#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "controller.h"

#include "bt.h"
#include "bt_trace.h"
#include "bt_types.h"
#include "btm_api.h"
#include "bta_api.h"
#include "bta_gatt_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "app_ble_gattc.h"

#define GATTC_TAG "gattc"

///Declare static functions
void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_a_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

static gatt_mode_t current_gatt_mode;

static EventGroupHandle_t ble_evt;

const int SCAN_START_BIT = BIT0;
const int SCAN_STOP_BIT = BIT1;
const int ADV_SET_BIT = BIT2;
const int ADV_START_BIT = BIT3;
const int ADV_STOP_BIT = BIT4;

void ble_adv_handle_scan_test(void);

static esp_ble_scan_params_t ble_scan_params = {
	.scan_type			  = BLE_SCAN_TYPE_ACTIVE,
	.own_addr_type		  = BLE_ADDR_TYPE_RPA_PUBLIC,
	.scan_filter_policy	 = BLE_SCAN_FILTER_ALLOW_ALL,
	.scan_interval		  = 80,
	.scan_window			= 48
};

#define PROFILE_NUM 1
#define PROFILE_A_APP_ID 0

struct gattc_profile_inst {
	esp_gattc_cb_t gattc_cb;
	uint16_t gattc_if;
	uint16_t app_id;
	uint16_t conn_id;
	esp_bd_addr_t remote_bda;
};

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
	[PROFILE_A_APP_ID] = {
		.gattc_cb = gattc_profile_a_event_handler,
		.gattc_if = ESP_GATT_IF_NONE,	   /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
	},
};

static void gattc_profile_a_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
	switch (event) {
	case ESP_GATTC_REG_EVT:
		ESP_LOGI(GATTC_TAG, "REG_EVT");
		esp_ble_gap_set_scan_params(&ble_scan_params);
		break;
	default:
		break;
	}
}

void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
	ESP_LOGI("gattc_cb", "EVT %d, gattc if %d", event, gattc_if);

	/* If event is register event, store the gattc_if for each profile */
	if (event == ESP_GATTC_REG_EVT) {
		if (param->reg.status == ESP_GATT_OK) {
			gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
		} else {
			ESP_LOGI(GATTC_TAG, "Reg app failed, app_id %04x, status %d",
					param->reg.app_id, 
					param->reg.status);
			return;
		}
	}

	/* If the gattc_if equal to profile A, call profile A cb handler,
	 * so here call each profile's callback */
	do {
		int idx;
		for (idx = 0; idx < PROFILE_NUM; idx++) {
			if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
					gattc_if == gl_profile_tab[idx].gattc_if) {
				if (gl_profile_tab[idx].gattc_cb) {
					gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
				}
			}
		}
	} while (0);
}

void ble_client_init(void) {
	esp_err_t status;

	//register the callback function to the gattc module
	if ((status = esp_ble_gattc_register_callback(esp_gattc_cb)) != ESP_OK) {
		ESP_LOGE(GATTC_TAG, "gattc register error, error code = %x", status);
		return;
	}

	ESP_LOGI(TAG, "registering gattc apps");

	
	if (( status = esp_ble_gattc_app_register(PROFILE_A_APP_ID)) != ESP_OK) {
		ESP_LOGE(GATTC_TAG, "gattc app register error, error code = %x", status);
		return;
	}
}

void ble_client_deinit(void) {
	esp_err_t status;

	if ((status = esp_ble_gattc_app_unregister(gl_profile_tab[PROFILE_A_APP_ID].gattc_if)) != ESP_OK)
			ESP_LOGE(TAG, "esp_ble_gattc_app_unregister 0 failed, ret = %d", status);
}


void ble_client_app_register(void)
{
	ESP_LOGI(GATTC_TAG, "register callback");
	ble_evt = xEventGroupCreate();
	
	ble_client_init();
}


void ble_set_scan_parameters(bool scan_is_active, uint32_t window, uint32_t interval) {
	if (scan_is_active) {
		ESP_LOGI(TAG, "ble scan started");
	}
	else {
		ESP_LOGI(TAG, "ble scan stopped");
	}
}


void gatt_set_mode(gatt_mode_t mode) {
	current_gatt_mode = mode;
}

gatt_mode_t gatt_get_mode(void) {
	return current_gatt_mode;
}
