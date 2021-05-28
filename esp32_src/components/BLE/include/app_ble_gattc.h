#ifndef BLE_GATTC_H
#define BLE_GATTC_H

#include <esp_err.h>
#include <esp_gap_ble_api.h>


typedef enum {
	GATT_CLIENT,
	GATT_SERVER
} gatt_mode_t;


void ble_client_app_register(void);
void ble_set_scan_parameters(bool scan_is_active, uint32_t window, uint32_t interval);
void ble_client_init(void);
void ble_client_deinit(void);

void gatt_set_mode(gatt_mode_t mode);
gatt_mode_t gatt_get_mode(void);


#endif /* BLE_GATTC_H */
