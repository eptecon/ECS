#ifndef APP_SPI_H
#define APP_SPI_H

esp_err_t app_spi_init(void);

esp_err_t app_spi_device_ctrl_req(uint8_t item, uint8_t *out_buf, size_t *size);
esp_err_t app_spi_device_ctrl_remote_rfid(const uint8_t remote_rfid[64], uint8_t *ret_state);

void app_spi_test(void);

#endif /* APP_SPI_H */
