/*
 * menu.c
 *
 * Created: 04.11.2026 15:12:55
 *  Author: quetzall
 */ 

#include <stdint.h>      // Para tipos como uint8_t
#include <stdio.h>       // Para printf
#include <util/delay.h>  // Para _delay_ms (si estás usando AVR; si es ATSAM3X8E nativo, se usa una función de delay propia)
#include "oled.h"        // Para oled_pos, oled_clear_line, etc.
#include "joystick.h"    // Para adc_raw_data_t, adc_read_all, joystick_process, etc.


#define NUM_ITEMS 3
#define NAV_UP    (1 << 4)
#define NAV_DOWN  (1 << 3)
#define NAV_BTN   (1 << 0)

// Opciones del menú principal
const char* menu_items[] = {
	"Jugar",
	"High Scores",
	"Opciones"
};


void menu_display(uint8_t selected) {
	//stdout_redirect(STDOUT_OLED);
	
	oled_pos(0, 0);
	printf("=== MAIN MENU ===");

	for (uint8_t i = 0; i < NUM_ITEMS; i++) {
		oled_clear_line(2 + i);
		oled_pos(2 + i, 0);
		if (i == selected) {
			printf("> %s", menu_items[i]);
			} else {
			printf("  %s", menu_items[i]);
		}
	}
}


uint8_t menu_navigate(void) {
	uint8_t current_option = 0;
	
	// Variables para control de flancos (evitar que el menú avance demasiado rápido si se mantiene presionado)
	uint8_t last_up = 0;
	uint8_t last_down = 0;

	menu_display(current_option);

	while (1) {
		// 1. Leer los datos de la IO Board a través de SPI usando el comando 0x04
		spi_select_slave(SPI_SLAVE_IO);
		
		spi_write_byte(0x04);           // Enviar comando de lectura de botones/nav
		uint8_t right_val = spi_read_byte(); // Leer byte 'right'
		uint8_t left_val  = spi_read_byte(); // Leer byte 'left'[cite: 1]
		uint8_t nav_val   = spi_read_byte(); // Leer byte 'nav'[cite: 1]
		
		spi_deselect_slave(SPI_SLAVE_IO);

		// 2. Extraer el estado actual de Up, Down y Btn del byte 'nav'
		uint8_t current_up   = (nav_val & NAV_UP) ? 1 : 0;
		uint8_t current_down = (nav_val & NAV_DOWN) ? 1 : 0;
		uint8_t current_btn  = (nav_val & NAV_BTN) ? 1 : 0;

		// 3. Navegación vertical basada en flancos (transición de no presionado a presionado)
		if (current_down && !last_down) {
			current_option = (current_option + 1) % NUM_ITEMS;
			menu_display(current_option);
			} else if (current_up && !last_up) {
			current_option = (current_option == 0) ? (NUM_ITEMS - 1) : (current_option - 1);
			menu_display(current_option);
		}
		
		// Actualizar estados anteriores
		last_down = current_down;
		last_up = current_up;

		// 4. Comprobar si se presiona el botón central del joystick/nav
		if (current_btn) {
			_delay_ms(50); // Antirrebote inicial
			
			// Releer el estado para confirmar la pulsación
			spi_select_slave(SPI_SLAVE_IO);
			spi_write_byte(0x04);
			spi_read_byte();
			spi_read_byte();
			uint8_t check_nav = spi_read_byte();
			spi_deselect_slave(SPI_SLAVE_IO);
			
			if (check_nav & NAV_BTN) {
				// Esperar a que el usuario suelte el botón para no registrar múltiples clics seguidos
				while (1) {
					spi_select_slave(SPI_SLAVE_IO);
					spi_write_byte(0x04);
					spi_read_byte();
					spi_read_byte();
					uint8_t release_nav = spi_read_byte();
					spi_deselect_slave(SPI_SLAVE_IO);
					
					if (!(release_nav & NAV_BTN)) break;
					_delay_ms(10);
				}
				return current_option; // Devuelve la opción elegida del menú
			}
		}

		_delay_ms(50); // Pequeña pausa para muestrear el SPI sin saturar el procesador
	}
}


/*// Opciones del menú principal
const char* main_menu_items[] = {
	"Jugar",
	"High Scores",
	"Opciones"
};

// Nombres para los submenús
const char* player_names[] = {
	"Uri",
	"Mikel",
	"Quetzal"
};

#define NUM_MAIN_ITEMS 3
#define NUM_PLAYERS 3

// Declaración previa de funciones
void menu_display_generic(const char* title, const char* items[], uint8_t num_items, uint8_t selected);
void handle_player_stats(uint8_t selected_player);

// Función principal de navegación del menú
uint8_t menu_navigate(void) {
	uint8_t current_option = 0;
	uint8_t last_up = 0;
	uint8_t last_down = 0;

	menu_display_generic("=== MAIN MENU ===", main_menu_items, NUM_MAIN_ITEMS, current_option);

	while (1) {
		// Leer IO Board mediante SPI
		spi_select_slave(SPI_SLAVE_IO);
		spi_write_byte(0x04); // Comando de lectura de botones/nav
		spi_read_byte();      // right
		spi_read_byte();      // left
		uint8_t nav_val = spi_read_byte(); // nav
		spi_deselect_slave(SPI_SLAVE_IO);

		uint8_t current_up   = (nav_val & NAV_UP) ? 1 : 0;
		uint8_t current_down = (nav_val & NAV_DOWN) ? 1 : 0;
		uint8_t current_btn  = (nav_val & NAV_BTN) ? 1 : 0;

		// Navegación vertical
		if (current_down && !last_down) {
			current_option = (current_option + 1) % NUM_MAIN_ITEMS;
			menu_display_generic("=== MAIN MENU ===", main_menu_items, NUM_MAIN_ITEMS, current_option);
			} else if (current_up && !last_up) {
			current_option = (current_option == 0) ? (NUM_MAIN_ITEMS - 1) : (current_option - 1);
			menu_display_generic("=== MAIN MENU ===", main_menu_items, NUM_MAIN_ITEMS, current_option);
		}
		
		last_down = current_down;
		last_up = current_up;

		// Selección con el botón central
		if (current_btn) {
			_delay_ms(50);
			
			// Confirmar pulsación
			spi_select_slave(SPI_SLAVE_IO);
			spi_write_byte(0x04);
			spi_read_byte();
			spi_read_byte();
			uint8_t check_nav = spi_read_byte();
			spi_deselect_slave(SPI_SLAVE_IO);
			
			if (check_nav & NAV_BTN) {
				// Esperar a que se suelte el botón
				while (1) {
					spi_select_slave(SPI_SLAVE_IO);
					spi_write_byte(0x04);
					spi_read_byte();
					spi_read_byte();
					uint8_t release_nav = spi_read_byte();
					spi_deselect_slave(SPI_SLAVE_IO);
					if (!(release_nav & NAV_BTN)) break;
					_delay_ms(10);
				}

				// Acciones según la opción principal elegida
				if (current_option == 0) {
					// SUBMENÚ JUGAR: Seleccionar jugador
					uint8_t selected_player = submenu_select_player();
					// Una vez elegido el jugador, pasamos a la pantalla de goles/juego
					handle_player_stats(selected_player);
					menu_display_generic("=== MAIN MENU ===", main_menu_items, NUM_MAIN_ITEMS, current_option);
				}
				else if (current_option == 1) {
					// SUBMENÚ HIGH SCORES: Muestra los tres nombres
					submenu_select_player(); // Muestra los nombres (de momento solo visualización)
					menu_display_generic("=== MAIN MENU ===", main_menu_items, NUM_MAIN_ITEMS, current_option);
				}
				else if (current_option == 2) {
					// OPCIONES: De momento no hacemos nada, volvemos a mostrar el menú
					_delay_ms(500);
				}
			}
		}
		_delay_ms(50);
	}
}

// Función auxiliar genérica para mostrar cualquier lista en la OLED
void menu_display_generic(const char* title, const char* items[], uint8_t num_items, uint8_t selected) {
	oled_pos(0, 0);
	printf("%s", title);

	for (uint8_t i = 0; i < num_items; i++) {
		oled_clear_line(2 + i);
		oled_pos(2 + i, 0);
		if (i == selected) {
			printf("> %s", items[i]);
			} else {
			printf("  %s", items[i]);
		}
	}
}

// Submenú para elegir uno de los tres jugadores
uint8_t submenu_select_player(void) {
	uint8_t selected_player = 0;
	uint8_t last_up = 0;
	uint8_t last_down = 0;

	menu_display_generic("SELECCIONAR JUGADOR", player_names, NUM_PLAYERS, selected_player);

	while (1) {
		spi_select_slave(SPI_SLAVE_IO);
		spi_write_byte(0x04);
		spi_read_byte();
		spi_read_byte();
		uint8_t nav_val = spi_read_byte();
		spi_deselect_slave(SPI_SLAVE_IO);

		uint8_t current_up   = (nav_val & NAV_UP) ? 1 : 0;
		uint8_t current_down = (nav_val & NAV_DOWN) ? 1 : 0;
		uint8_t current_btn  = (nav_val & NAV_BTN) ? 1 : 0;

		if (current_down && !last_down) {
			selected_player = (selected_player + 1) % NUM_PLAYERS;
			menu_display_generic("SELECCIONAR JUGADOR", player_names, NUM_PLAYERS, selected_player);
			} else if (current_up && !last_up) {
			selected_player = (selected_player == 0) ? (NUM_PLAYERS - 1) : (selected_player - 1);
			menu_display_generic("SELECCIONAR JUGADOR", player_names, NUM_PLAYERS, selected_player);
		}
		
		last_down = current_down;
		last_up = current_up;

		if (current_btn) {
			_delay_ms(50);
			while (1) {
				spi_select_slave(SPI_SLAVE_IO);
				spi_write_byte(0x04);
				spi_read_byte();
				spi_read_byte();
				uint8_t release_nav = spi_read_byte();
				spi_deselect_slave(SPI_SLAVE_IO);
				if (!(release_nav & NAV_BTN)) break;
				_delay_ms(10);
			}
			return selected_player; // Devuelve el índice del jugador escogido (0, 1 o 2)
		}
		_delay_ms(50);
	}
}

// Pantalla de juego/goles que se actualiza mediante mensajes CAN entrantes del Nodo 2
void handle_player_stats(uint8_t selected_player) {
	can_message_t recv_msg;
	uint8_t goals_conceded = 0;
	uint8_t back_to_menu = 0;

	oled_clear_line(0);
	oled_pos(0, 0);
	printf("Jugador: %s", player_names[selected_player]);
	
	oled_clear_line(2);
	oled_pos(2, 0);
	printf("Goles encajados: %d", goals_conceded);
	
	oled_clear_line(4);
	oled_pos(4, 0);
	printf("Pulsa boton para salir");

	while (!back_to_menu) {
		// 1. Comprobar si llega información de goles vía CAN desde el Nodo 2
		if (can_receive(&recv_msg)) {
			// Suponemos que el mensaje CAN que trae los goles tiene un ID específico (ej. ID = 2)
			// y que el primer byte (data[0]) contiene el contador de goles encajados.
			if (recv_msg.id == 2) {
				goals_conceded = recv_msg.data[0];
				
				oled_clear_line(2);
				oled_pos(2, 0);
				printf("Goles encajados: %d", goals_conceded);
			}
		}

		// 2. Permitir salir de la pantalla de goles si se pulsa el botón central (o izquierdo/derecho)
		spi_select_slave(SPI_SLAVE_IO);
		spi_write_byte(0x04);
		spi_read_byte();
		spi_read_byte();
		uint8_t nav_val = spi_read_byte();
		spi_deselect_slave(SPI_SLAVE_IO);

		if (nav_val & NAV_BTN) {
			_delay_ms(50);
			back_to_menu = 1; // Sale del bucle y regresa al menú principal
		}

		_delay_ms(50);
	}
}*/
