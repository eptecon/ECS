#ifndef SPI_PROTOCOL_SLAVE_INTERNALS_H
#define SPI_PROTOCOL_SLAVE_INTERNALS_H

/* 
 * Length of buffer where request data is saved
 */
#define SPI_PROT_RQ_BUF_LEN 256

/*
 * Length of buffer where response data is saved
 */
#define SPI_PROT_RESP_BUF_LEN 256



/*
 * Slave state machine
 */
typedef enum {
	SPI_PROT_STATE_IDLE,
	SPI_PROT_COMMAND,
	SPI_PROT_DATA_ITEM,
	SPI_PROT_DATA,
	SPI_PROT_CHECKSUM,
	SPI_PROT_STOP,
} spi_prot_state_slave_t;

/*
 * Slave context data.
 * Used to intercommunicate between a different processor contexts
 */
typedef struct {
	uint8_t rq_command;
	uint8_t rq_data_item;
	uint8_t rq_index;
	uint8_t rq_len;
	uint8_t *rq_data;

	uint8_t checksum;

	uint8_t *resp_buf;
	uint16_t resp_index;
	uint16_t resp_len;
} spi_prot_slave_ctx_t;

/*
 * SPI received byte polling task
 */
void spi_task(void *arg);

/*
 * Slave state management. Used in SPI rx IRQ handling.
 * According to protocol, slave can be either in listening state, when it
 * receives a data from master, or in respoinding state, when SPI receive task stops,
 * response data is already bufferised and all the slave do is consequently puts a response byte-by-byte
 * on each RX IRQ
 */
void spi_switch_state(spi_slave_state_t state);
spi_slave_state_t spi_slave_get_state(void);

/*
 * Used to handle MOSI byte in IRQ
 */
void spi_buffer_mosi(uint8_t byte);

/*
 * Read next unread byte from SPI rx buffer
 */
int spi_read_byte_from_buffer(uint8_t *data);
/*
 * Bufferize slave response.
 * After command reception and required data processing,
 * all the response is being bufferized and spi task is turned off.
 * Master then reads this response in further interrupts.
 */
void spi_protocol_slave_buffer_response(spi_prot_slave_ctx_t *ctx);


/*
 * The entry point for SPI received byte processing (in a task context)
 */
void spi_protocol_handler(uint8_t byte);

/*
 * Used for testing
 */
void spi_irq_emulation_task(void *arg);

void decode_state(spi_prot_state_slave_t state, char *out_str);
void spi_protocol_slave_do_response_emulation(void);

#endif /* SPI_PROTOCOL_SLAVE_INTERNALS_H */
