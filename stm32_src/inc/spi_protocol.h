#ifndef SPI_PROTOCOL_H
#define SPI_PROTOCOL_H

/*
 * Used for printing the current state and current received byte.
 * Doesn't affect slave performance, just covers some code/data and saves space.
 */
//#define SPI_PROT_DBG

/*
 * Used for internal MOSI transmission emulation and testing the slave behaviour.
 * Once defined, hardware SPI is not initialized
 */
//#define SPI_PROT_TEST


#ifdef SPI_PROT_DBG
	#define LOG_DBG(format, ...) printf(format, __VA_ARGS__)

	#define STATE_MACHINE_PRINT_BYTE() do { 				\
						printf("byte: %02X\n", byte);	\
					} while(0);

	#define PRINT_LINE() do {						\
					printf("[%d]\n", __LINE__);		\
					} while (0);

					

	#define PRINT_CURRENT_STATE() do {					\
					char str[32];				\
					decode_state(spi_prot_state, str);	\
					printf("state: %s\n", str);		\
					} while(0);

	#define LIST_REQ() do {							\
				int i = 0;					\
				for (; i < ctx->rq_len; i++) {			\
					printf("0x%02X ", ctx->rq_data[i]);	\
				}						\
				printf("\n");					\
			 } while (0)

#else 
	#define LOG_DBG(format, ...)
	#define PRINT_LINE()
	#define PRINT_CURRENT_STATE()
	#define STATE_MACHINE_PRINT_BYTE()
	#define LIST_REQ()
#endif /* SPI_PROT_DBG */




void spi_task_init(void);

struct spi_handlers_str {
	int (*scan_rfid_cb)(uint8_t rfid_scanned[64]);
	int (*set_rfid_cb)(const uint8_t const rfid[64]);
	void (*device_on_cb)(void);
	void (*device_off_cb)(void);
	bool (*switch_handler_cb)(uint8_t switch_num);
	void (*temperature_sensor_handler_cb)(uint8_t sensor_num, uint8_t temperature[4]);
};
#endif /* SPI_PROTOCOL_H */
