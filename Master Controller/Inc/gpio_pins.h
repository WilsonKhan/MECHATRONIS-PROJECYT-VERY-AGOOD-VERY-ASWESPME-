#ifndef GPIO_PINS_H
#define GPIO_PINS_H

#include <stdint.h>

/* Initialise all GPIO: LEDs (PE8-15), buttons, SysTick */
void gpio_init_all(void);

/* Millisecond counter (driven by SysTick) */
uint32_t millis(void);

/* Onboard LEDs on PE8-PE15 (led_num 0-7 maps to PE8-PE15) */
void led_on(uint8_t led_num);
void led_off(uint8_t led_num);
void led_toggle(uint8_t led_num);
void leds_all_off(void);

/* Buttons — accent return 1 if currently pressed */
/* User button = PA0 (onboard blue button) */
int btn_user_pressed(void);

/* External buttons on PB4 (correct), PB5 (wrong), PB6 (roulette) */
/* Wire as: pin -> button -> GND (internal pull-up enabled) */
int btn_correct_pressed(void);
int btn_wrong_pressed(void);
int btn_roulette_pressed(void);

/* Debounce helper: returns 1 on rising edge, updates *last_ms */
int btn_debounce(int raw, uint32_t *last_ms, uint32_t interval_ms);

#endif
