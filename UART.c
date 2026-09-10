#include "uart.h"

void UART_Init(unsigned int ubrr) {
    // Configurar dirección de pines en el PUERTO D:
    // PD1 (TXD0) como SALIDA (1)
    // PD0 (RXD0) como ENTRADA (0)
    DDRD |= (1 << DDD1);   // TXD0 como salida
    DDRD &= ~(1 << DDD0);  // RXD0 como entrada

    // Configurar velocidad de baudios
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)ubrr;

    // Habilitar RX, TX e Interrupción RXCIE0
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);

    // Formato: 8 bits de datos, 1 bit de parada (8N1)
    UCSR0C = (1 << URSEL0) | (1 << UCSZ01) | (1 << UCSZ00) | (1 << USBS0);
}

void UART_Transmit(unsigned char data) {
    // Esperar a que el buffer de transmisión esté vacío
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

unsigned char UART_Receive(void) {
    // Esperar a que llegue un dato
    while (!(UCSR0A & (1 << RXC0)));
    return UDR0;
}


int uart_putchar(char c, FILE *stream) {
	if (c == '\n') {
		UART_Transmit('\r'); // Añadir retorno de carro para PuTTY
	}
	UART_Transmit(c);
	return 0; // Exitoso
}

// 2. Adaptador para lectura (get)
// Adaptador para entrada de datos (scanf / stdin)
int uart_getchar(FILE *stream) {
	// Bloquea en UART_Receive hasta recibir una tecla del PC
	char c = UART_Receive();
	
	// Eco local: Reenviar el carácter de vuelta a PuTTY para ver lo que escribes
	uart_putchar(c, stream);
	
	return (int)c;
}