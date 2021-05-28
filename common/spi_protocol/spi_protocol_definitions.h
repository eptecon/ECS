#ifndef SPI_PROTOCOL_DEFINITIONS_H
#define SPI_PROTOCOL_DEFINITIONS_H

#define START_BYTE 0x7e
#define STOP_BYTE 0x7e

/* Commands */

/* from ESC to Device */
#define DEVICE_INFO_REQ	0x00 /* Request device information */
#define RFID_REQ 	0x02 /* Request RFID scan */
#define DEVICE_CTRL_REQ 0x04 /* Request to control the device */

/* from Device to ESC */
#define DEVICE_INFO_RES 0x01 /* Response to device information request */
#define RFID_RES	0x03 /* Response to RFID scan request */
#define DEVICE_CTRL_RES	0x05 /* Confirmation response to device control request */




/* Data Exchange Items */

/* Device Info */
#define DEVICE_NAME	0x10 /* Device name, < 255 bytes */
#define RFID_SCAN 	0x11 /* RFID scan, < 255 bytes */
#define ERROR_CODE	0x12 /* Error code, 6 bytes */
#define TOT_OP_TIME	0x13 /* Total operating time, 6 bytes */
#define DEVICE_FW_VERSION	0x14 /* Firmware version. < 255 bytes */

/* Boolean Sensors */
#define DEVICE_STATUS	0x20 /* Device status, 1 byte */
#define SWITCH_1_STATUS	0x21 /* Switch 1 status, 1 byte */
#define SWITCH_2_STATUS	0x22 /* Switch 2 status, 1 byte */
#define SWITCH_3_STATUS	0x23 /* Switch 3 status, 1 byte */
#define SWITCH_4_STATUS	0x24 /* Switch 4 status, 1 byte */
#define SWITCH_5_STATUS	0x25 /* Switch 5 status, 1 byte */

/* Analog Sensons */
#define TEMP_SENSOR_1	0x30 /* Temperature Sensor 1, 4 bytes */
#define TEMP_SENSOR_2	0x31 /* Temperature Sensor 2, 4 bytes */
#define RESERVED_1	0x32
#define RESERVED_2	0x33
#define MOTOR_ROT_SPEED	0x34 /* Motor rotating speed, 4 bytes */

/* Controls */
#define DEVICE_OFF	0x40 /* Device off, 1 byte */
#define DEVICE_ON	0x41 /* Device on, 1 byte */
#define DEVICE_START	0x42 /* Device start, 1 byte */
#define REMOTE_RFID	0x44 /* Remote RFID, 64 bytes */


#endif /* SPI_PROTOCOL_DEFINITIONS_H */
