#ifndef SPI_PROTOCOL_HANDLERS_H
#define SPI_PROTOCOL_HANDLERS_H

void spi_protocol_bufferize_response(spi_prot_slave_ctx_t *ctx);
int spi_protocol_get_data_item(uint8_t item, void *out_buf);

#endif /* SPI_PROTOCOL_HANDLERS_H */
