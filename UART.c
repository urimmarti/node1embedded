#include "uart.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>

static volatile uint8_t g_camera_x = 0;
static volatile uint8_t g_camera_y = 0;
static volatile bool    g_new_camera_data = false;

void UART_Init(unsigned int ubrr) {
	// Set PD1 (TXD0) as output, PD0 (RXD0) as input
	DDRD |= (1 << DDD1);
	DDRD &= ~(1 << DDD0);

	// Set Baud Rate
	UBRR0H = (unsigned char)(ubrr >> 8);
	UBRR0L = (unsigned char)ubrr;

	// Enable RX, TX, and RX Complete Interrupt
	UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);

	// 8 data bits, 1 stop bit (8N1) - Note: URSEL0 required for ATmega162/16/32
	#ifdef URSEL0
	UCSR0C = (1 << URSEL0) | (1 << UCSZ01) | (1 << UCSZ00);
	#else
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
	#endif
}

void UART_Transmit(unsigned char data) {
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = data;
}

// Non-blocking stdin handler to prevent freezing when ISR is active
unsigned char UART_Receive(void) {
	return 0; // Return empty to prevent blocking menu calls
}

int uart_putchar(char c, FILE *stream) {
	if (c == '\n') {
		UART_Transmit('\r');
	}
	UART_Transmit(c);
	return 0;
}

int uart_getchar(FILE *stream) {
	return 0; // Non-blocking stub
}

bool camera_get_target(uint8_t *target_x, uint8_t *target_y) {
	if (g_new_camera_data) {
		*target_x = g_camera_x;
		*target_y = g_camera_y;
		g_new_camera_data = false;
		return true;
	}
	return false;
}

// RX Complete Interrupt Service Routine
#if defined(USART0_RX_vect)
ISR(USART0_RX_vect)
#elif defined(USART0_RXC_vect)
ISR(USART0_RXC_vect)
#else
ISR(USART_RX_vect)
#endif
{
	uint8_t byte = UDR0;

	typedef enum {
		STATE_WAIT_HEADER,
		STATE_READ_X,
		STATE_READ_Y,
		STATE_READ_CRC
	} PacketState;

	static PacketState state = STATE_WAIT_HEADER;
	static uint8_t rx_x = 0;
	static uint8_t rx_y = 0;

	switch (state) {
		case STATE_WAIT_HEADER:
		if (byte == 0xFF) {
			state = STATE_READ_X;
		}
		break;

		case STATE_READ_X:
		rx_x = byte;
		state = STATE_READ_Y;
		break;

		case STATE_READ_Y:
		rx_y = byte;
		state = STATE_READ_CRC;
		break;

		case STATE_READ_CRC:
		if (byte == ((rx_x ^ rx_y ^ 0xFF) & 0xFF)) {
			g_camera_x = rx_x;
			g_camera_y = rx_y;
			g_new_camera_data = true;
		}
		state = STATE_WAIT_HEADER;
		break;
	}
}