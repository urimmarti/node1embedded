/*
 * joystick.c
 *
 * Created: 02.10.2026 10:44:06
 *  Author: quetzall
 */ 

#include "joystick.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>

// Calibración Joystick
#define JOY_RAW_MIN   65   // ~1.0V -> -100%
#define JOY_RAW_MID  160   // Tu centro calibrado por software -> 0%
#define JOY_RAW_MAX  250   // ~4.0V -> +100%

// Calibración Slider
#define SLIDER_RAW_MIN   0
#define SLIDER_RAW_MAX 255

void joystick_init(void) {
	// Configurar PD5 (OC1A) para generar ~2.45 MHz al MAX156
	DDRD |= (1 << PD5);
	TCCR1A = (1 << COM1A0);              // Toggle en Compare Match
	TCCR1B = (1 << WGM12) | (1 << CS10); // CTC, Prescaler = 1
	OCR1A = 0;
}

// --- LECTURA ÚNICA EN RÁFAGA (AUTOSCAN) ---
adc_raw_data_t adc_read_all(void) {
	adc_raw_data_t raw;
	volatile uint8_t *adc_addr = (uint8_t *) 0x1000;

	// 1. Disparar conversión escribiendo en la dirección del ADC
	*adc_addr = 0x00;

	// 2. Tiempo de conversión (~50 us)
	_delay_us(50);

	// 3. Lecturas consecutivas (Auto-Scan avanza canal en cada lectura) 
	raw.slider_x = *adc_addr; 
	raw.slider_y = *adc_addr; 
	raw.joy_y    = *adc_addr;
	raw.joy_x    = *adc_addr;

	return raw;
}

// --- CONVERSIONES Y ESCALADOS ---

static int8_t scale_joystick(uint8_t raw) {
	// Deadzone en reposo alrededor de 160
	if (raw >= (JOY_RAW_MID - 3) && raw <= (JOY_RAW_MID + 3)) {
		return 0;
	}
	
	if (raw <= JOY_RAW_MIN) return -100;
	if (raw >= JOY_RAW_MAX) return 100;

	if (raw < JOY_RAW_MID) {
		return (int8_t)(((int16_t)(raw - JOY_RAW_MID) * 100) / (JOY_RAW_MID - JOY_RAW_MIN));
		} else {
		return (int8_t)(((int16_t)(raw - JOY_RAW_MID) * 100) / (JOY_RAW_MAX - JOY_RAW_MID));
	}
}

static uint8_t scale_slider(uint8_t raw) {
	if (raw <= SLIDER_RAW_MIN) return 0;
	if (raw >= SLIDER_RAW_MAX) return 100;

	return (uint8_t)(((uint16_t)raw * 100) / SLIDER_RAW_MAX);
}

static joystick_dir_t calculate_direction(int8_t x, int8_t y) {
	uint8_t threshold = 50;

	if (abs(x) > abs(y)) {
		if (x > threshold)  return RIGHT;
		if (x < -threshold) return LEFT;
		} else {
		if (y > threshold)  return UP;
		if (y < -threshold) return DOWN;
	}

	return NEUTRAL;
}

joystick_position_t joystick_process(adc_raw_data_t raw) {
	joystick_position_t joy;
	joy.x = scale_joystick(raw.joy_x);
	joy.y = scale_joystick(raw.joy_y);
	joy.dir = calculate_direction(joy.x, joy.y);
	return joy;
}

slider_position_t slider_process(adc_raw_data_t raw) {
	slider_position_t slider;
	slider.x = scale_slider(raw.slider_x);
	slider.y = scale_slider(raw.slider_y);
	return slider;
}