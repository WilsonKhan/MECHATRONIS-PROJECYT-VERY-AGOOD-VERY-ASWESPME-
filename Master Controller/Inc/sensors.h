#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>

void     adc_init(void);
uint16_t adc_read_ldr(void);   /* Returns 12-bit ADC value (0-4095) */

#endif
