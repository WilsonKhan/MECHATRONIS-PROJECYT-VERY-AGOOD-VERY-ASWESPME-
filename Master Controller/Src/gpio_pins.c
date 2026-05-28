#include "gpio_pins.h"
#include "stm32f303xc.h"

/* ---- SysTick millisecond counter ---- */
static volatile uint32_t tick_ms = 0;

void SysTick_Handler(void)
{
    tick_ms++;
}

uint32_t millis(void)
{
    return tick_ms;
}

/* ---- GPIO Init ---- */
void gpio_init_all(void)
{
    /* Enable clocks for GPIOA, GPIOB, GPIOE */
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOEEN;

    /* --- LEDs: PE8 - PE15 as outputs --- */
    uint32_t moder = GPIOE->MODER;
    for (int i = 8; i <= 15; i++) {
        moder &= ~(3U << (i * 2));
        moder |=  (1U << (i * 2));
    }
    GPIOE->MODER = moder;

    /* All LEDs off */
    GPIOE->ODR &= ~(0xFF << 8);

    /* --- User button: PA0, input (default), no pull --- */
    /* PA0 is already input after reset, and the Discovery has
     * an external pull-down on PA0 so it reads 0 when not pressed. */

    /* --- External buttons: PB4, PB5, PB6 as inputs with pull-up --- */
    /* MODER: 00 = input (default after reset for port B) */
    GPIOB->MODER &= ~((3U << (4*2)) | (3U << (5*2)) | (3U << (6*2)));
    /* PUPDR: 01 = pull-up */
    GPIOB->PUPDR &= ~((3U << (4*2)) | (3U << (5*2)) | (3U << (6*2)));
    GPIOB->PUPDR |=  ((1U << (4*2)) | (1U << (5*2)) | (1U << (6*2)));

    /* --- SysTick: 1ms tick --- */
    /* SystemCoreClock should be 8 MHz (HSI default, no PLL configured).
     * If you configure PLL to 72 MHz, change this accordingly. */
    SysTick_Config(8000000U / 1000U);

}

/* ---- LED helpers ---- */
void led_on(uint8_t led_num)
{
    if (led_num > 7) return;
    GPIOE->BSRR = (1U << (led_num + 8));
}

void led_off(uint8_t led_num)
{
    if (led_num > 7) return;
    GPIOE->BSRR = (1U << (led_num + 8 + 16));
}

void led_toggle(uint8_t led_num)
{
    if (led_num > 7) return;
    GPIOE->ODR ^= (1U << (led_num + 8));
}

void leds_all_off(void)
{
    GPIOE->ODR &= ~(0xFFU << 8);
}

/* ---- Button reads ---- */
int btn_user_pressed(void)
{
    /* PA0 is active HIGH on Discovery (pressed = 1) */
    return (GPIOA->IDR & (1U << 0)) ? 1 : 0;
}

int btn_correct_pressed(void)
{
    /* PB4, pull-up, active LOW (pressed = 0) */
    return (GPIOB->IDR & (1U << 4)) ? 0 : 1;
}

int btn_wrong_pressed(void)
{
    /* PB5, pull-up, active LOW */
    return (GPIOB->IDR & (1U << 5)) ? 0 : 1;
}

int btn_roulette_pressed(void)
{
    /* PB6, pull-up, active LOW */
    return (GPIOB->IDR & (1U << 6)) ? 0 : 1;
}

/* ---- Debounce ---- */
int btn_debounce(int raw, uint32_t *last_ms, uint32_t interval_ms)
{
    if (raw && (millis() - *last_ms > interval_ms)) {
        *last_ms = millis();
        return 1;
    }
    return 0;
}
