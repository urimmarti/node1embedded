/*
 * adc.c
 *
 * Created: 02.10.2026 10:43:42
 *  Author: quetzall
 */ 

#include "adc.h"
#include <avr/io.h>
#include <util/delay.h>

void adc_init(void) {
	// Configurar reloj en PD5 (~2.45 MHz)
	DDRD |= (1 << PD5);
	TCCR1A = (1 << COM1A0);
	TCCR1B = (1 << WGM12) | (1 << CS10);
	OCR1A = 0;
}

uint8_t adc_read(uint8_t channel) {
	volatile uint8_t *adc_addr = (uint8_t *) 0x1000;
	
	// Iniciar conversión en el canal solicitado
	*adc_addr = channel;
	
	// Esperar a que se complete la conversión (~15-50 us)
	_delay_us(50);
	
	// Leer y devolver el resultado del bus de datos
	return *adc_addr;
}
