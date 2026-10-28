#ifndef SPI_DRIVER_H_
#define SPI_DRIVER_H_

#include <stdint.h>
#include <stdbool.h>

// -----------------------------------------------------------------------------
// DEFINICIONES DE PINES DE CHIP SELECT (CS) Y CONTROL
// -----------------------------------------------------------------------------
#define DISPLAY_DC_PIN	DDB2
#define CS_IO_PIN		DDB4 
#define CS_DISPLAY_PIN	DDB1
#define CS_CAN_PIN		DDB3 

// Definición de pines estándar del bus SPI
#define SPI_SS_HARDWARE DDB4
#define SPI_MOSI		DDB5
#define SPI_MISO		DDB6
#define SPI_SCK			DDB7

// Tipos de esclavos soportados
typedef enum {
	SPI_SLAVE_IO,
    SPI_SLAVE_DISPLAY_COMMAND,
	SPI_SLAVE_DISPLAY_DATA,
	SPI_SLAVE_CAN
} spi_slave_t;

/**
 * @brief Inicializa el periférico SPI y configura los pines CS como salidas inactivas (HIGH).
 */
void spi_init(void);

/**
 * @brief Selecciona (baja a LOW) el pin CS del esclavo indicado.
 * @param slave Esclavo a activar.
 */
void spi_select_slave(spi_slave_t slave);

/**
 * @brief Deselecciona (sube a HIGH) el pin CS del esclavo indicado.
 * @param slave Esclavo a desactivar.
 */
void spi_deselect_slave(spi_slave_t slave);

/**
 * @brief Escribe un solo byte por el bus SPI.
 * @param data Byte a transmitir.
 */
void spi_write_byte(uint8_t data);

/**
 * @brief Lee un solo byte enviando un byte dummy (0xFF).
 * @return uint8_t Byte recibido por MISO.
 */
uint8_t spi_read_byte(void);

/**
 * @brief Transmite un byte y devuelve el byte recibido (Full-Duplex Primitivo).
 * @param data Byte a enviar por MOSI.
 * @return uint8_t Byte recibido simultáneamente por MISO.
 */
uint8_t spi_master_transfer(uint8_t data);

/**
 * @brief Transmisión/recepción en bloque de n bytes.
 * @param tx_buffer Puntero al buffer de datos a transmitir.
 * @param rx_buffer Puntero al buffer donde se guardará la lectura.
 * @param n Cantidad de bytes a transferir.
 * @return uint8_t* Puntero al buffer de recepción rx_buffer.
 */
uint8_t* spi_master_transfer_buffer(uint8_t* tx_buffer, uint8_t* rx_buffer, uint8_t n);

/**
 * @brief Escribe un bloque de datos hacia un esclavo específico gestionando su CS automáticamente.
 * @param slave Esclavo objetivo.
 * @param data_out Puntero al buffer de datos a transmitir.
 * @param length Cantidad de bytes a enviar.
 */
void spi_write_packet(spi_slave_t slave, const uint8_t *data_out, uint16_t length);

/**
 * @brief Lee un bloque de datos desde un esclavo específico enviando bytes dummy.
 * @param slave Esclavo objetivo.
 * @param data_in Puntero al buffer donde se guardará la respuesta.
 * @param length Cantidad de bytes a recibir.
 */
void spi_read_packet(spi_slave_t slave, uint8_t *data_in, uint16_t length);

#endif /* SPI_DRIVER_H_ */