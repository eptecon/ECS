#ifndef RFID_H
#define RFID_H

void rfid_task(void *arg);
int rfid_set(const uint8_t rfid[64]);
int rfid_send(void);
int rfid_scan(uint8_t rfid_scanned[64]);

#endif /* RFID_H */
