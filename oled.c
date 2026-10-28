#include "oled.h"
#include "spi.h"
#include "fonts.h"
#include <stdio.h>
#include <util/delay.h>
#include <avr/pgmspace.h>

extern const uint8_t font8[][8] PROGMEM;

static void oled_write_command(uint8_t cmd) {
	spi_select_slave(SPI_SLAVE_DISPLAY_COMMAND);
	spi_write_byte(cmd);
	spi_deselect_slave(SPI_SLAVE_DISPLAY_COMMAND);
}

static void oled_write_data(uint8_t data) {
	spi_select_slave(SPI_SLAVE_DISPLAY_DATA);
	spi_write_byte(data);
	spi_deselect_slave(SPI_SLAVE_DISPLAY_DATA);
}

void oled_init(void) {
	spi_init();
	_delay_ms(50);
	oled_reset();
}

void oled_reset(void) {
	_delay_ms(10);
	
	// 1. Apagar pantalla
	oled_write_command(0xAE); // Display OFF
	
	// 2. Configuración de Reloj y Multiplex
	oled_write_command(0xD5); // Set display clock
	oled_write_command(0x80);
	oled_write_command(0xA8); // Set multiplex
	oled_write_command(0x3F);
	oled_write_command(0xD3); // Display offset
	oled_write_command(0x00);
	oled_write_command(0x40); // Start line 0
	
	// 3. Bomba de Carga (CRÍTICO: Activar antes de encender)
	oled_write_command(0x8D); // Charge Pump
	oled_write_command(0x14); // Enable 7.5V/9V
	
	// 4. Modo de Direccionamiento por Páginas
	oled_write_command(0x20); // Memory mode
	oled_write_command(0x02); // Page addressing
	
	// 5. Orientación Correcta (Texto derecho de Izquierda a Derecha)
	oled_write_command(0xA1); // Segment remap (columna 127 a SEG0)
	oled_write_command(0xC8); // COM scan direction (remap reverso)
	
	// 6. Pines Hardware COM y Contraste
	oled_write_command(0xDA); // COM pins
	oled_write_command(0x12);
	oled_write_command(0x81); // Contrast
	oled_write_command(0xCF); // Valor estable de contraste
	
	oled_write_command(0xD9); // Pre-charge
	oled_write_command(0xF1);
	oled_write_command(0xDB); // VCOMH
	oled_write_command(0x40);
	
	oled_write_command(0xA4); // Renderizar contenido RAM
	oled_write_command(0xA6); // Modo Normal (no invertido)

	// 7. Limpiar todas las líneas de la pantalla
	for (uint8_t i = 0; i < 8; i++) {
		oled_clear_line(i);
	}
	
	oled_home();
	
	// 8. Encender pantalla
	oled_write_command(0xAF); // Display ON
	_delay_ms(100);
}

void oled_home(void) {
	oled_pos(0, 0);
}

void oled_goto_line(uint8_t line) {
	if (line > 7) line = 7;
	oled_write_command(0xB0 | (line & 0x07)); // 0xB0 base page
}

void oled_goto_column(uint8_t column) {
	if (column > 127) column = 127;
	oled_write_command(0x00 | (column & 0x0F));        // Lower column
	oled_write_command(0x10 | ((column >> 4) & 0x0F)); // Higher column
}

void oled_pos(uint8_t row, uint8_t column) {
	oled_goto_line(row);
	oled_goto_column(column);
}

void oled_clear_line(uint8_t line) {
	oled_goto_line(line);
	oled_goto_column(0);
	for (uint8_t i = 0; i < 128; i++) {
		oled_write_data(0x00);
	}
}

void oled_print(const char* str) {
	while (*str) {
		uint8_t char_index = *str - 32;
		for (uint8_t i = 0; i < 8; i++) {
			uint8_t byte_f = pgm_read_byte(&(font8[char_index][i]));
			oled_write_data(byte_f);
		}
		str++;
	}
}

int oled_putchar(char c, FILE *stream) {
	if (c == '\n') return 0;
	char str[2] = {c, '\0'};
	oled_print(str);
	return 0;
}