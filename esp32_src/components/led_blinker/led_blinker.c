#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "led_blinker.h"

static blink_mode_t blink_mode = BLINK_MODE_STARTED;

void led_blinker_set_mode(blink_mode_t mode) {
	blink_mode = mode;
}

void led_blinker_task(void *param) {

	gpio_pad_select_gpio(LED_GPIO);
	gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

	uint32_t current_state = 1;

	while(1) {

		switch (blink_mode) {
		case BLINK_MODE_STARTED:

			vTaskDelay(1000 / portTICK_PERIOD_MS);
			break;

		case BLINK_MODE_CONNECTED:

			if (current_state)
				current_state = 0;
			else
				current_state = 1;
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			break;

		case BLINK_MODE_HTTP_SERVER:

			if (current_state)
				current_state = 0;
			else
				current_state = 1;
			vTaskDelay(250 / portTICK_PERIOD_MS);
			break;
		}

		gpio_set_level(LED_GPIO, current_state);
	}
}

