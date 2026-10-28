/*
 * io_board.c
 *
 * Created: 07.10.2026 10:22:38
 * Author: quetzall
 */ 

#include "io_board.h"
#include "spi.h"
#include "oled.h"
#include "stdout_control.h"
#include <stdio.h>
#include <util/delay.h>

#define CMD_READ_BUTTONS 0x04

void io_board_read_buttons(Buttons *buttons) {
    if (buttons == NULL) return;

    uint8_t tx_data[3] = {0x00, 0x00, 0x00};
    uint8_t rx_data[3] = {0x00, 0x00, 0x00};

    // 1. Activar IO Board
    spi_select_slave(SPI_SLAVE_IO);

    // 2. Enviar comando
    spi_write_byte(CMD_READ_BUTTONS);

    // 3. Delay obligatorio del controlador (min 40 us)
    _delay_us(50);

    // 4. Leer los 3 bytes en ráfaga con tu función
    for (uint8_t i = 0; i < 3; i++) {
        rx_data[i] = spi_master_transfer(tx_data[i]);
        _delay_us(2);
    }

    // 5. Desactivar IO Board
    spi_deselect_slave(SPI_SLAVE_IO);

    // 6. Asignar los bytes a las unions de la estructura
    buttons->right = rx_data[0];
    buttons->left  = rx_data[1];
    buttons->nav   = rx_data[2];
}

void io_print_bin(uint8_t byte) {
    for (int8_t i = 7; i >= 0; i--) {
        if (byte & (1 << i)) {
            printf("1");
        } else {
            printf("0");
        }
    }
}

void io_print_buttons_bin(const Buttons *buttons) {
    if (buttons == NULL) return;

    stdout_redirect(STDOUT_OLED);

    oled_clear_line(2);
    oled_pos(2, 0);
    printf("R: ");
    io_print_bin(buttons->right);

    oled_clear_line(4);
    oled_pos(4, 0);
    printf("L: ");
    io_print_bin(buttons->left);

    oled_clear_line(6);
    oled_pos(6, 0);
    printf("N: ");
    io_print_bin(buttons->nav);
}