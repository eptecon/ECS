#ifndef SPI_H
#define SPI_H

typedef enum {
	SPI_SLAVE_RECEIVE,
	SPI_SLAVE_RSP
} spi_slave_state_t;

void spi_init(void);
void spi_write_byte(uint8_t b);

#endif /* SPI_H */
