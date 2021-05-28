#ifndef FLASH_H
#define FLASH_H

#define FLASH_NV_BLOB_ADDR 0x08007000
#define FLASH_NV_BLOB_MAGIC (uint16_t)0xdeadbeef

typedef struct nv_blob 	{
	uint16_t nv_blob_magic;
	uint8_t rfid[64];
	uint8_t device_state;
} __attribute__((aligned (2))) nv_blob_t;

void flash_init(void);
int flash_write_nv_blob(void);
nv_blob_t *flash_get_nv_blob(void);

#endif /* FLASH_H */
