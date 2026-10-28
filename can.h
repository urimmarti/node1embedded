/*
 * can.h
 *
 * Created: 07.10.2026 12:00:18
 *  Author: quetzall
 */ 


#ifndef CAN_H_
#define CAN_H_

#include <stdint.h>

// Estructura de un mensaje CAN estándar (CAN 2.0A - ID 11 bits)
typedef struct {
    uint16_t id;        // ID del mensaje (11 bits)
    uint8_t length;     // Longitud de datos (DLC: 0 a 8 bytes)
    uint8_t data[8];    // Payload de datos
} can_message_t;

/**
 * @brief Inicializa el módulo CAN y configura el MCP2515 en modo Loopback.
 * @return uint8_t 0 si fue correcto.
 */
uint8_t can_init_NORMAL(void);

/**
 * @brief Envía un mensaje CAN usando el buffer TX0.
 * @param msg Puntero al mensaje CAN a enviar.
 */
void can_send(const can_message_t *msg);

/**
 * @brief Lee un mensaje CAN recibido en RX0.
 * @param msg Puntero a la estructura donde se guardará el mensaje.
 * @return uint8_t 1 si se recibió un mensaje, 0 si no hay mensajes.
 */
uint8_t can_receive(can_message_t *msg);

#endif /* CAN_H_ */