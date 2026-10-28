/*
 * oled.h
 *
 * Driver para pantalla OLED (SSD1306 / SH1106)
 */

#ifndef OLED_H_
#define OLED_H_

#include <stdio.h>
#include <stdint.h>

/**
 * @brief Inicializa el bus SPI y aplica el procedimiento de arranque
 *        completo al controlador OLED.
 */
void oled_init(void);

/**
 * @brief Reinicia el controlador de la pantalla y restablece la configuración.
 */
void oled_reset(void);

/**
 * @brief Lleva el cursor gráfico a la posición inicial (Línea 0, Columna 0).
 */
void oled_home(void);

/**
 * @brief Cambia la línea/página activa actual.
 * @param line Número de línea (0 a 7).
 */
void oled_goto_line(uint8_t line);

/**
 * @brief Cambia la columna activa dentro de la línea actual.
 * @param column Número de columna (0 a 127).
 */
void oled_goto_column(uint8_t column);

/**
 * @brief Posiciona el cursor en una línea y columna específicas.
 * @param row Línea/página objetivo (0 a 7).
 * @param column Columna objetivo (0 a 127).
 */
void oled_pos(uint8_t row, uint8_t column);

/**
 * @brief Borra los 128 píxeles horizontales de una línea específica.
 * @param line Número de línea a borrar (0 a 7).
 */
void oled_clear_line(uint8_t line);

/**
 * @brief Imprime una cadena de texto almacenada en RAM en la posición actual del cursor.
 * @param str Puntero a la cadena de texto a renderizar.
 */
void oled_print(const char* str);

/**
 * @brief Función de salida para integrar el OLED con stdout / printf.
 */
int oled_putchar(char c, FILE *stream);

#endif /* OLED_H_ */