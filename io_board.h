/*
 * io_board.h
 *
 * Created: 07.10.2026 10:22:53
 * Author: quetzall
 */ 

#ifndef IO_BOARD_H_
#define IO_BOARD_H_

#include <stdint.h>

// Estructura oficial del datasheet usando unions y bit-fields
typedef struct __attribute__((packed)) {
    union {
        uint8_t right;
        struct {
            uint8_t R1:1;
            uint8_t R2:1;
            uint8_t R3:1;
            uint8_t R4:1;
            uint8_t R5:1;
            uint8_t R6:1;
        };
    };
    union {
        uint8_t left;
        struct {
            uint8_t L1:1;
            uint8_t L2:1;
            uint8_t L3:1;
            uint8_t L4:1;
            uint8_t L5:1;
            uint8_t L6:1;
            uint8_t L7:1;
        };
    };
    union {
        uint8_t nav;
        struct {
            uint8_t NB:1; // Nav Button
            uint8_t NR:1; // Nav Right
            uint8_t ND:1; // Nav Down
            uint8_t NL:1; // Nav Left
            uint8_t NU:1; // Nav Up
        };
    };
} Buttons;

/**
 * @brief Lee el estado de los botones de la IO Board vía SPI.
 * @param buttons Puntero a la estructura Buttons.
 */
void io_board_read_buttons(Buttons *buttons);

/**
 * @brief Imprime un byte en formato binario de 8 bits.
 */
void io_print_bin(uint8_t byte);

/**
 * @brief Muestra el estado en binario de los 3 bytes en el OLED.
 */
void io_print_buttons_bin(const Buttons *buttons);

#endif /* IO_BOARD_H_ */