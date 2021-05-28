#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_gap_ble_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"

#include "esp_log.h"

#include "app_ble_gap.h"

static const char *TAG = "ble_gap";

static EventGroupHandle_t ble_evt;

static const int SCAN_START_BIT = BIT0;
static const int SCAN_STOP_BIT = BIT1;
static const int ADV_SET_BIT = BIT2;
static const int ADV_START_BIT = BIT3;
static const int ADV_STOP_BIT = BIT4;


static esp_ble_adv_params_t test_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

void (*scan_handle_cb)(esp_ble_gap_cb_param_t *scan_result) = NULL;

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
	if (event != 4 && event != 6 && event != 17) {
		ESP_LOGI("", "%s: %d", __FUNCTION__, (int)event);
	}
	switch (event) {
	case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
		esp_ble_gap_start_advertising(&test_adv_params);
		break;
	case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
		xEventGroupSetBits(ble_evt, ADV_SET_BIT);
		esp_ble_gap_start_advertising(&test_adv_params);
		break;
	case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
		xEventGroupSetBits(ble_evt, ADV_START_BIT);
		//advertising start complete event to indicate advertising start successfully or failed
		if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
			ESP_LOGE(TAG, "Advertising start failed\n");
		}
		break;
	case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
		xEventGroupSetBits(ble_evt, ADV_STOP_BIT);
		if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
			ESP_LOGE(TAG, "Advertising stop failed\n");
		}
		break;
	case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
		ESP_LOGI(TAG, "update connetion params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
			param->update_conn_params.status,
			param->update_conn_params.min_int,
			param->update_conn_params.max_int,
			param->update_conn_params.conn_int,
			param->update_conn_params.latency,
			param->update_conn_params.timeout);
		break;
	case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
		esp_ble_gap_start_advertising(&test_adv_params);
		break;
	case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
		xEventGroupSetBits(ble_evt, SCAN_START_BIT);
		//scan start complete event to indicate scan start successfully or failed
		if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
			ESP_LOGE(TAG, "Scan start failed");
			//esp_ble_gap_start_scanning(BLE_GAP_SCAN_DURATION);
		}
		break;
	case ESP_GAP_BLE_SCAN_RESULT_EVT: {
		scan_handle_cb((esp_ble_gap_cb_param_t *)param);
		break;
	}
	case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: {
		xEventGroupSetBits(ble_evt, SCAN_STOP_BIT);
		break;
	}
	default:
		break;
	}
}

esp_err_t user_gap_config_adv_data_raw(uint8_t *adv_data, uint8_t adv_data_len) {
	esp_err_t err = ESP_OK;
	err = esp_ble_gap_config_adv_data_raw((uint8_t *)adv_data, (uint32_t)adv_data_len);
	if (err != ESP_OK) {
		return err;
	}
	xEventGroupWaitBits(ble_evt, ADV_SET_BIT, true, true, 2000 / portTICK_RATE_MS);
	return err;
}

esp_err_t user_gap_start_advertising(esp_ble_adv_params_t *adv_params) {
	esp_err_t err = ESP_OK;
	err = esp_ble_gap_start_advertising(adv_params);
	if (err != ESP_OK) {
		return err;
	}
	xEventGroupWaitBits(ble_evt, ADV_START_BIT, true, true, 2000 / portTICK_RATE_MS);
	return err;
}

esp_err_t user_gap_stop_advertising(void) {
	esp_err_t err = ESP_OK;
	err = esp_ble_gap_stop_advertising();
	if (err != ESP_OK) {
		return err;
	}
	xEventGroupWaitBits(ble_evt, ADV_STOP_BIT, true, true, 2000 / portTICK_RATE_MS);
	return err;
}

esp_err_t user_gap_start_scanning(uint32_t duration) {
	esp_err_t err = ESP_OK;
	err = esp_ble_gap_start_scanning(duration);
	xEventGroupWaitBits(ble_evt, SCAN_START_BIT, true, true, 2000 / portTICK_RATE_MS);
	return err;
}

esp_err_t user_gap_stop_scanning(void) {
	esp_err_t err = ESP_OK;
	err = esp_ble_gap_stop_scanning();
	xEventGroupWaitBits(ble_evt, SCAN_STOP_BIT, true, true, 2000 / portTICK_RATE_MS);
	return err;
}

esp_err_t app_ble_gap_init(void (*scan_result_handler_cb)(esp_ble_gap_cb_param_t *scan_result)) {

	scan_handle_cb = scan_result_handler_cb;

	ble_evt = xEventGroupCreate();

	return esp_ble_gap_register_callback(gap_event_handler);
}

