/*
 * EmbeddedLab.c
 * Created: 07.09.2026
 * Author : quetzall
 */ 

// 1. PRIMERO definir la frecuencia del reloj
#define F_CPU 4915200UL

// 2. SEGUNDO incluir uart.h para que capture F_CPU y calcule MYUBRR
#include "uart.h"

// 3. TERCERO incluir el resto de librerías AVR
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
/* LO QUITAMOS POR EL BIEN DE LA TRAMA
// Vector de interrupción de recepción del USART0
ISR(USART0_RXC_vect) {
    unsigned char dato = UART_Receive(); 
    UART_Transmit(dato); // Eco del carácter
}*/


int main(void) {
    UART_Init(MYUBRR);
	
	fdevopen(uart_putchar, uart_getchar);
	int a;
	// Mensaje de arranque
	printf("--- ATmega162 UART Control ---\n");

    while (1) {
		printf("\nIntroduce numero: ");
		// scanf bloquea la ejecución en uart_getchar hasta pulsar Enter
		if (scanf(" %d", &a) == 1) {
			printf("\nValor leido correctamente: %d (Hex: 0x%X)\n", a, a);
			} else {
			printf("\nError: Entrada no valida.\n");
			getchar();
		}

    }
}