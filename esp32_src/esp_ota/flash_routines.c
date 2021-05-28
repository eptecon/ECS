#include "esp_log.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_system.h"

static const char *TAG = "flash_routines";


esp_err_t string_save_to_flash(char *partition_name, char *src, size_t size) {
	esp_err_t err = ESP_OK;

	const esp_partition_t *ota_data = esp_partition_find_first(0x40, 0x00, partition_name);
			
	err = esp_partition_erase_range(ota_data, 0, ota_data->size);
	if (ESP_OK != err) {
		ESP_LOGE(TAG, "failed to erase partition, ret = 0x%x", err);
		return err;
	}
	
	err = esp_partition_write(ota_data, 0, src, size);
	if (ESP_OK != err) {
		ESP_LOGE(TAG, "failed to write ota_data partition, ret = 0x%x", err);
		return err;
	}
	return err;
}

esp_err_t string_read_from_flash(char *partition_name, char *dst, size_t size) {
	esp_err_t err = ESP_OK;

	const esp_partition_t *ota_data = esp_partition_find_first(0x40, 0x00, partition_name);

	err = esp_partition_read(ota_data, 0, dst, size);
	if (ESP_OK != err) {
		ESP_LOGE(TAG, "failed to read ota_data partition");
		return err;
	}
	return err;
}



