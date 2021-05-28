#ifndef BLE_TASK_H
#define BLE_TASK_H

typedef enum {
	BLE_SCAN_N,
	BLE_IBEACON_N,
	BLE_EURL_N,
	BLE_EUID_N,
	BLE_EEID_N,
	BLE_ETLM_N,
	BLE_UGATT_N,
	BLE_MAX_MODE_N
} ble_mode_t;



void ble_task(void *arg);
void ble_task_mode_set(ble_mode_t mode, bool state);
void ble_task_apply_modes(void);
void ble_task_start(void);
void ble_task_suspend(void);
void ble_task_resume(void);
#endif /* BLE_TASK_H */
