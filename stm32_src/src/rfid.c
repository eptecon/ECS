#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "rfid.h"
#include "flash.h"

#include "tinyprintf.h"

void rfid_task(void *arg) {

	(void) arg;

	while (true) {
		rfid_send();
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

int rfid_set(const uint8_t rfid[64]) {

	nv_blob_t *nv = flash_get_nv_blob();

	memcpy(nv->rfid, rfid, 64);

	flash_write_nv_blob();

	return 0;
}

int rfid_send(void) {

	nv_blob_t *nv = flash_get_nv_blob();

	/*
	 * check that RFID value has been saved into flash
	 */
	if (nv->nv_blob_magic != FLASH_NV_BLOB_MAGIC)
		return -1;

	printf("=======================================\nRFID:");
	for (int i = 0; i < 64; i++) {
		if (i % 8 == 0)
			printf("\n");
		printf("0x%02X ", nv->rfid[i]);
	}
	printf("\n=======================================\n");

	return 0;
}


int rfid_scan(uint8_t rfid_scanned[64]) {
	
	for (int i = 0; i < 64; i++) {
		rfid_scanned[i] = i;
	}

	return 0;
}

