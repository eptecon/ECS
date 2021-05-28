#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "app_aws_iot.h"
#include "app_ecs_ctrl.h"
#include "app_ecs_startup.h"
#include "app_ble_task.h"
#include "led_blinker.h"

#include "fw_version.h"

extern void aws_iot_test_task(void *param);

void app_main() {

	ESP_LOGI("FW", "running fw version %s", FW_VERSION);

	xTaskCreatePinnedToCore(&led_blinker_task, "led_task",  1 * 1024, NULL, 6, NULL, 1);

	app_ecs_startup();
	
	xTaskCreatePinnedToCore(aws_iot_test_task, 
			"iot_test_task", 
			2048, 
			"uptime",
			1,
			NULL, 0);

	xTaskCreatePinnedToCore(&aws_iot_task,	"aws_iot_task", 6 * 1024, NULL, 2, NULL, 0);
	xTaskCreatePinnedToCore(&ecs_ctrl_task,	"ecs_ctrl", 	6 * 1024, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(&ble_task, 	"ble_task", 	2 * 1024, NULL, 5, NULL, 1);

}
