/**
 ******************************************************************************
 * @file    sd_spi.h
 * @brief   Driver minimo de tarjeta SD/SDHC/SDXC en modo SPI.
 *
 * @details Conexionado (lector microSD del shield):
 *          | Senal | Pin MCU | Funcion              |
 *          |-------|---------|----------------------|
 *          | SCK   | PA5     | SPI1 reloj           |
 *          | MISO  | PA6     | SPI1 dato SD -> MCU  |
 *          | MOSI  | PA7     | SPI1 dato MCU -> SD  |
 *          | CS    | PB6     | seleccion (activo 0) |
 *
 *          Secuencia de inicializacion (SD Physical Layer Spec., modo SPI):
 *          @verbatim
 *          >= 74 clocks con CS alto (a <= 400 kHz)
 *          CMD0  (GO_IDLE_STATE)      -> R1 = 0x01: la tarjeta entra en modo SPI
 *          CMD8  (SEND_IF_COND 0x1AA) -> eco 0x1AA: tarjeta v2 (si no, v1)
 *          CMD55 + ACMD41 (HCS = 1)   -> repetir hasta R1 = 0x00 (lista)
 *          CMD58 (READ_OCR)           -> bit CCS: SDHC/SDXC (direccion por bloque)
 *          CMD16 (512)                -> solo SDSC: bloque de 512 bytes
 *          reloj SPI a 10,5 MHz
 *          @endverbatim
 *          Lectura: CMD17 + token 0xFE + 512 bytes + CRC.
 *          Escritura: CMD24 + token 0xFE + 512 bytes + CRC + respuesta 0x05.
 *
 * @author  Grupo TD II - UTN FRBA
 ******************************************************************************
 */
#ifndef INC_SD_SPI_H_
#define INC_SD_SPI_H_

#include "main.h"
#include <stdint.h>

/** Tipo de tarjeta detectado en la inicializacion. */
typedef enum
{
    SD_TYPE_UNKNOWN = 0,   /**< Sin inicializar o no reconocida.            */
    SD_TYPE_SDSC,          /**< Capacidad estandar (<= 2 GB): direccion en bytes.   */
    SD_TYPE_SDHC           /**< Alta capacidad (SDHC/SDXC): direccion en bloques.   */
} SD_CardType;

/** Tipo de la tarjeta insertada (lo completa SD_Init). */
extern SD_CardType g_sd_type;

/**
 * @brief  Inicializa la tarjeta en modo SPI.
 * @return 0 = OK; 1..6 = paso que fallo (1 CMD0, 2 CMD8, 3 ACMD41 v2,
 *         4 CMD58, 5 ACMD41 v1, 6 CMD16).
 */
uint8_t SD_Init(void);

/**
 * @brief  Lee un sector de 512 bytes.
 * @param  sector  Numero de sector (LBA).
 * @param  buff    Destino de 512 bytes.
 * @return 0 = OK; 1 = comando rechazado; 2 = no llego el token de datos.
 */
uint8_t SD_ReadBlock(uint32_t sector, uint8_t *buff);

/**
 * @brief  Escribe un sector de 512 bytes.
 * @param  sector  Numero de sector (LBA).
 * @param  buff    Origen de 512 bytes.
 * @return 0 = OK; 1 = comando rechazado; 2 = dato rechazado; 3 = tarjeta ocupada.
 */
uint8_t SD_WriteBlock(uint32_t sector, const uint8_t *buff);

#endif /* INC_SD_SPI_H_ */
