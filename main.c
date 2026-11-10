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
#include "menu.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <avr/interrupt.h>


void xmem_init(void) {
	// 1. Habilitar interfaz de Memoria Externa (XMEM)
	MCUCR |= (1 << SRE);
	
	// 2. Liberar los pines PC4-PC7 (JTAG) si fuera necesario, o ajustar XMM
	// Mantiene solo las direcciones necesarias activas en PORTC
	SFIOR |= (1 << XMM2);
	SFIOR &= ~((1 << XMM1) | (1 << XMM0));
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
	sei();
	spi_init();
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
	
	uint8_t last_recorded_goals = 0;
	uint8_t current_match_goals = 0;
	
	stdout_redirect(STDOUT_OLED);
	menu_update_display();

/*
	// 3. Bucle principal no bloqueante
	while (1) {
		// Leer datos de navegación desde la IO Board vía SPI
		spi_select_slave(SPI_SLAVE_IO);
		spi_write_byte(0x04);
		spi_read_byte(); // Ignorar byte 'right'
		spi_read_byte(); // Ignorar byte 'left'
		uint8_t nav_val = spi_read_byte(); // Byte 'nav'
		spi_deselect_slave(SPI_SLAVE_IO);
		stdout_redirect(STDOUT_UART);
		printf("nav val %d\r\n", nav_val);

		// Actualizar FSM y OLED con la nueva lectura
		stdout_redirect(STDOUT_OLED);
		menu_update(nav_val);

		// Muestreo periódico de 500 ms
		_delay_ms(300);
	}*/
	while (1) {
	
		
		joystick_data = adc_read_all();
		// 1. Enviar mensaje del Nodo 1 al Nodo 2
		send_joystick_msg.data[0] = joystick_data.joy_x;
		send_joystick_msg.data[1] = joystick_data.joy_y;
		send_joystick_msg.data[2] = joystick_data.slider_x;
		send_joystick_msg.data[3] = joystick_data.slider_y;
		
		stdout_redirect(STDOUT_UART);
		can_send(&send_joystick_msg);
		printf("-> Nodo 1 envió mensaje CAN\n");
		printf("X: %d\nY: %d\n", joystick_data.joy_x, joystick_data.joy_y);
		uint8_t cam_x = 0, cam_y = 0;
		if (camera_get_target(&cam_x, &cam_y)) {
			printf("-> CAM RX [X: %3d, Y: %3d]\n", cam_x, cam_y);
		}
		
		stdout_redirect(STDOUT_OLED);

		// 2. Comprobar si el Nodo 2 nos ha enviado algo
		uint8_t goal_changed_flag = 0;
		if (can_receive(&recv_msg)) {
			// Suponiendo que el ID 2 trae los goles en el primer byte (data[0])
			if (recv_msg.id == 2) {
				uint8_t incoming_goals = recv_msg.data[0];
				
				// Si estamos jugando y los goles han cambiado respecto a la última lectura:
				if (is_in_game() && (incoming_goals != last_recorded_goals)) {
					current_match_goals = incoming_goals;
					last_recorded_goals = incoming_goals;
					goal_changed_flag = 1; // Activamos el booleano para actualizar OLED
				}
			}
		}

		// C. Leer el estado del 'nav' desde la IO Board en esta iteración
		uint8_t nav_val = menu_read_nav_raw();

		// D. Si estamos saliendo de la partida (cuando is_in_game pasa a 0),
		// reseteamos el contador local de goles para la próxima vez.
		if (!is_in_game()) {
			last_recorded_goals = 0;
			current_match_goals = 0;
		}

		// E. Llamar a la función centralizada en menu.c pasando el nav y el estado del gol
		handle_nav_and_game(nav_val, goal_changed_flag, current_match_goals);

		// Pequeña pausa del bucle principal
		_delay_ms(50);
		

	}
	
	return 0;
}