#include "uart.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#define CAMERA_NO_BALL 0xFFFF
static volatile uint16_t g_camera_x = 0;
static volatile uint16_t g_camera_y = 0;
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

bool camera_get_target(uint16_t *target_x, uint16_t *target_y) {
	if (g_new_camera_data) {
		// Clear flag first (or briefly disable UART RX interrupt on 8-bit MCUs
		// so a 16-bit read cannot be torn mid-update)
		g_new_camera_data = false;
		*target_x = g_camera_x;
		*target_y = g_camera_y;
		//printf("uart test: %d %d\r\n", g_camera_x,g_camera_y);
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
		STATE_READ_X_HIGH,
		STATE_READ_X_LOW,
		STATE_READ_Y_HIGH,
		STATE_READ_Y_LOW,
		STATE_READ_CRC
	} PacketState;

	static PacketState state = STATE_WAIT_HEADER;
	static uint8_t rx_x_high = 0;
	static uint8_t rx_x_low  = 0;
	static uint8_t rx_y_high = 0;
	static uint8_t rx_y_low  = 0;

	switch (state) {
		case STATE_WAIT_HEADER:
		if (byte == 0xFF) {
			state = STATE_READ_X_HIGH;
		}
		break;

		case STATE_READ_X_HIGH:
		rx_x_high = byte;
		state = STATE_READ_X_LOW;
		break;

		case STATE_READ_X_LOW:
		rx_x_low = byte;
		state = STATE_READ_Y_HIGH;
		break;

		case STATE_READ_Y_HIGH:
		rx_y_high = byte;
		state = STATE_READ_Y_LOW;
		break;

		case STATE_READ_Y_LOW:
		rx_y_low = byte;
		state = STATE_READ_CRC;
		break;

		case STATE_READ_CRC: {
			uint8_t expected_crc = (0xFF ^ rx_x_high ^ rx_x_low ^ rx_y_high ^ rx_y_low) & 0xFF;
			if (byte == expected_crc) {
				g_camera_x = ((uint16_t)rx_x_high << 8) | rx_x_low;
				g_camera_y = ((uint16_t)rx_y_high << 8) | rx_y_low;
				g_new_camera_data = true;
			}
			state = STATE_WAIT_HEADER;
			break;
		}
	}
}