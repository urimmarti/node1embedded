#ifndef UART_H_
#define UART_H_

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef F_CPU
#define F_CPU 4915200UL
#endif

#define BAUDRATE 9600
// Garantizamos cálculo correcto en tipo unsigned long
#define MYUBRR (((F_CPU / (BAUDRATE * 16UL))) - 1)

void UART_Init(unsigned int ubrr);
void UART_Transmit(unsigned char data);
unsigned char UART_Receive(void);
int uart_putchar(char c, FILE *stream);
int uart_getchar(FILE *stream);
bool camera_get_target(uint8_t *target_x, uint8_t *target_y);

#endif