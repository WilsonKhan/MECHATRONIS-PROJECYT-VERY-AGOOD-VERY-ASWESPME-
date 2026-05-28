#include "sensors.h"
#include "stm32f303xc.h"

/*
 * LDR on PA1 -> ADC1 channel 2
 * Wire as voltage divider: 3.3V -> LDR -> PA1 -> 10k resistor -> GND
 *
 * Using ADC1 in single conversion mode, software trigger.
 */

void adc_init(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_ADC12EN;

    /* PA1 as analog */
    GPIOA->MODER |= (3U << (1*2));

    // ====== ADD THIS LINE HERE ======
    // Set CKMODE[1:0] to 01 -> Select Synchronous AHB clock divided by 1
    ADC12_COMMON->CCR |= ADC12_CCR_CKMODE_0;
    // ================================

    /* ADC1 voltage regulator startup */
    ADC1->CR &= ~ADC_CR_ADVREGEN;
    ADC1->CR |= ADC_CR_ADVREGEN_0;
    /* Wait ~10us for regulator */
    for (volatile int i = 0; i < 1000; i++) {}

    /* Calibrate ADC */
    ADC1->CR &= ~ADC_CR_ADEN;
    ADC1->CR &= ~ADC_CR_ADCALDIF;
    ADC1->CR |= ADC_CR_ADCAL;
    while (ADC1->CR & ADC_CR_ADCAL) {}

    /* Enable ADC */
    ADC1->CR |= ADC_CR_ADEN;
    while (!(ADC1->ISR & ADC_ISR_ADRDY)) {}

    /* Channel 2 (PA1), 12-bit, single conversion */
    ADC1->SQR1 = (2U << ADC_SQR1_SQ1_Pos);
    ADC1->SMPR1 = (6U << ADC_SMPR1_SMP2_Pos);  /* 181.5 ADC clocks sample time */
}

uint16_t adc_read_ldr(void)
{
    ADC1->CR |= ADC_CR_ADSTART;
    while (!(ADC1->ISR & ADC_ISR_EOC)) {}
    return (uint16_t)(ADC1->DR & 0xFFF);
}
