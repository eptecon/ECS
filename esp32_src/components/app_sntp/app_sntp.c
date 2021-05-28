#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <time.h>
#include <apps/sntp/sntp.h>
#include <lwip/sockets.h>
#include <sys/time.h>
#include "sdkconfig.h"
#include <stdio.h>

//#define SNTP_SERVER_STR "129.6.15.28"
#define SNTP_SERVER_STR "216.239.35.0"

struct timeval now = {.tv_sec = 0, .tv_usec = 0};
time_t nowtime;
struct tm *nowtm;
char tmbuf[64];

/*
 * Connect to an Internet time server to get the current date and
 * time.
 */
void sntp_start() {
	ip_addr_t addr;
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	inet_pton(AF_INET, SNTP_SERVER_STR, &addr);
	sntp_setserver(0, &addr);
	sntp_init();
}

bool sntp_isenabled(void) {
	gettimeofday(&now, NULL);
	/* 
	 * An ugly workaround to get actual value from server
	 */
	if (now.tv_sec > 1493030000)
		return true;
	else
		return false;
}

bool sntp_wait_enabled(uint8_t timeout_sec) {
	uint8_t sec = timeout_sec;
	while (!sntp_isenabled() && sec--) {
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
	if (sec)	
		return true;
	else
		return false;
}

int sntp_get_date(char *buf, size_t buflen) {
	gettimeofday(&now, NULL);
	nowtime = now.tv_sec;
	nowtm = localtime(&nowtime);
	strftime(tmbuf, sizeof tmbuf, "%Y-%m-%dT%H:%M:%S", nowtm);
	return snprintf(buf, buflen, "%s.%06ldZ", tmbuf, now.tv_usec);
}
