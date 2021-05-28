#ifndef FLASH_ROUTINES_H
#define FLASH_ROUTINES_H

esp_err_t string_save_to_flash(char *partition_name, char *src, size_t size);
esp_err_t string_read_from_flash(char *partition_name, char *dst, size_t size);

#endif /* FLASH_ROUTINES_H */
