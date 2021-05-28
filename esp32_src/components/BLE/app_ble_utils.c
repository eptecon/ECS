#include <stdlib.h>
#include <stdint.h>

#include "esp_log.h"

#include "app_ble_utils.h"

#define TX_MEASURED_MINUS_21DBM_1M (-55)

extern int8_t calibrated_tx_power_1m;

/* 
 * Get the the current transmit power level in dBm at 1m distance from ESP32.
 * The level ranges from -100 dBm to +20 dBm to a resolution of 1 dBm 
 */
int8_t ble_get_calibrated_tx_power_1m(void) {
	return calibrated_tx_power_1m;
}

/* 
 * Get the the current transmit power level in dBm at 0m distance from ESP32.
 * The level ranges from -100 dBm to +20 dBm to a resolution of 1 dBm 
 */
int8_t ble_get_calibrated_tx_power_0m(void) {
	return (calibrated_tx_power_1m + 41);
}


/* 
 * Set the the current transmit power level in dBm at 1m distance from ESP32.
 * The level ranges from -100 dBm to +20 dBm to a resolution of 1 dBm 
 */
void ble_set_calibrated_tx_power_1m(int8_t level) {
	calibrated_tx_power_1m = level;
}

/*
 * As we can set up only a few tx_pwr levels,
 * then we need to associate available tx_pwr values
 * with measured calibrated value.
 */
void ble_set_tx_power(char *tx_pwr) {
	int i = atoi(tx_pwr);
	int8_t level;

	if (i < -21)
		i = -21;
	if (i > 9)
		i = 9;
	
	if (i >= -21 && i < -15) {
		level = TX_MEASURED_MINUS_21DBM_1M;
	}
	
	if (i >= -15 && i < -7) {
		level = TX_MEASURED_MINUS_21DBM_1M;
	}

	if (i >= -7 && i < 1) {
		level = TX_MEASURED_MINUS_21DBM_1M;
	}

	if (i >= 1 && i <= 9) {
		level = TX_MEASURED_MINUS_21DBM_1M;
	}

	ble_set_calibrated_tx_power_1m(level);
}
//BTM_BLE_ADV_TX_POWER {-21, -15, -7, 1, 9}

