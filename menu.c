#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "oled.h"
#include "spi.h"
#include "menu.h"

static const char* main_menu[] = {
	"Jugar",
	"Best Score"
};
#define NUM_MENU_ITEMS 2

static uint8_t current_selection = 0;
static int8_t in_game_mode = 0;
static int8_t best_score_val = -1;

// Función para leer el nav y los botones por SPI (comando 0x04)[cite: 2]
uint8_t menu_read_nav_raw(void) {
	spi_select_slave(SPI_SLAVE_IO);
	spi_write_byte(0x04); // Comando de lectura de botones/nav[cite: 2]
	uint8_t right_val = spi_read_byte(); // Byte right[cite: 2]
	spi_read_byte();                      // Byte left[cite: 2]
	uint8_t nav_val = spi_read_byte();    // Byte nav[cite: 2]
	spi_deselect_slave(SPI_SLAVE_IO);
	return nav_val;
}

// Pintar el menú principal limpiando antes la pantalla para que no queden restos
void menu_update_display(void) {
	// Si tienes una función oled_clear() úsala; si no, limpiamos línea a línea de forma segura:
	for (uint8_t i = 0; i < 8; i++) {
		oled_clear_line(i);
	}

	oled_pos(0, 0);
	printf("=== MENU PRINCIPAL ===");

	for (uint8_t i = 0; i < NUM_MENU_ITEMS; i++) {
		oled_pos(2 + i, 0);
		if (i == current_selection) {
			printf("> %s", main_menu[i]);
			} else {
			printf("  %s", main_menu[i]);
		}
	}
}

// Función central para gestionar la navegación y el estado del juego por iteración
void handle_nav_and_game(uint8_t nav_val, uint8_t goal_changed, uint8_t current_goals) {
	
	// CASO 1: ESTAMOS EN EL MENÚ PRINCIPAL
	if (!in_game_mode) {
		uint8_t up = (nav_val & NAV_UP);
		uint8_t down = (nav_val & NAV_DOWN);
		uint8_t btn = (nav_val & NAV_BTN);

		// Movimiento hacia ABAJO y ARRIBA con control individual robusto
		if (down) {
			current_selection = (current_selection + 1) % NUM_MENU_ITEMS;
			menu_update_display();
			_delay_ms(150); // Pausa prudente para que el usuario alcance a soltar la palanca
		}
		else if (up) {
			current_selection = (current_selection == 0) ? (NUM_MENU_ITEMS - 1) : (current_selection - 1);
			menu_update_display();
			_delay_ms(150);
		}

		// Si se pulsa el botón central (Btn)
		if (btn) {
			_delay_ms(50); // Antirrebote
			
			// Limpiamos toda la pantalla antes de entrar a la nueva sub-pantalla
			for (uint8_t i = 0; i < 8; i++) { oled_clear_line(i); }

			if (current_selection == 0) {
				// Opción 0: Jugar
				in_game_mode = 1;
				
				oled_pos(0, 0);
				printf("=== MODO JUGADOR ===");
				oled_pos(2, 0);
				printf("Goles: 0");
				oled_pos(4, 0);
				printf("Pulsa SR6 para salir");
			}
			else if (current_selection == 1) {
				// Opción 1: Best Score
				oled_pos(0, 0);
				if (best_score_val == -1) {
					printf("Best Score: N/A");
					} else {
					printf("Best Score: %d", best_score_val);
				}
				
				// Delay de 2 segundos exactos
				_delay_ms(2000);
				
				// Al salir de Best Score, redibujamos el menú principal limpio
				menu_update_display();
			}
		}
	}
	// CASO 2: ESTAMOS DENTRO DE LA PANTALLA DE JUEGO
	else {
		// En juego comprobamos si se pulsa SR6 para salir
		spi_select_slave(SPI_SLAVE_IO);
		spi_write_byte(0x04);
		uint8_t right_val = spi_read_byte(); // Leemos el byte right[cite: 2]
		spi_read_byte();
		spi_read_byte();
		spi_deselect_slave(SPI_SLAVE_IO);

		if (right_val & BTN_R6) {
			// Guardamos el récord (menor número de goles encajados es mejor)
			if (best_score_val == -1 || current_goals < best_score_val) {
				best_score_val = current_goals;
			}
			
			in_game_mode = 0;       // Volvemos al menú principal
			current_selection = 0;  // Reseteamos selección
			
			// Limpiamos y redibujamos el menú principal limpio
			menu_update_display();
			_delay_ms(300);         // Antirrebote de salida
		}
		
		// Si hay gol nuevo, actualizamos la línea de goles
		if (goal_changed) {
			oled_clear_line(2);
			oled_pos(2, 0);
			printf("Goles: %d", current_goals);
		}
	}
}

int8_t is_in_game(void) { return in_game_mode; }
int8_t get_best_score(void) { return best_score_val; }