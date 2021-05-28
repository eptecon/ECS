#ifndef BLE_UTILS
#define BLE_UTILS

#define TX_PWR_UNINITIALIZED 0x7f

int8_t ble_get_calibrated_tx_power_1m(void);
int8_t ble_get_calibrated_tx_power_0m(void);
void ble_set_calibrated_tx_power_1m(int8_t level);
void ble_set_tx_power(char *tx_pwr);

#endif /* BLE_UTILS */ 
