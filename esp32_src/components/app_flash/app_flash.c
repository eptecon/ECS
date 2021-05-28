#include <string.h>

#include <time.h>
#include <sys/time.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_partition.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "esp_wifi_types.h"

#include "app_flash.h"

#define SECTION_NAME "storage"

#define KEY_FIRST_BOOT_TIME "first_boot_time"
#define KEY_ECDH_PUB_KEY "ecdh_pub_key"
#define KEY_EDDYSTONE_TIME_PARAMS "eddystone_time_params"
#define KEY_WIFI_STA_SETTINGS "sta_settings"

static const char *TAG = "storage";

esp_err_t app_flash_init() {
	return nvs_flash_init();
}

bool app_flash_load_ecdh_keys(char *pub_key, char *priv_key) {

	nvs_handle my_handle;
	esp_err_t err;
	size_t required_size = 0;

	char key[32] = KEY_ECDH_PUB_KEY;
	char buffer[ECDH_KEY_LEN * 2];

	err = nvs_open(SECTION_NAME, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "%s: can't open nvs", __FUNCTION__);
		return false;
	}

	err = nvs_get_blob(my_handle, key, NULL, &required_size);
	if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
		ESP_LOGE(TAG, "%s: Can't get wifi info size\n", __FUNCTION__);
		return false;
	}

	if (required_size > 0) {
		err = nvs_get_blob(my_handle, key, buffer, &required_size);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "app_flash_load_wifi_info: Can't get wifi info data, size=%d\n", required_size);
			return false;
		}

		memcpy(pub_key, &buffer[0], ECDH_KEY_LEN);
		memcpy(priv_key, &buffer[ECDH_KEY_LEN], ECDH_KEY_LEN);
	}
	else 
		return false;

	return true;
}

esp_err_t app_flash_save_ecdh_keys(const char *pub_key, const char *priv_key) {

	nvs_handle my_handle;
	esp_err_t err;
	char key[32] = KEY_ECDH_PUB_KEY;
	char buf[ECDH_KEY_LEN * 2];

	memcpy(buf, pub_key, ECDH_KEY_LEN);
	memcpy(buf, priv_key + ECDH_KEY_LEN, ECDH_KEY_LEN);

	err = nvs_open(SECTION_NAME, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "nvs_open fail\n");
		return err;
	}


	err = nvs_set_blob(my_handle, key, buf, ECDH_KEY_LEN * 2);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "nvs_set_blob fail\n");
		return err;
	}

	err = nvs_commit(my_handle);
	if (err != ESP_OK)
		return err;

	nvs_close(my_handle);
	return ESP_OK;
}

bool app_flash_load_first_boot_time(time_t *time) {

	nvs_handle my_handle;
	esp_err_t err;
	
	size_t required_size = 0;

	char key[32] = KEY_FIRST_BOOT_TIME;

	err = nvs_open(SECTION_NAME, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "%s: can't open nvs", __FUNCTION__);
		return false;
	}

	err = nvs_get_blob(my_handle, key, NULL, &required_size);
	if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
		ESP_LOGE(TAG, "%s: Can't get first boot time size\n", __FUNCTION__);
		return false;
	}

	if (required_size > 0) {
		err = nvs_get_blob(my_handle, key, time, &required_size);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "%s: Can't get first boot time info data, err=%d\n", __FUNCTION__, err);
			return false;
		}
	}
	else 
		return false;

	return true;
}

esp_err_t app_flash_save_first_boot_time(void) {

	nvs_handle my_handle;
	esp_err_t err;

	char key[32] = KEY_FIRST_BOOT_TIME;

	time_t now;
	time(&now);

	err = nvs_open(SECTION_NAME, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "nvs_open fail\n");
		return err;
	}

	err = nvs_set_blob(my_handle, key, &now, sizeof(now));
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "nvs_set_blob fail\n");
		return err;
	}

	err = nvs_commit(my_handle);
	if (err != ESP_OK)
		return err;

	nvs_close(my_handle);
	return ESP_OK;
}

esp_err_t app_flash_save_eddystone_time_params(uint32_t time_in_prior_boots, uint32_t time_since_last_boot) {

	nvs_handle my_handle;
	esp_err_t err;

	char key[32] = KEY_EDDYSTONE_TIME_PARAMS;

	char buf[8];

	memcpy(buf, &time_in_prior_boots, sizeof(uint32_t));
	memcpy(buf + sizeof(uint32_t), &time_since_last_boot, sizeof(uint32_t));

	err = nvs_open(SECTION_NAME, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "nvs_open fail\n");
		return err;
	}

	err = nvs_set_blob(my_handle, key, buf, sizeof(buf));
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "nvs_set_blob fail\n");
		return err;
	}

	err = nvs_commit(my_handle);
	if (err != ESP_OK)
		return err;

	nvs_close(my_handle);
	return ESP_OK;
}

bool app_flash_load_eddystone_time_params(uint32_t *time_in_prior_boots, uint32_t *time_since_last_boot) {
	nvs_handle my_handle;
	esp_err_t err;
	
	size_t required_size = 0;

	char key[32] = KEY_EDDYSTONE_TIME_PARAMS;

	char buf[8];

	err = nvs_open(SECTION_NAME, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "%s: can't open nvs", __FUNCTION__);
		return false;
	}

	err = nvs_get_blob(my_handle, key, NULL, &required_size);
	if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
		ESP_LOGE(TAG, "%s: Can't get first boot time size\n", __FUNCTION__);
		return false;
	}

	if (required_size > 0) {
		err = nvs_get_blob(my_handle, key, buf, &required_size);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "%s: Can't get eddystone time params, err=%d\n", __FUNCTION__, err);
			return false;
		}
	}
	else 
		return false;

	memcpy(time_in_prior_boots, buf, sizeof(uint32_t));
	memcpy(time_since_last_boot, buf + sizeof(uint32_t), sizeof(uint32_t));

	return true;

}

esp_err_t app_flash_load_wifi_sta_settings(wifi_sta_config_t *sta) 
{
	nvs_handle my_handle;
	esp_err_t err;
	size_t required_size = 0;

	char key[16] = KEY_WIFI_STA_SETTINGS;
	char buffer[sizeof(wifi_sta_config_t)];

	err = nvs_open(SECTION_NAME, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "%s: can't open nvs", __FUNCTION__);
		return ESP_FAIL;
	}

	err = nvs_get_blob(my_handle, key, NULL, &required_size);
	if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
		ESP_LOGE(TAG, "%s: Can't get wifi info size\n", __FUNCTION__);
		return ESP_FAIL;
	}

	if (required_size > 0) {
		err = nvs_get_blob(my_handle, key, buffer, &required_size);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "app_flash_load_wifi_sta_settings: Can't get wifi info data, size=%d\n", required_size);
			return ESP_FAIL;
		}

		memcpy(sta, buffer, sizeof(wifi_sta_config_t));
	}
	else 
		return ESP_FAIL;

	return ESP_OK;
}

esp_err_t app_flash_save_wifi_sta_settings(wifi_sta_config_t *sta) {

	nvs_handle my_handle;
	esp_err_t err;

	char key[32] = KEY_WIFI_STA_SETTINGS;
	
	err = nvs_open(SECTION_NAME, NVS_READWRITE, &my_handle);

	if (err != ESP_OK) {
		ESP_LOGE(TAG, "nvs_open fail\n");
		return err;
	}

	err = nvs_set_blob(my_handle, key, sta, sizeof(wifi_sta_config_t));
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "nvs_set_blob fail, err = %x\n", err);
		return err;
	}

	err = nvs_commit(my_handle);
	if (err != ESP_OK)
		return err;

	nvs_close(my_handle);
	return ESP_OK;
}


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

