#ifndef UART_H
#define UART_H

#include <stdint.h>

typedef enum {
    UART_PC   = 0,  /* USART1: PA9 TX, PA10 RX */
    UART_POD1 = 1,  /* USART2: PA2 TX, PA3 RX  */
    UART_POD2 = 2   /* USART3: PB10 TX, PB11 RX */
} uart_channel_t;

void    uart_init_all(void);
void    uart_send(uart_channel_t ch, const uint8_t *buf, uint16_t len);
int     uart_available(uart_channel_t ch);
uint8_t uart_read_byte(uart_channel_t ch);

#endif
