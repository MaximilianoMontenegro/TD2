/**
 ******************************************************************************
 * @file    ecg_logger.h
 * @brief   Registro del ECG en la tarjeta microSD en formato CSV (etapa ACTIVAR).
 *
 * @details Capas usadas para escribir en la tarjeta:
 *          @verbatim
 *          ecg_logger.c  (f_open / f_write / f_sync)
 *             -> FatFs (ff.c)            sistema de archivos FAT32
 *             -> user_diskio.c           lectura/escritura de sectores
 *             -> sd_spi.c                comandos de la SD en modo SPI
 *             -> HAL SPI1 (PA5/PA6/PA7) + CS en PB6
 *          @endverbatim
 *          Cada arranque crea un archivo nuevo ECG_0001.CSV, ECG_0002.CSV...
 *          con una fila por muestra (250 por segundo):
 *          @verbatim
 *          muestra,ecg,bpm,rr_ms,qrs_ms,r_mv,sqi
 *          @endverbatim
 *          La tarea tiene la prioridad mas baja de la aplicacion: si la SD se
 *          demora escribiendo, nunca frena la adquisicion ni el dibujo.
 *
 * @note    La tarjeta debe estar en FAT32 (exFAT deshabilitado en ffconf.h).
 *
 * @author  Grupo TD II - UTN FRBA
 ******************************************************************************
 */
#ifndef ECG_LOGGER_H
#define ECG_LOGGER_H

#include "ecg_config.h"

/**
 * @brief  Crea el StreamBuffer por el que llegan los bloques de la tarea DSP.
 * @note   Llamar entre osKernelInitialize() y osKernelStart().
 */
void ECG_Logger_CreateQueue(void);

/**
 * @brief  Crea la tarea de registro (prioridad ECG_LOG_TASK_PRIO).
 * @note   La tarea monta la tarjeta y reintenta cada 2 s si no esta.
 */
void ECG_Logger_Start(void);

/**
 * @brief  Envia un bloque al registro sin bloquear (lo llama la tarea DSP).
 * @param  block  ECG_BLOCK_LEN muestras filtradas en counts.
 * @note   Si la cola esta llena (SD lenta) el bloque se descarta entero.
 */
void ECG_Logger_Push(const int16_t *block);

/* ---- Variables de diagnostico (para Live Expressions) ---- */
extern volatile uint8_t  sd_logging_ok;    /**< 1 = tarjeta montada y escribiendo.   */
extern volatile int      sd_fresult;       /**< Ultimo codigo FRESULT (0 = FR_OK).   */
extern volatile uint32_t sd_write_errors;  /**< Escrituras fallidas (debe ser 0).    */
extern volatile uint16_t sd_file_num;      /**< Numero nnnn del ECG_nnnn.CSV actual. */

#endif /* ECG_LOGGER_H */
