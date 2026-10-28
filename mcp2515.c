/*
 * mcp2515.c
 *
 * Created: 07.10.2026 11:57:09
 *  Author: quetzall
 */ 


#include "mcp2515.h"
#include "spi.h"
#include <stdio.h>
#include <util/delay.h>

uint8_t mcp2515_init(void) {
	// 1. Asegurar que SPI está listo
	spi_init();
	printf("spi_init hecho\n");

	// 2. Resetear el MCP2515 para entrar en Modo Configuración automáticamente
	mcp2515_reset();
	_delay_ms(1);
	
	printf("mcp reset hecho\n");

	// 3. Verificar que entramos en Modo Configuración (CANSTAT bits 7-5 = 100)
	uint8_t value = mcp2515_read(MCP_CANSTAT);
	if ((value & MODE_MASK) != 0x80) {
		printf("MCP2515 Init Failed! CANSTAT: 0x%02X\n", value);
		return 1;
	}

	return 0; // Exitoso
}

void mcp2515_reset(void) {
	spi_select_slave(SPI_SLAVE_CAN);
	spi_write_byte(MCP_RESET);
	spi_deselect_slave(SPI_SLAVE_CAN);
}

uint8_t mcp2515_read(uint8_t address) {
	spi_select_slave(SPI_SLAVE_CAN);
	spi_write_byte(MCP_READ);
	spi_write_byte(address);          // dirección: ignoramos lo recibido
	uint8_t data = spi_read_byte();   // dummy 0xFF, aquí sí llega el dato
	spi_deselect_slave(SPI_SLAVE_CAN);
	return data;
}

void mcp2515_write(uint8_t address, uint8_t data) {
	spi_select_slave(SPI_SLAVE_CAN);
	spi_write_byte(MCP_WRITE);
	spi_write_byte(address);
	spi_write_byte(data);
	spi_deselect_slave(SPI_SLAVE_CAN);
}

void mcp2515_request_to_send(uint8_t buffer_mask) {
	spi_select_slave(SPI_SLAVE_CAN);
	// RTS command base (0x80) ORed con los bits de buffer (1 = TX0, 2 = TX1, 4 = TX2)
	spi_write_byte(MCP_RTS | (buffer_mask & 0x07));
	spi_deselect_slave(SPI_SLAVE_CAN);
}

uint8_t mcp2515_read_status(void) {
	spi_select_slave(SPI_SLAVE_CAN);
	spi_write_byte(MCP_READ_STATUS);
	uint8_t status = spi_read_byte();
	spi_deselect_slave(SPI_SLAVE_CAN);
	return status;
}

void mcp2515_bit_modify(uint8_t address, uint8_t mask, uint8_t data) {
	spi_select_slave(SPI_SLAVE_CAN);
	spi_write_byte(MCP_BITMOD);
	spi_write_byte(address);
	spi_write_byte(mask);
	spi_write_byte(data);
	spi_deselect_slave(SPI_SLAVE_CAN);
}