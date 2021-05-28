#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stdbool.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bt.h"
#include "app_ble_utils.h"

#include "esp_log.h"

const static char *TAG = "ibeacon";


/* default adv period definitions */
#define IBEACON_MIN_PERIOD 100
#define IBEACON_MAX_PERIOD 5000
#define IBEACON_DEFAULT_PERIOD 1000

static uint16_t ibeacon_adv_period = IBEACON_DEFAULT_PERIOD;

/* advertisement data array */
uint8_t ibcn_adv_data[31];
uint8_t ibcn_adv_data_len = 30;
static void ibeacon_debug_print_adv_data(void) __attribute((used));
#define HCI_H4_CMD_PREAMBLE_SIZE		   (4)

/*  HCI Command opcode group field(OGF) */
#define HCI_GRP_HOST_CONT_BASEBAND_CMDS	(0x03 << 10)			/* 0x0C00 */
#define HCI_GRP_BLE_CMDS				   (0x08 << 10)

#define HCI_RESET						  (0x0003 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_BLE_WRITE_ADV_ENABLE		   (0x000A | HCI_GRP_BLE_CMDS)
#define HCI_BLE_WRITE_ADV_PARAMS		   (0x0006 | HCI_GRP_BLE_CMDS)
#define HCI_BLE_WRITE_ADV_DATA			 (0x0008 | HCI_GRP_BLE_CMDS)

#define HCIC_PARAM_SIZE_WRITE_ADV_ENABLE		(1)
#define HCIC_PARAM_SIZE_BLE_WRITE_ADV_PARAMS	(15)
#define HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA	  (31)

#define BD_ADDR_LEN	 (6)					 /* Device address length */
typedef uint8_t bd_addr_t[BD_ADDR_LEN];		 /* Device address */

#define UINT16_TO_STREAM(p, u16) {*(p)++ = (uint8_t)(u16); *(p)++ = (uint8_t)((u16) >> 8);}
#define UINT8_TO_STREAM(p, u8)   {*(p)++ = (uint8_t)(u8);}
#define BDADDR_TO_STREAM(p, a)   {int ijk; for (ijk = 0; ijk < BD_ADDR_LEN;  ijk++) *(p)++ = (uint8_t) a[BD_ADDR_LEN - 1 - ijk];}
#define ARRAY_TO_STREAM(p, a, len) {int ijk; for (ijk = 0; ijk < len;		ijk++) *(p)++ = (uint8_t) a[ijk];}

enum {
	H4_TYPE_COMMAND = 1,
	H4_TYPE_ACL	 = 2,
	H4_TYPE_SCO	 = 3,
	H4_TYPE_EVENT   = 4
};


void ibeacon_set_period(uint16_t period) {
	if (period < IBEACON_MIN_PERIOD) {
		ibeacon_adv_period = IBEACON_MIN_PERIOD;
	}
	else if (period > IBEACON_MIN_PERIOD) {
		ibeacon_adv_period = IBEACON_MAX_PERIOD;
	}
	else
		ibeacon_adv_period = period;
}

/*
 * TODO: remove
 */
void ibeacon_adv_data_set_default(void) {
	ibcn_adv_data[0] = 2;	  // Len
	ibcn_adv_data[1] = 0x01;   // Type Flags
	ibcn_adv_data[2] = 0x04;   // BR_EDR_NOT_SUPPORTED 0x04
	ibcn_adv_data[3] = 26;	 // Len
	ibcn_adv_data[4] = 0xFF;   // Type
	ibcn_adv_data[5] = 0x4C;   // Company 2 -> fake Apple 0x004C LSB
	ibcn_adv_data[6] = 0x00;   // Company 1 MSB
	ibcn_adv_data[7] = 0x02;   // Type Beacon
	ibcn_adv_data[8] = 21;	 // Length of Beacon Data
	ibcn_adv_data[9] = 0x11;   // UUID 1 128-Bit (may use linux tool uuidgen or random numbers via https://www.uuidgenerator.net/)
	ibcn_adv_data[10] = 0x22;  // UUID 2
	ibcn_adv_data[11] = 0x33;  // UUID 3
	ibcn_adv_data[12] = 0x53;  // UUID 4
	ibcn_adv_data[13] = 0x32;  // UUID 5
	ibcn_adv_data[14] = 0x6C;  // UUID 6
	ibcn_adv_data[15] = 0x44;  // UUID 7
	ibcn_adv_data[16] = 0x23;  // UUID 8
	ibcn_adv_data[17] = 0xBB;  // UUID 9
	ibcn_adv_data[18] = 0x89;  // UUID 10
	ibcn_adv_data[19] = 0x65;  // UUID 11
	ibcn_adv_data[20] = 0x87;  // UUID 12
	ibcn_adv_data[21] = 0xAA;  // UUID 13
	ibcn_adv_data[22] = 0xEE;  // UUID 14
	ibcn_adv_data[23] = 0xEE;  // UUID 15
	ibcn_adv_data[24] = 0x07;  // UUID 16
	ibcn_adv_data[25] = 0x00;  // Major 1 Value
	ibcn_adv_data[26] = 0x20;  // Major 2 Value
	ibcn_adv_data[27] = 0x21;  // Minor 1 Value
	ibcn_adv_data[28] = 0x22;  // Minor 2 Value
	ibcn_adv_data[29] = 0xA0;  // Beacons TX power
}

void ibeacon_set_adv_data(uint16_t maj, uint16_t min, uint16_t interval, char *uuid) {
	int i;

	for (i = 0; i < 16; i++) {
		sscanf(uuid + i*2, "%2hhx", &ibcn_adv_data[9 + i]);
	}

	ibcn_adv_data[25] = (char)((maj >> 8) & 0x00ff);
	ibcn_adv_data[26] = (char)((maj) & 0x00ff);
	ibcn_adv_data[27] = (char)((min >> 8) & 0x00ff);
	ibcn_adv_data[28] = (char)((min) & 0x00ff);

	ibcn_adv_data[29] = ble_get_calibrated_tx_power_1m();
	
	ibeacon_set_period(interval);
}

static void ibeacon_debug_print_adv_data(void) {
	char tmp[4];

	tmp[0] = ibcn_adv_data[26];
	tmp[1] = ibcn_adv_data[25];
	ESP_LOGI(TAG, "MAJOR: 0d%d = 0x%02X", *(uint16_t*)tmp, *(uint16_t*)tmp);

	tmp[0] = ibcn_adv_data[28];
	tmp[1] = ibcn_adv_data[27];
	ESP_LOGI(TAG, "MINOR: 0d%d = 0x%02X", *(uint16_t*)tmp, *(uint16_t*)tmp);
}
