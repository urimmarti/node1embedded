/*
 * can.c
 *
 * Created: 07.10.2026 12:00:08
 *  Author: quetzall
 */ 


#include "can.h"
#include "mcp2515.h"
#include <stdio.h>

uint8_t can_init_NORMAL(void) {
	if (mcp2515_init() != 0) {
		printf("mcp init falla\n");
		return 1;
	}

	// ==========================================
	// CONFIGURACIÓN DE LOS REGISTROS CNFx (Baudrate)
	// ==========================================
	// Estos valores configuran el oscilador y los segmentos de tiempo (Propagation, Phase1, Phase2, SJW).
	// Deben coincidir exactamente con los parámetros del Nodo 2 para que la comunicación sea válida.
	// (Ejemplo común para cristal de 16MHz a 125 kbps / 250 kbps en la práctica de NTNU):
	
	mcp2515_write(MCP_CNF1, 0x03); // SJW = 1 TQ, BRP = 3 (divide el reloj)
	mcp2515_write(MCP_CNF2, 0xB5); // BTLMODE = 1, Sample configurable, Phase1 y PropSeg
	mcp2515_write(MCP_CNF3, 0x01); // Phase2 y longitud del segmento
	
	mcp2515_bit_modify(MCP_RXB0CTRL, 0x60, 0x60);
	mcp2515_bit_modify(MCP_RXB1CTRL, 0x60, 0x60);

	mcp2515_bit_modify(MCP_CANCTRL, MODE_MASK, MODE_NORMAL);

	// Verificar si se cambió el modo correctamente
	uint8_t mode = mcp2515_read(MCP_CANSTAT);
	if ((mode & MODE_MASK) != MODE_NORMAL) {
		printf("Error al entrar en modo NORMAL! CANSTAT: 0x%02X\n", mode);
		return 1;
	}

	printf("CAN Inicializado en MODO NORMAL.\n");
	return 0;
}

void can_send(const can_message_t *msg) {
	if (msg == NULL) return;

	// 1. Escribir ID estándar (11 bits) en TXB0SIDH y TXB0SIDL
	// SIDH = ID[10:3], SIDL = ID[2:0] desplazado 5 bits a la izquierda
	mcp2515_write(MCP_TXB0SIDH, (uint8_t)(msg->id >> 3));
	mcp2515_write(MCP_TXB0SIDL, (uint8_t)((msg->id & 0x07) << 5));

	// 2. Escribir longitud de datos DLC (0-8)
	mcp2515_write(MCP_TXB0DLC, msg->length & 0x0F);

	// 3. Cargar datos en el buffer TXB0D0..D7
	for (uint8_t i = 0; i < msg->length; i++) {
		mcp2515_write(MCP_TXB0D0 + i, msg->data[i]);
	}

	// 4. Solicitar envío (RTS para TX0 -> buffer mask 1)
	mcp2515_request_to_send(0x01);
}

uint8_t can_receive(can_message_t *msg) {
	if (msg == NULL) return 0;

	// Verificar si hay un mensaje en RX0 (Bit RX0IF en CANINTF)
	uint8_t interrupt_flags = mcp2515_read(MCP_CANINTF);

	if (interrupt_flags & MCP_RX0IF) {
		// 1. Leer ID
		uint8_t sidh = mcp2515_read(MCP_RXB0SIDH);
		uint8_t sidl = mcp2515_read(MCP_RXB0SIDL);
		msg->id = (sidh << 3) | (sidl >> 5);

		// 2. Leer DLC
		msg->length = mcp2515_read(MCP_RXB0DLC) & 0x0F;
		if (msg->length > 8) msg->length = 8;

		// 3. Leer Datos
		for (uint8_t i = 0; i < msg->length; i++) {
			msg->data[i] = mcp2515_read(MCP_RXB0D0 + i);
		}

		// 4. Limpiar bandera de interrupción de RX0
		mcp2515_bit_modify(MCP_CANINTF, MCP_RX0IF, 0x00);

		return 1; // Mensaje recibido
	}

	return 0; // No hay mensaje
}