#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

void motor_pwm_init(void);
void motor_set_speed(uint16_t speed);  /* 0-999 */
void motor_stop(void);

#endif
