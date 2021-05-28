#ifndef BLE_IBEACON_H
#define BLE_IBEACON_H

void ibeacon_adv_data_set_default(void);
void ibeacon_set_adv_data(uint16_t maj, uint16_t min, uint16_t interval, char *uuid);

#endif /* BLE_IBEACON_H */
