#include "spi.h"
#include <avr/io.h>  // Sustituible por los registros de tu arquitectura (ej. STM32, PIC, etc.)
#include <stdio.h>


void spi_init(void) {	
	// MOSI, SCK, SS hardware y los Chip Selects de los esclavos deben ser SALIDAS.
	DDRB |= (1 << SPI_MOSI) | (1 << SPI_SCK) | (1 << SPI_SS_HARDWARE) |
	(1 << CS_IO_PIN) | (1 << DISPLAY_DC_PIN) | (1 << CS_DISPLAY_PIN) | (1 << CS_CAN_PIN);
	
	// SPI MISO debe ser entrada
	DDRB &=  ~(1 << SPI_MISO);
	
	// INICIALIZACIÓN CRÍTICA: Desactivar todos los esclavos (CS en ALTO)
	PORTB |= (1 << CS_IO_PIN) | (1 << CS_DISPLAY_PIN) | (1 << SPI_SS_HARDWARE) | (1 << CS_CAN_PIN);
	
	SPCR = (1 << SPE) | (1 << MSTR) | (1<<SPR0);
	//SPE activates spi enable
	//MSTR atmega is master
	//SPR0 necessary to decide the frequency --> important for later
	

}

void spi_select_slave(spi_slave_t slave) {
	switch (slave)
	{
		case SPI_SLAVE_IO:
			PORTB &=  ~(1 << CS_IO_PIN);
			break;
		
		case SPI_SLAVE_DISPLAY_COMMAND:
			PORTB &=  ~(1 << CS_DISPLAY_PIN);
			PORTB &=  ~(1 << DISPLAY_DC_PIN); // define D/C pin at low for commands
			break;
			
		case SPI_SLAVE_DISPLAY_DATA:
			PORTB &=  ~(1 << CS_DISPLAY_PIN);
			PORTB |= (1 << DISPLAY_DC_PIN); // define D/C pin at high for data
			break;
			
		case SPI_SLAVE_CAN:
			PORTB &= ~(1 << CS_CAN_PIN);
			break;
		
		default:
			printf("Selected an unvalid slave");
			break;
	}

}

void spi_deselect_slave(spi_slave_t slave) {
	switch (slave)
	{
		case SPI_SLAVE_IO:
			PORTB |= (1 << CS_IO_PIN);
			break;
		
		case SPI_SLAVE_DISPLAY_DATA:
			PORTB |= (1 << CS_DISPLAY_PIN);
			break;
		
		case SPI_SLAVE_DISPLAY_COMMAND:
			PORTB |= (1 << CS_DISPLAY_PIN);
			break;
			
		case SPI_SLAVE_CAN:
			PORTB |= (1 << CS_CAN_PIN);
			break;
		
		default:
			printf("Selected an unvalid slave");
			break;
	}

}

void spi_write_byte(uint8_t data){
	SPDR = data;
	while(!(SPSR & (1 << SPIF)));
	
}
uint8_t spi_read_byte(){
	SPDR = 0xFF;
	while(!(SPSR & (1 << SPIF)));
	return SPDR;
}

uint8_t spi_master_transfer(uint8_t data){
	SPDR = data;
	while(!(SPSR & (1 << SPIF)));
	return SPDR;
}

uint8_t* spi_master_transfer_buffer(uint8_t* tx_buffer, uint8_t* rx_buffer, uint8_t n){
	for (uint8_t i = 0; i < n; i++){
		rx_buffer[i] = spi_master_transfer(tx_buffer[i]);
	}
	return rx_buffer;
}



