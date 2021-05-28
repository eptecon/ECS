/* OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

#include <string.h>

#include "flash_routines.h"

#define BUFFSIZE 1024
#define TEXT_BUFFSIZE 1024

char credentials_buf[256];
char ap_ssid[128];
char ap_pwd[128];
uint8_t ssl_cert_buf[2000];

char http_request[512];

const uint8_t *ssl_cert_buf_start = ssl_cert_buf;

static void __attribute__((noreturn)) task_fatal_error();

static const char *TAG = "ota";
/*an ota data write buffer ready to write to the flash*/
static char ota_write_data[BUFFSIZE + 1] = { 0 };
/*an packet receive buffer*/
static char text[BUFFSIZE + 1] = { 0 };
/* an image total length*/
static int binary_file_length = 0;
/*socket id*/
static int socket_id = -1;
static char url_string[128] = {0};
static char url_stored[128] = {0};
static char filename[128] = {0};

#define WEB_PORT "443"

EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

const esp_partition_t *update_partition = NULL;

/*read buffer by byte still delim ,return read bytes counts*/
static int read_until(char *buffer, char delim, int len)
{
//  /*TODO: delim check,buffer check,further: do an buffer length limited*/
	int i = 0;
	while (buffer[i] != delim && i < len) {
		++i;
	}
	return i + 1;
}

/* resolve a packet from http socket
 * return true if packet including \r\n\r\n that means http packet header finished,start to receive packet body
 * otherwise return false
 * */
static bool read_past_http_header(char text[], int total_len, esp_ota_handle_t update_handle)
{
	/* i means current position */
	int i = 0, i_read_len = 0;
	while (text[i] != 0 && i < total_len) {
		i_read_len = read_until(&text[i], '\n', total_len);
		// if we resolve \r\n line,we think packet header is finished
		if (i_read_len == 2) {
			int i_write_len = total_len - (i + 2);
			memset(ota_write_data, 0, BUFFSIZE);
			/*copy first http packet body to write buffer*/
			memcpy(ota_write_data, &(text[i + 2]), i_write_len);

			esp_err_t err = esp_ota_write( update_handle, (const void *)ota_write_data, i_write_len);
			if (err != ESP_OK) {
				ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%x", err);
				return false;
			} else {
				ESP_LOGI(TAG, "esp_ota_write header OK");
				binary_file_length += i_write_len;
			}
			return true;
		}
		i += i_read_len;
	}
	return false;
}

static void __attribute__((noreturn)) task_fatal_error()
{
	ESP_LOGE(TAG, "Exiting task due to fatal error...");
	close(socket_id);
	(void)vTaskDelete(NULL);

	while (1) {
		;
	}
}

void ota_example_task(void *pvParameter)
{
	char buf[512];
	int ret;
	int flags; 
	esp_err_t err;

	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_ssl_context ssl;
	mbedtls_x509_crt cacert;
	mbedtls_ssl_config conf;
	mbedtls_net_context server_fd;


	/* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
	esp_ota_handle_t update_handle = 0 ;
	//const esp_partition_t *update_partition = NULL;

	ESP_LOGI(TAG, "Starting OTA example...");

	const esp_partition_t *configured = esp_ota_get_boot_partition();
	const esp_partition_t *running = esp_ota_get_running_partition();

	assert(configured == running); /* fresh from reset, should be running from configured boot partition */
	ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
			 configured->type, configured->subtype, configured->address);

	ESP_LOGI(TAG, "Connect to Wifi ! Start to Connect to Server....");




	mbedtls_ssl_init(&ssl);
	mbedtls_x509_crt_init(&cacert);
	mbedtls_ctr_drbg_init(&ctr_drbg);
	ESP_LOGI(TAG, "Seeding the random number generator");

	mbedtls_ssl_config_init(&conf);

	mbedtls_entropy_init(&entropy);
	if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
									NULL, 0)) != 0)
	{
		ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned %d", ret);
		abort();
	}

	ESP_LOGI(TAG, "Loading the CA root certificate...");

	ret = mbedtls_x509_crt_parse(&cacert, ssl_cert_buf, strlen((char*)ssl_cert_buf) + 1);

	if(ret < 0)
	{
		ESP_LOGE(TAG, "mbedtls_x509_crt_parse returned -0x%x\n\n", -ret);
		abort();
	}

	ESP_LOGI(TAG, "Setting hostname for TLS session...");

	 /* Hostname set here should match CN in server certificate */
	if((ret = mbedtls_ssl_set_hostname(&ssl, url_string)) != 0)
	{
		ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned -0x%x", -ret);
		abort();
	}

	ESP_LOGI(TAG, "Setting up the SSL/TLS structure...");

	if((ret = mbedtls_ssl_config_defaults(&conf,
										  MBEDTLS_SSL_IS_CLIENT,
										  MBEDTLS_SSL_TRANSPORT_STREAM,
										  MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
	{
		ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", ret);
		goto exit;
	}

	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
	mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
#ifdef CONFIG_MBEDTLS_DEBUG
	mbedtls_esp_enable_debug_log(&conf, 4);
#endif

	if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
	{
		ESP_LOGE(TAG, "mbedtls_ssl_setup returned -0x%x\n\n", -ret);
		goto exit;
	}

	ESP_LOGI(TAG, "Connected to AP");

	mbedtls_net_init(&server_fd);

	ESP_LOGI(TAG, "Connecting to %s:%s...", url_string, WEB_PORT);

	if ((ret = mbedtls_net_connect(&server_fd, url_string,
								  WEB_PORT, MBEDTLS_NET_PROTO_TCP)) != 0)
	{
		ESP_LOGE(TAG, "mbedtls_net_connect returned -%x", -ret);
		goto exit;
	}

	ESP_LOGI(TAG, "Connected.");

	mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

	ESP_LOGI(TAG, "Performing the SSL/TLS handshake...");

	while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
	{
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
		{
			ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", -ret);
			goto exit;
		}
	}

	ESP_LOGI(TAG, "Verifying peer X.509 certificate...");

	if ((flags = mbedtls_ssl_get_verify_result(&ssl)) != 0)
	{
		/* In real life, we probably want to close connection if ret != 0 */
		ESP_LOGW(TAG, "Failed to verify peer certificate!");
		bzero(buf, sizeof(buf));
		mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", flags);
		ESP_LOGW(TAG, "verification info: %s", buf);
	}
	else {
		ESP_LOGI(TAG, "Certificate verified.");
	}

	ESP_LOGI(TAG, "Writing HTTP request...");
	
	while((ret = mbedtls_ssl_write(&ssl, (const unsigned char *)http_request, strlen(http_request))) <= 0) {
		if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			ESP_LOGE(TAG, "mbedtls_ssl_write returned -0x%x", -ret);
			goto exit;
		}
	}

	update_partition = esp_ota_get_next_update_partition(NULL);
	ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
			 update_partition->subtype, update_partition->address);
	assert(update_partition != NULL);

	err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", err);
		task_fatal_error();
	}
	ESP_LOGI(TAG, "esp_ota_begin succeeded");


	bool resp_body_start = false, flag = true;
	/*deal with all receive packet*/
	while (flag) {
		memset(text, 0, TEXT_BUFFSIZE);
		memset(ota_write_data, 0, BUFFSIZE);
		bzero(text, sizeof(text));
		ret = mbedtls_ssl_read(&ssl, (unsigned char *)text, TEXT_BUFFSIZE);

		if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
			continue;

		if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
			ret = 0;
			break;
		}
		
		int buff_len = ret;
		if(ret < 0) {
			ESP_LOGE(TAG, "mbedtls_ssl_read returned -0x%x", -ret);
			break;
		} else if (buff_len > 0 && !resp_body_start) { /*deal with response header*/
			memcpy(ota_write_data, text, buff_len);
			resp_body_start = read_past_http_header(text, buff_len, update_handle);
		} else if (buff_len > 0 && resp_body_start) { /*deal with response body*/
			memcpy(ota_write_data, text, buff_len);

			err = esp_ota_write( update_handle, (const void *)ota_write_data, buff_len);
			if (err != ESP_OK) {
				ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%x", err);
				task_fatal_error();
			}

			binary_file_length += buff_len;
			ESP_LOGI(TAG, "Have written image length %d", binary_file_length);
		} else if (ret == 0) {
			ESP_LOGI(TAG, "connection closed");
			flag = false;
		}
	}
		mbedtls_ssl_close_notify(&ssl);


	ESP_LOGI(TAG, "Total Write binary data length : %d", binary_file_length);


	if (esp_ota_end(update_handle) != ESP_OK) {
		ESP_LOGE(TAG, "esp_ota_end failed!");
		task_fatal_error();
	}
	
	err = esp_ota_set_boot_partition(update_partition);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
		task_fatal_error();
	}
	ESP_LOGI(TAG, "Prepare to restart system!");
	esp_restart();

exit:
	mbedtls_ssl_session_reset(&ssl);
	mbedtls_net_free(&server_fd);
	if(ret != 0)
	{
		mbedtls_strerror(ret, buf, 100);
		ESP_LOGE(TAG, "Last error was: -0x%x - %s", -ret, buf);
	}
	return;
}

static esp_err_t event_handler(void *ctx, system_event_t *event) {
	switch(event->event_id) {
	case SYSTEM_EVENT_STA_START:
		esp_wifi_connect();
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
		xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		/* This is a workaround as ESP32 WiFi libs don't currently
		   auto-reassociate. */
		esp_wifi_connect();
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
		//esp_restart();
		break;
	default:
		break;
	}
	return ESP_OK;
}

static void initialise_wifi(void) {
	tcpip_adapter_init();
	wifi_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

	wifi_config_t wifi_config;
	memcpy(wifi_config.sta.ssid, ap_ssid, strlen(ap_ssid) + 1);
	memcpy(wifi_config.sta.password, ap_pwd, strlen(ap_pwd) + 1);

	ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
	ESP_ERROR_CHECK( esp_wifi_start() );
}

static int get_ap_credentials(void) {
	esp_err_t err = string_read_from_flash("credentials", credentials_buf, sizeof(credentials_buf));

	if (err != ESP_OK) {
		ESP_LOGE(TAG, "failed to read credentials partition = %x", err);
		return -1;
	}

	char *ssid_start, *ssid_end, *pwd_start, *pwd_end;

	ssid_start = credentials_buf;
	ssid_end = strstr(ssid_start, "\n");
	pwd_start = ssid_end + 1;
	pwd_end = strstr(pwd_start, "\n");

	strncpy(ap_ssid, ssid_start, ssid_end - ssid_start);
	strncpy(ap_pwd, pwd_start, pwd_end - pwd_start);
	return 0;
}

static int get_ssl_cert(void) {
	esp_err_t err = string_read_from_flash("ssl_cert", (char*)ssl_cert_buf, sizeof(ssl_cert_buf));

	if (err != ESP_OK) {
		ESP_LOGE(TAG, "failed to read ssl_cert partition = %x", err);
		return -1;
	}

	char *cert_footer = strstr((char*)ssl_cert_buf_start, "-----END CERTIFICATE-----\n");

	if (cert_footer == NULL) {
		ESP_LOGE(TAG, "error parsing cert");
		return -2;
	}
	cert_footer[strlen("-----END CERTIFICATE-----\n")] = 0;

	return 0;
}

void app_main()
{
	esp_err_t err;
	const esp_partition_t *ota_data = esp_partition_find_first(0x40, 0x00, "ota_data");
	update_partition = esp_ota_get_next_update_partition(NULL);

	char *file;

	err = esp_partition_read(ota_data, 0, url_stored, sizeof(url_stored));
	
	if (ESP_OK != err) {
		ESP_LOGE(TAG, "failed to read ota_data partition, trying to reboot from app partition...");
		err = esp_ota_set_boot_partition(update_partition);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
			task_fatal_error();
		}
		esp_restart();
	}

	/* 
	 * 'ota_data' partition hasn't been initialized.
	 * It means we're making a first run after flash
	 * and all we need there is to run the application.
	 */
	if (url_stored[0] == 0xff) {
		ESP_LOGI(TAG, "ota_data partition is empty, rebooting into app partition");
		err = esp_ota_set_boot_partition(update_partition);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
			task_fatal_error();
		}
			esp_restart();
	}

	file = strstr(url_stored, "/");
	strncpy(filename, file, strlen(file));
	strncpy(url_string, url_stored, (int)(file - url_stored));

	sprintf(http_request, 
			"GET %s HTTP/1.1\r\n"
			"Accept: */*\r\n"
			"Accept-Encoding: gzip, deflate\r\n"
			"Host: %s\r\n"
			"Connection: keep-alive, close\r\n"
			"\r\n",
			filename,
			url_string);

	// Initialize NVS.
	err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
		// OTA app partition table has a smaller NVS partition size than the non-OTA
		// partition table. This size mismatch may cause NVS initialization to fail.
		// If this happens, we erase NVS partition and initialize NVS again.
		const esp_partition_t* nvs_partition = esp_partition_find_first(
				ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
		assert(nvs_partition && "partition table must have an NVS partition");
		ESP_ERROR_CHECK( esp_partition_erase_range(nvs_partition, 0, nvs_partition->size) );
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK( err );

	if (get_ap_credentials() != 0)
		abort();

	initialise_wifi();
	xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
					   false, true, portMAX_DELAY);
	if (get_ssl_cert() != 0)
		ESP_LOGE(TAG, "Error parsing SSL certificate");

	xTaskCreate(&ota_example_task, "ota_example_task", 16 * 1024, NULL, 5, NULL);
}
