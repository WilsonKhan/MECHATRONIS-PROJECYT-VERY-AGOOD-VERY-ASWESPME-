#include "uart.h"
#include "stm32f303xc.h"

/*
 * USART1 (PC):   PA9  TX / PA10 RX  — AF7, APB2 clock (72 MHz)
 * USART2 (Pod1): PA2  TX / PA3  RX  — AF7, APB1 clock (36 MHz)
 * USART3 (Pod2): PB10 TX / PB11 RX  — AF7, APB1 clock (36 MHz)
 */

static USART_TypeDef * const uart_periph[] = { USART1, USART2, USART3 };

static void usart1_init(uint32_t baud)
{
    // 1. Change GPIOAEN to GPIOCEN
    RCC->AHBENR  |= RCC_AHBENR_GPIOCEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /* PC4, PC5 -> AF7 */
    // 2. Change all GPIOA to GPIOC, and shift by (4*2) and (5*2) instead of 9 and 10
    GPIOC->MODER  &= ~((3U << (4*2)) | (3U << (5*2)));
    GPIOC->MODER  |=  ((2U << (4*2)) | (2U << (5*2)));

    // 3. Change AFR[1] to AFR[0] (Low register) and shift by (4*4) and (5*4)
    GPIOC->AFR[0] &= ~((0xFU << (4*4)) | (0xFU << (5*4)));
    GPIOC->AFR[0] |=  ((7U   << (4*4)) | (7U   << (5*4)));

    // 4. Match the speed register to pin 4
    GPIOC->OSPEEDR |= (3U << (4*2));

    // 5. Change 72MHz to 8MHz for correct timing math
    USART1->BRR = 8000000U / baud;

    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    USART1->CR2 = 0;
    USART1->CR3 = 0;
}

static void usart2_init(uint32_t baud)
{
    RCC->AHBENR  |= RCC_AHBENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* PA2, PA3 -> AF7 */
    GPIOA->MODER  &= ~((3U << (2*2)) | (3U << (3*2)));
    GPIOA->MODER  |=  ((2U << (2*2)) | (2U << (3*2)));
    GPIOA->AFR[0] &= ~((0xFU << (2*4)) | (0xFU << (3*4)));
    GPIOA->AFR[0] |=  ((7U   << (2*4)) | (7U   << (3*4)));
    GPIOA->OSPEEDR |= (3U << (2*2));

    USART2->BRR = 36000000U / baud;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    USART2->CR2 = 0;
    USART2->CR3 = 0;
}

static void usart3_init(uint32_t baud)
{
    RCC->AHBENR  |= RCC_AHBENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;

    /* PB10, PB11 -> AF7 */
    GPIOB->MODER  &= ~((3U << (10*2)) | (3U << (11*2)));
    GPIOB->MODER  |=  ((2U << (10*2)) | (2U << (11*2)));
    GPIOB->AFR[1] &= ~((0xFU << ((10-8)*4)) | (0xFU << ((11-8)*4)));
    GPIOB->AFR[1] |=  ((7U   << ((10-8)*4)) | (7U   << ((11-8)*4)));
    GPIOB->OSPEEDR |= (3U << (10*2));

    USART3->BRR = 36000000U / baud;
    USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    USART3->CR2 = 0;
    USART3->CR3 = 0;
}

void uart_init_all(void)
{
    usart1_init(115200);
    usart2_init(9600);
    usart3_init(9600);
}

void uart_send(uart_channel_t ch, const uint8_t *buf, uint16_t len)
{
    USART_TypeDef *u = uart_periph[ch];
    while (len--) {
        while (!(u->ISR & USART_ISR_TXE)) {}
        u->TDR = *buf++;
    }
}

int uart_available(uart_channel_t ch)
{
    return (uart_periph[ch]->ISR & USART_ISR_RXNE) ? 1 : 0;
}

uint8_t uart_read_byte(uart_channel_t ch)
{
    while (!(uart_periph[ch]->ISR & USART_ISR_RXNE)) {}
    return (uint8_t)(uart_periph[ch]->RDR & 0xFF);
}
