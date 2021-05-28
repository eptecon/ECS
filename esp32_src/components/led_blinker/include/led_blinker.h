#ifndef LED_BLINKER_H
#define LED_BLINKER_H

#define LED_GPIO 21 

typedef enum {
	BLINK_MODE_STARTED,
	BLINK_MODE_CONNECTED,
	BLINK_MODE_HTTP_SERVER
} blink_mode_t;

void led_blinker_set_mode(blink_mode_t mode);

void led_blinker_task(void *param);

#endif /* LED_BLINKER_H */
