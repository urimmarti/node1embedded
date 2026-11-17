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




#include <stdlib.h>
void SRAM_test(void)
{
	volatile char *ext_ram = (char *) 0x1800; // Start address for the SRAM
	uint16_t ext_ram_size = 0x800;
	uint16_t write_errors = 0;
	uint16_t retrieval_errors = 0;
	printf("Starting SRAM test...\n");
	// rand() stores some internal state, so calling this function in a loop will
	// yield different seeds each time (unless srand() is called before this function)
	uint16_t seed = rand();
	// Write phase: Immediately check that the correct value was stored
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
	// Retrieval phase: Check that no values were changed during or after the write phase
	srand(seed);
	// reset the PRNG to the state it had before the write phase
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
	spi_init();
	oled_init();
	joystick_init();
	sei();

	stdout_redirect(STDOUT_UART);
	printf("=== NODO 1: INICIALIZANDO CAN ===\n");

	if (can_init_NORMAL() != 0) {
		printf("Error critico: No se pudo inicializar el CAN en Nodo 1.\n");
		while(1);
	}
	printf("--> ¡ÉXITO! Nodo 1 listo y en bus CAN.\n");
	
			stdout_redirect(STDOUT_UART);
			SRAM_test();
			stdout_redirect(STDOUT_OLED);

	can_message_t send_msg_test = {
		.id = 1,
		.length = 3,
		.data = {100, 200, 50}
	};
	can_message_t recv_msg;
	uint8_t counter_n1 = 0;

	
	
	adc_raw_data_t joystick_data = adc_read_all();
	uint16_t cam_x = 0, cam_y = 0;
	can_message_t send_joystick_msg = {
		.id = 0,
		.length = 3,
		.data = {joystick_data.joy_x,joystick_data.joy_y, joystick_data.solenoid}
	};
	uint8_t dummy_byte;
	can_message_t send_camera_msg = {
		.id = 0,
		.length = 4,
		.data = {dummy_byte, dummy_byte}
	};
	
	uint8_t last_recorded_goals = 0;
	uint8_t current_match_goals = 0;
	bool camera_mode = true;
	
	stdout_redirect(STDOUT_OLED);
	menu_update_display();
	_delay_ms(1050);
	while (1) {
		
		// use different can message according to the mode (joystick or camera)
		stdout_redirect(STDOUT_UART);
		if (camera_mode == false){
			joystick_data = adc_read_all();
			// 1. Enviar mensaje del Nodo 1 al Nodo 2
			send_joystick_msg.data[0] = joystick_data.joy_x;
			send_joystick_msg.data[1] = joystick_data.joy_y;
			send_joystick_msg.data[2] = joystick_data.solenoid;
			can_send(&send_joystick_msg);
			printf("-> Nodo 1 envió mensaje CAN joystick\n");
			printf("[JOYSTICK] X: %d Y: %d\n", joystick_data.joy_x, joystick_data.joy_y);
		}
		else{
			send_camera_msg.data[0] = (cam_x >> 8) & 0xFF; // X high byte
			send_camera_msg.data[1] = cam_x & 0xFF;        // X low byte
			send_camera_msg.data[2] = (cam_y >> 8) & 0xFF; // Y high byte
			send_camera_msg.data[3] = cam_y & 0xFF;        // Y low byte
			can_send(&send_camera_msg);
			printf("-> Nodo 1 envió mensaje CAN camera\n");
			if (!camera_get_target(&cam_x, &cam_y)) {
				printf("No camera detected\n");
				cam_x = -1;
				cam_y = -1;
			}
			printf("-> CAM RX [X: %3d, Y: %3d] %d %d\n", cam_x, cam_y);
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
		handle_nav_and_game(nav_val, goal_changed_flag, current_match_goals, camera_mode);

		// Pequeña pausa del bucle principal
		_delay_ms(20);
		

	}
	
	return 0;
}


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