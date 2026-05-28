#include "motor.h"
#include "stm32f303xc.h"

/*
 * Motor PWM on PA8 using TIM1 CH1, AF6
 * Wire: PA8 -> motor driver IN -> motor
 *
 * TIM1 runs on APB2 (72 MHz with PLL, 8 MHz without).
 * We'll assume 8 MHz (HSI, no PLL) for now.
 * Prescaler 7 -> 1 MHz tick, ARR 999 -> 1 kHz PWM.
 * Adjust if you configure the PLL.
 */

void motor_pwm_init(void)
{
    RCC->AHBENR  |= RCC_AHBENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

    /* PA8 -> AF6 (TIM1_CH1) */
    GPIOA->MODER  &= ~(3U << (8*2));
    GPIOA->MODER  |=  (2U << (8*2));
    GPIOA->AFR[1] &= ~(0xFU << ((8-8)*4));
    GPIOA->AFR[1] |=  (6U   << ((8-8)*4));
    GPIOA->OSPEEDR |= (3U << (8*2));

    /* TIM1: PWM mode 1 on CH1 */
    TIM1->PSC  = 7;       /* 8 MHz / 8 = 1 MHz */
    TIM1->ARR  = 999;     /* 1 MHz / 1000 = 1 kHz PWM */
    TIM1->CCR1 = 0;       /* Start stopped */
    TIM1->CCMR1 = (6U << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;
    TIM1->CCER  = TIM_CCER_CC1E;
    TIM1->BDTR  = TIM_BDTR_MOE;   /* Main output enable (TIM1 is advanced timer) */
    TIM1->CR1   = TIM_CR1_CEN;
}

void motor_set_speed(uint16_t speed)
{
    if (speed > 999) speed = 999;
    TIM1->CCR1 = speed;
}

void motor_stop(void)
{
    TIM1->CCR1 = 0;
}
