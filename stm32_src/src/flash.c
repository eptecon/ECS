#include <string.h>

#include "stm32f0xx_flash.h"

#include "flash.h"

#include "FreeRTOS.h"

static nv_blob_t nv;

void flash_init(void) {
	memcpy(&nv, (void*)FLASH_NV_BLOB_ADDR, sizeof(nv_blob_t));
}

int flash_write_nv_blob(void) {

	int rc = 0;
	uint16_t hw;

	nv.nv_blob_magic = FLASH_NV_BLOB_MAGIC;

	vPortEnterCritical();

	FLASH_Unlock();

	if (FLASH_ErasePage(FLASH_NV_BLOB_ADDR) != FLASH_COMPLETE) {
		rc = -2;
		goto exit;
	}

	for (int i = 0; i < sizeof(struct nv_blob) / 2; i += 1) {

		while (FLASH_GetFlagStatus(FLASH_FLAG_EOP) == RESET);
		while (FLASH_GetFlagStatus(FLASH_FLAG_BSY) == SET);

		hw = *((uint16_t*)&nv + i);

		if ((FLASH_ProgramHalfWord(FLASH_NV_BLOB_ADDR + i * 2, hw)) != FLASH_COMPLETE) {
			rc = -1;
			goto exit;
		}
	}

exit:
	FLASH_Lock();
	vPortExitCritical();

	return rc;
}

nv_blob_t *flash_get_nv_blob(void) {
	return &nv;
}
