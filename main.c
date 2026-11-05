/*
 * EmbeddedLab.c
 * Created: 07.09.2026
 * Author : quetzall
 */ 

#define F_CPU 4915200UL

#include "uart.h"
#include "joystick.h"
#include "adc.h"
#include "stdout_control.h"
#include "oled.h"
#include "spi.h"
#include "io_board.h"
#include "can.h"
#include "mcp2515.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>

// Definición de la función de prueba de la SRAM
void SRAM_test(void)
{
    volatile char *ext_ram = (char *) 0x1800; // Dirección de inicio de la SRAM
    uint16_t ext_ram_size = 0x800;           // Tamaño: 2 KiB (0x1800 a 0x1FFF)
    uint16_t write_errors = 0;
    uint16_t retrieval_errors = 0;

    printf("\nStarting SRAM test...\n");

    uint16_t seed = rand();

    // Fase 1: Escritura y lectura inmediata
    srand(seed);
    for (uint16_t i = 0; i < ext_ram_size; i++) {
        uint8_t some_value = rand();
        ext_ram[i] = some_value;
        uint8_t retreived_value = ext_ram[i];
        if (retreived_value != some_value) {
            printf("Write phase error: ext_ram[%4d] = %02X (should be %02X)\n", i, retreived_value, some_value);
            write_errors++;
        }
    }

    // Fase 2: Comprobación de retención
    srand(seed);
    for (uint16_t i = 0; i < ext_ram_size; i++) {
        uint8_t some_value = rand();
        uint8_t retreived_value = ext_ram[i];
        if (retreived_value != some_value) {
            printf("Retrieval phase error: ext_ram[%4d] = %02X (should be %02X)\n", i, retreived_value, some_value);
            retrieval_errors++;
        }
    }

    printf("SRAM test completed with \n%4d errors in write phase and \n%4d errors in retrieval phase\n\n", write_errors, retrieval_errors);
}

void xmem_init(void) {
	// 1. Habilitar interfaz de Memoria Externa (XMEM)
	MCUCR |= (1 << SRE);
	
	// 2. Liberar los pines PC4-PC7 (JTAG) si fuera necesario, o ajustar XMM
	// Mantiene solo las direcciones necesarias activas en PORTC
	SFIOR |= (1 << XMM2);
	SFIOR &= ~((1 << XMM1) | (1 << XMM0));
}

const char* dir_to_string(joystick_dir_t dir) {
	switch (dir) {
		case LEFT:    return "LEFT";
		case RIGHT:   return "RIGHT";
		case UP:      return "UP";
		case DOWN:    return "DOWN";
		default:      return "NEUTRAL";
	}
}


// Definimos los streams globales
FILE uart_stream = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);
FILE oled_stream = FDEV_SETUP_STREAM(oled_putchar, NULL, _FDEV_SETUP_WRITE);

// Función para cambiar el destino de printf dinámicamente
void stdout_redirect(stdout_target_t target) {
	if (target == STDOUT_OLED) {
		stdout = &oled_stream;
		} else {
		stdout = &uart_stream;
	}
}



int main(void) {
	// 1. Inicialización de periféricos generales del sistema
	xmem_init();
	UART_Init(MYUBRR);
	oled_init();
	joystick_init();

	stdout_redirect(STDOUT_UART);
	printf("=== NODO 1: INICIALIZANDO CAN ===\n");

	if (can_init_NORMAL() != 0) {
		printf("Error critico: No se pudo inicializar el CAN en Nodo 1.\n");
		while(1);
	}
	printf("--> ¡ÉXITO! Nodo 1 listo y en bus CAN.\n");

	can_message_t send_msg_test = {
		.id = 1,
		.length = 3,
		.data = {100, 200, 50}
	};
	can_message_t recv_msg;
	uint8_t counter_n1 = 0;

	
	
	adc_raw_data_t joystick_data = adc_read_all();
	can_message_t send_joystick_msg = {
		.id = 0,
		.length = 4,
		.data = {joystick_data.joy_x,joystick_data.joy_y, joystick_data.slider_x, joystick_data.slider_y}
	};
	
	while (1) {
		joystick_data = adc_read_all();
		// 1. Enviar mensaje del Nodo 1 al Nodo 2
		send_joystick_msg.data[0] = joystick_data.joy_x;
		send_joystick_msg.data[1] = joystick_data.joy_y;
		send_joystick_msg.data[2] = joystick_data.slider_x;
		send_joystick_msg.data[3] = joystick_data.slider_y;
		
		can_send(&send_joystick_msg);
		printf("-> Nodo 1 envió mensaje CAN\n");
		//printf("X: %d\nY: %d\n", &joystick_data.joy_x, &joystick_data.joy_y);

		// 2. Comprobar si el Nodo 2 nos ha enviado algo
		if (can_receive(&recv_msg)) {
			printf("<- ¡Nodo 1 recibió mensaje! ID: %d, Data: {%d, %d, %d}\n",
			recv_msg.id, recv_msg.data[0], recv_msg.data[1], recv_msg.data[2]);
		}

		_delay_ms(50);
	}
	
	return 0;
}