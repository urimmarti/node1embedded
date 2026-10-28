/*
 * stdout_control.h
 *
 * Created: 06.10.2026 16:15:39
 *  Author: quetzall
 */ 


#ifndef STDOUT_CONTROL_H_
#define STDOUT_CONTROL_H_

#include <stdio.h>

typedef enum {
	STDOUT_UART,
	STDOUT_OLED
} stdout_target_t;

// Declaración de variables globales de stream y función de selección
extern FILE uart_stream;
extern FILE oled_stream;

void stdout_redirect(stdout_target_t target);

#endif