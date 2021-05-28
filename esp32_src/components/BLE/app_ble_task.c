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

//#include "sntp_task.h"

#include "app_ble_gattc.h"
#include "app_ble_gap.h"
#include "app_ble_ibeacon.h"
//#include "ble_eddystone.h"
#include "app_ble_task.h"

#define BLE_PERIOD_MS 100 
#define BLE_GAP_SCAN_DURATION 1

#define PDU_PER_MIN_WINDOW 3

static char ble_windows[BLE_MAX_MODE_N] = { [0 ... BLE_MAX_MODE_N - 1] = 0 };
static bool ble_modes[BLE_MAX_MODE_N] = { [0 ... BLE_MAX_MODE_N - 1] = false};


extern uint8_t ibcn_adv_data[31];
extern uint8_t ibcn_adv_data_len;

extern uint8_t e_url_adv_data[31];
extern uint8_t e_url_adv_data_len;

extern uint8_t e_uid_adv_data[20];

extern uint8_t e_tlm_adv_data[31];
extern uint8_t e_tlm_adv_data_len;

extern uint8_t e_eid_adv_data[31];
extern uint8_t e_eid_adv_data_len;

static TaskHandle_t ble_task_handler = NULL;

void ble_task_mode_set(ble_mode_t mode, bool state) {
	ble_modes[mode] = state;
}

void ble_task_apply_modes(void) {
	uint8_t divider = 0, mode_wdw = 0;
	uint8_t i;

	/*
	 * Calculate the divider value;
	 * just divide BLE_PERIOD_MS by modes that 'on'.
	 */
	for (i = 0; i < BLE_MAX_MODE_N; i++) {
		if (ble_modes[i])
			divider++;
	}

	if (divider != 0)
		mode_wdw = (BLE_PERIOD_MS / divider);

	for (i = 0; i < BLE_MAX_MODE_N; i++) {
		ble_windows[i] = mode_wdw;
	}
	
	/*
	 * But if all is 'on', set ETLM mode window to 10ms
	 */
	if (divider == BLE_MAX_MODE_N) {
		ble_windows[BLE_ETLM_N] = 10;
	}

	for (i = 0; i < BLE_MAX_MODE_N; i++) {
		ESP_LOGI(TAG, "mode %d state: %d, window: %d", i, ble_modes[i], ble_windows[i]);
	}
}

void ble_task(void *arg) {
	(void) arg;

	int i = 0;
	esp_err_t err;

	esp_ble_adv_params_t advParams = { 0 };
	advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
	advParams.adv_int_max = 0x0100;
	advParams.adv_int_min = 0x0100;
	advParams.adv_type = ADV_TYPE_NONCONN_IND;
	advParams.channel_map = ADV_CHNL_ALL;
	advParams.own_addr_type = BLE_ADDR_TYPE_PUBLIC;

	for (;;) {
		
		if (gatt_get_mode() == GATT_CLIENT) {
#if 0					
			if (ble_modes[BLE_SCAN_N]) {
				//esp_ble_gap_start_scanning(ble_windows[BLE_SCAN_N] / portTICK_RATE_MS);
				err = user_gap_start_scanning(ble_windows[BLE_SCAN_N] / portTICK_RATE_MS);
				if (err != ESP_OK) {
					ESP_LOGE("ble_tsk", "user_gap_start_scanning err = %d", err);
					continue;
				}

				vTaskDelay(ble_windows[BLE_SCAN_N] / portTICK_RATE_MS);

				//esp_ble_gap_stop_scanning();
				err = user_gap_stop_scanning();
				if (err != ESP_OK) {
					ESP_LOGE("ble_tsk", "user_gap_stop_scanning err = %d", err);
					continue;
				}
			}
#endif
//			if (ble_modes[BLE_IBEACON_N]) {
				err = user_gap_config_adv_data_raw(ibcn_adv_data, ibcn_adv_data_len);
				if (err != ESP_OK) {
					ESP_LOGE("ble_tsk", "user_gap_config_adv_data_raw err = %d", err);
					continue;
				}

				err = user_gap_start_advertising(&advParams);
				if (err != ESP_OK) {
					ESP_LOGE("ble_tsk", "esp_ble_gap_start_advertising err %d", err);
				}

				vTaskDelay(500 / portTICK_RATE_MS);
				
				err = user_gap_stop_advertising();
				if (err != ESP_OK) {
					ESP_LOGE("ble tsk", "esp_ble_gap_stop_advertising err %d", err);
				}
//			}
#if 0
			if (ble_modes[BLE_EURL_N]) {


				err = user_gap_config_adv_data_raw(e_url_adv_data, e_url_adv_data_len);
				if (err != ESP_OK) {
					ESP_LOGE("ble_tsk", "user_gap_config_adv_data_raw err = %d", err);
					continue;
				}
				
				err = user_gap_start_advertising(&advParams);
				if (err != ESP_OK) {
					ESP_LOGE("ble tsk", "user_gap_start_advertising err %d", err);
				}

				vTaskDelay(ble_windows[BLE_EURL_N] / portTICK_RATE_MS);

				err = user_gap_stop_advertising();
				if (err != ESP_OK) {
					ESP_LOGE("ble tsk", "esp_ble_gap_stop_advertising err %d", err);
				}
			}

			if (ble_modes[BLE_EUID_N]) {


				err = user_gap_config_adv_data_raw(e_uid_adv_data, 31);
				if (err != ESP_OK) {
					ESP_LOGE("ble_tsk", "user_gap_config_adv_data_raw err = %d", err);
					continue;
				}

				err = user_gap_start_advertising(&advParams);
				if (err != ESP_OK) {
					ESP_LOGE("ble tsk", "user_gap_start_advertising err %d", err);
					continue;
				}

				vTaskDelay(ble_windows[BLE_EUID_N] / portTICK_RATE_MS);

				err = user_gap_stop_advertising();
				if (err != ESP_OK) {
					ESP_LOGE("ble tsk", "user_gap_stop_advertising err %d", err);
					continue;
				}
			}
			if (ble_modes[BLE_ETLM_N]) {


				etlm_set_adv_data();

				err = user_gap_config_adv_data_raw(e_tlm_adv_data, 25);
				if (err != ESP_OK) {
					ESP_LOGI("ble_tsk", "e_tlm_adv_data_len = %d", e_tlm_adv_data_len);
					ESP_LOGE("ble_tsk", "user_gap_config_adv_data_raw err = %d", err);
					continue;
				}

				err = user_gap_start_advertising(&advParams);
				if (err != ESP_OK) {
					ESP_LOGE("ble tsk", "esp_ble_gap_start_advertising err %d", err);
					continue;
				}

				vTaskDelay(ble_windows[BLE_ETLM_N] / portTICK_RATE_MS);

				err = user_gap_stop_advertising();
				if (err != ESP_OK) {
					ESP_LOGE("ble tsk", "esp_ble_gap_stop_advertising err %d", err);
					continue;
				}
			}
			if (ble_modes[BLE_EEID_N]) {


				eeid_set_adv_data();

				err = user_gap_config_adv_data_raw(e_eid_adv_data, 21);
				if (err != ESP_OK) {
					ESP_LOGI("ble_tsk", "e_eid_adv_data_len = %d", e_tlm_adv_data_len);
					ESP_LOGE("ble_tsk", "user_gap_config_adv_data_raw err = %d", err);
					continue;
				}

				err = user_gap_start_advertising(&advParams);
				if (err != ESP_OK) {
					ESP_LOGE("ble tsk", "esp_ble_gap_start_advertising err %d", err);
					continue;
				}

				vTaskDelay(ble_windows[BLE_EEID_N] / portTICK_RATE_MS);

				err = user_gap_stop_advertising();
				if (err != ESP_OK) {
					ESP_LOGE("ble tsk", "esp_ble_gap_stop_advertising err %d", err);
					continue;
				}
			}
#endif
		}


#if 0
		/*
		 * If no mode is turned on, then wait for 50ms to give
		 * parallel tasks a chance
		 */
		for (i = 0; i < BLE_MAX_MODE_N; i++) {
			if (ble_modes[i])
				break;
		}

		if (i == BLE_MAX_MODE_N)
			vTaskDelay(50);
#endif
		(void)i;
		vTaskDelay(50 / portTICK_RATE_MS);
	}

}

#if 0
void ble_task_start(void) {
	xTaskCreatePinnedToCore(&ble_task, "ble_task", 2048, NULL, 5, &ble_task_handler, 0);
}
#endif

#if 0
void ble_task_suspend(void) {
	if (ble_task_handler != NULL) {
		vTaskSuspend(ble_task_handler);
	}
}
#endif

#if 0
void ble_task_resume(void) {
	if (ble_task_handler != NULL) {
		vTaskResume(ble_task_handler);
	}
}
#endif
