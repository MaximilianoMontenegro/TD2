/**
 ******************************************************************************
 * @file    ecg_config.h
 * @brief   Parametros comunes del monitor de ECG.
 *
 * @details Unico lugar donde se definen los valores que comparten varios
 *          modulos: frecuencias de muestreo, tamano de bloque, calibracion,
 *          prioridades y pilas de las tareas de FreeRTOS y el mensaje que
 *          viaja de la tarea DSP a la tarea de pantalla.
 *
 *          No incluye la HAL a proposito: asi ecg_qrs.c se puede compilar
 *          y probar en una PC con el mismo codigo que corre en la placa.
 *
 *          Cadena de tiempos:
 *          @verbatim
 *          TIM3 (2 kHz) -> ADC1 -> DMA (400 muestras, doble buffer)
 *               cada 200 crudas (100 ms) -> 1 bloque de 25 muestras a 250 Hz
 *          @endverbatim
 *
 * @author  Grupo TD II - UTN FRBA
 ******************************************************************************
 */
#ifndef ECG_CONFIG_H
#define ECG_CONFIG_H

#include <stdint.h>

/* ========================================================================== */
/*  Adquisicion y procesamiento                                               */
/* ========================================================================== */

/** Frecuencia de muestreo del ADC [Hz] (TIM3: 84 MHz / 8400 / 5). */
#define ECG_ADC_FS_HZ        2000u

/** Factor de sobremuestreo: se promedian 8 muestras crudas por muestra util. */
#define ECG_OSR                 8u

/** Frecuencia de trabajo de todo el procesamiento [Hz] = 2000 / 8. */
#define ECG_FS_HZ             250u

/** Muestras utiles por bloque de procesamiento (25 a 250 Hz = 100 ms). */
#define ECG_BLOCK_LEN          25u

/** Muestras crudas en cada mitad del buffer del DMA (= 1 bloque). */
#define ECG_ADC_HALF_LEN     (ECG_BLOCK_LEN * ECG_OSR)

/** Largo total del buffer circular del DMA (doble buffer). */
#define ECG_ADC_BUF_LEN      (ECG_ADC_HALF_LEN * 2u)

/** Convierte milisegundos a muestras a ECG_FS_HZ (division entera). */
#define ECG_MS_TO_SAMPLES(ms) ((uint32_t)(ms) * ECG_FS_HZ / 1000u)

/**
 * @brief Calibracion de amplitud [counts por mV en la piel].
 *
 * Ganancia total del modulo AD8232 ~1100 y ADC de 12 bits sobre 3,3 V:
 * 1 mV -> 1,1 V -> 1,1 * 4096 / 3,3 = ~1365 counts.
 * Calibrar si el modulo usado tiene otra ganancia.
 */
#define ECG_COUNTS_PER_MV    1365.0f

/* ========================================================================== */
/*  Tareas de FreeRTOS                                                        */
/*  (la tarea de pantalla es la defaultTask de CubeMX: osPriorityNormal = 24, */
/*   pila de 1024 palabras; se configura en el .ioc)                          */
/* ========================================================================== */

/** Prioridad de la tarea DSP (40 = osPriorityHigh): la de tiempo real. */
#define ECG_DSP_TASK_PRIO      40

/** Pila de la tarea DSP en palabras de 32 bits (512 = 2 KB). */
#define ECG_DSP_TASK_STACK    512u

/** Prioridad de la tarea de registro en SD (16 = osPriorityBelowNormal). */
#define ECG_LOG_TASK_PRIO      16

/** Pila de la tarea de registro en palabras (1024 = 4 KB: sprintf con float). */
#define ECG_LOG_TASK_STACK   1024u

/* ========================================================================== */
/*  Mensaje entre tareas                                                      */
/* ========================================================================== */

/**
 * @brief Bloque de ECG filtrado que la tarea DSP envia a la de pantalla.
 *
 * Lleva el indice absoluto de su primera muestra: la pantalla puede ubicar
 * la onda R exacta dentro de su anillo y detectar bloques perdidos.
 */
typedef struct
{
    uint32_t base;                 /**< Indice absoluto de s[0].            */
    int16_t  s[ECG_BLOCK_LEN];     /**< ECG filtrado en counts del ADC.     */
} ecg_msg_t;

#endif /* ECG_CONFIG_H */
