#ifndef APP_FLASH_H
#define APP_FLASH_H

#define ECDH_KEY_LEN 32

#include <stdbool.h>
#include "esp_err.h"

#include "esp_wifi_types.h"

esp_err_t app_flash_init(void);
bool app_flash_load_ecdh_keys(char *pub_key, char *priv_key);
esp_err_t app_flash_save_ecdh_keys(const char *pub_key, const char *priv_key);

esp_err_t app_flash_save_first_boot_time(void);
bool app_flash_load_first_boot_time(time_t *time);

esp_err_t app_flash_save_eddystone_time_params(uint32_t time_in_prior_boots, uint32_t time_since_last_boot);
bool app_flash_load_eddystone_time_params(uint32_t *time_in_prior_boots, uint32_t *time_since_last_boot);

esp_err_t app_flash_save_wifi_sta_settings(wifi_sta_config_t *sta);
esp_err_t app_flash_load_wifi_sta_settings(wifi_sta_config_t *sta);


esp_err_t string_save_to_flash(char *partition_name, char *src, size_t size);
esp_err_t string_read_from_flash(char *partition_name, char *dst, size_t size);


#endif /* APP_FLASH_H */
