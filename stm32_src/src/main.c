#include <stdbool.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"

#include "uart.h"
#include "spi.h"
#include "spi_protocol.h"
#include "rfid.h"
#include "flash.h"

#include "tinyprintf.h"

TaskHandle_t blinker_task_handle;

extern void device_peripherals_poll_task(void *arg);

static void blinker_task(void *arg) {
	(void) arg;

	nv_blob_t *nv = flash_get_nv_blob();

	for(;;) {
		vTaskDelay(200 / portTICK_RATE_MS);
		if (nv->device_state)
			STM_EVAL_LEDToggle(LED3);
	}
}

void prvSetupHardware(void) {
    /* Configure LED3 and LED4 on STM32F0-Discovery */
	STM_EVAL_LEDInit(LED3);
	STM_EVAL_LEDInit(LED4);

    /* Initialize User_Button on STM32F0-Discovery */
	STM_EVAL_PBInit(BUTTON_USER, BUTTON_MODE_GPIO);

	uart_init();
	init_printf(NULL, uart_putc);

	flash_init();
}


int main(void) {
	prvSetupHardware();
	spi_task_init();

#if 0
	xTaskCreate(spi_task,
                "spi_task",
                configMINIMAL_STACK_SIZE,
                NULL,                 /* pvParameters */
                tskIDLE_PRIORITY + 1, /* uxPriority */
                NULL                  /* pvCreatedTask */);
#endif
	xTaskCreate(blinker_task,
			"blinker_task",
			configMINIMAL_STACK_SIZE,
			NULL,
			tskIDLE_PRIORITY,
			&blinker_task_handle);

	xTaskCreate(rfid_task, 
			"rfid_task",
			configMINIMAL_STACK_SIZE,
			NULL,
			tskIDLE_PRIORITY,
			NULL);

	xTaskCreate(device_peripherals_poll_task, 
			"per_poll_task",
			configMINIMAL_STACK_SIZE,
			NULL,
			tskIDLE_PRIORITY,
			NULL);


	vTaskStartScheduler();
	return (0);
}
