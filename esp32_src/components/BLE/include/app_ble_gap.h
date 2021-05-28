#ifndef BLE_GAP_H
#define BLE_GAP_H

#include <esp_err.h>
#include <esp_gap_ble_api.h>

esp_err_t app_ble_gap_init(void (*scan_result_handler_cb)(esp_ble_gap_cb_param_t *scan_result));

esp_err_t user_gap_config_adv_data_raw(uint8_t *adv_data, uint8_t adv_data_len);
esp_err_t user_gap_start_advertising(esp_ble_adv_params_t *adv_params);
esp_err_t user_gap_stop_advertising(void);

#endif /* BLE_GAP_H */
