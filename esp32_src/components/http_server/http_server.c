#include <string.h>
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"

#include "esp_wifi_types.h"
#include "esp_event_loop.h"
#include "apps/sntp/sntp.h"

#include "app_flash.h"


void http_server_task(void* arg)
{
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0)
	{
		printf("Can't create server socket\n");
		printf("error num= %d\n", server_socket);
		vTaskDelete(NULL);
		return;
	}

	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	uint32_t socklen = sizeof(client_addr);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(80);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		printf("Bind fail\n");
		close(server_socket);
		vTaskDelete(NULL);
		return;
	}
	if (listen(server_socket, 5) < 0)
	{
		printf("Listen fail\n");
		close(server_socket);
		vTaskDelete(NULL);
		return;
	}

	while (1)
	{
		int connect_socket = accept(server_socket, (struct sockaddr*)&client_addr, &socklen);
		if (connect_socket >= 0)
		{
			printf("Accept socket\n");
			char buffer[1024], c;
			int bytes_read = 0;
			memset(buffer, 0, 1024);
			while (recv(connect_socket, &c, 1, 0) > 0)
			{
				buffer[bytes_read++] = c;
				printf("%c", c);
				if (bytes_read == 1023)
					break;
				if (strstr(buffer, "\r\n\r\n"))
					break;
			}
			//while (recv(connect_socket, &c, 1, 0) > 0) printf("%c", c);
			printf("\n\n");

			if (strncmp(buffer, "GET ", 4))
			{
				printf("Not found GET\n");
				close(connect_socket);
#if 0
				xSemaphoreTake(s_app_mutex, portMAX_DELAY);
				s_wifi_config_started = false;
				xSemaphoreGive(s_app_mutex);
#endif
				vTaskDelay(200 / portTICK_RATE_MS);
				continue;
			}
#if 0
			xSemaphoreTake(s_app_mutex, portMAX_DELAY);
			s_wifi_config_started = true;
			xSemaphoreGive(s_app_mutex);
#endif

			char* ptr = strstr(buffer, "/msg?ssid=");
			if (ptr)
			{
				char* ptr2 = strstr(buffer, "&password=");
				char* ptr3 = strstr(buffer, " HTTP/1.");

				char ssid[64], password[64];
				memset(ssid, 0, 64);
				memset(password, 0, 64);

				strncpy(ssid, ptr + 10, (ptr2 - ptr) - 10);
				strncpy(password, ptr2 + 10, (ptr3 - ptr2) - 10);
				//printf("ssid: %s, password: %s\n", ssid, password);

				char* response =
					"HTTP/1.1 200 OK\r\n"
					"Content-Type: text/html\r\n\r\n"
					"<!DOCTYPE HTML>\r\n<html>\r\n"
					"<h1><center>Completed WiFi configuration</center></h1>"
					"<h1><center>Device is restarting...</center></h1>"
					"</html>\n";
				send(connect_socket, response, strlen(response), 0);
				close(connect_socket);

				if (strlen(ssid) > 0 && strlen(password) > 0) {

					wifi_config_t wifi_config = {0};

					memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));
					memcpy(wifi_config.sta.password, password, strlen(password));

					ESP_ERROR_CHECK(app_flash_save_wifi_sta_settings(&wifi_config.sta));

//					app_flash_save_wifi_info(ssid, password);
					ESP_LOGI("http server", "SSID: %s, PASS: %s", ssid, password);
				}

				vTaskDelay(1000 / portTICK_RATE_MS);

				close(server_socket);
				//esp_wifi_deinit();
				esp_restart();
			}
			else
			{
				char* response =
					"HTTP/1.1 200 OK\r\n"
					"Content-Type: text/html\r\n\r\n"
					"<!DOCTYPE HTML>\r\n<html>\r\n"
					"<body>"
					"<p>"
					"<center>"
					"<h1>WiFi Configuration</h1>"
					"<div>"
					"</div>"
					"<form action='msg'><p>SSID:  <input type='text' name='ssid' size=50 autofocus></p>"
					"<p>Password: <input type='text' name='password' size=50 autofocus></p>"
					"<p><input type='submit' value='Submit'></p>"
					"</form>"
					"</center>"
					"</body></html>\n";
				send(connect_socket, response, strlen(response), 0);
				close(connect_socket);
			}
		}
		vTaskDelay(200 / portTICK_RATE_MS);
	}
	close(server_socket);
	vTaskDelete(NULL);
}

