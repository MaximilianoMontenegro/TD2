/**
 ******************************************************************************
 * @file    ecg_dsp.h
 * @brief   Procesamiento digital del ECG en tiempo real (etapa PROCESAR).
 *
 * @details La tarea DSP despierta cada 100 ms (notificacion del DMA) y
 *          procesa un bloque:
 *          @verbatim
 *          200 crudas (2 kHz)
 *            -> decimacion x8 por promedio ........ 25 muestras a 250 Hz
 *            -> pasa-altos Butterworth 0,5 Hz ..... quita la deriva de linea de base
 *            -> pasa-bajos FIR 83 coef. (43 Hz) ... quita 50 Hz y ruido muscular
 *                 |-> ECG "de pantalla" -> pantalla, SD y UART
 *                 |-> pasa-banda 5-15 Hz -> detector de QRS + SQI (ecg_qrs.c)
 *          @endverbatim
 *          Todos los filtros son de CMSIS-DSP en punto flotante (el
 *          Cortex-M4F tiene FPU de simple precision).
 *
 * @author  Grupo TD II - UTN FRBA
 ******************************************************************************
 */
#ifndef ECG_DSP_H
#define ECG_DSP_H

#include "main.h"
#include "ecg_config.h"

/**
 * @brief  Crea la tarea DSP (prioridad ECG_DSP_TASK_PRIO).
 * @param  huart_plot  UART por donde se envia el ECG filtrado como texto
 *                     (una muestra por linea) para el Serial Plotter.
 *                     NULL = no enviar.
 * @note   Llamar antes de osKernelStart(). La tarea arranca la adquisicion.
 */
void ECG_DSP_Start(UART_HandleTypeDef *huart_plot);

/* ---- Variables de diagnostico (para Live Expressions) ---- */

/** Indice absoluto de la primera muestra del proximo bloque (250 por segundo). */
extern volatile uint32_t ecg_abs_base;

/** 1 = algun electrodo desconectado (comparadores LO+ / LO- del AD8232). */
extern volatile uint8_t  ecg_lead_status;

/** 1 = el ADC estuvo pegado al riel durante el ultimo bloque (saturacion). */
extern volatile uint8_t  ecg_adc_sat;

/** Veces que la tarea DSP no llego a tiempo (debe quedar en 0). */
extern volatile uint32_t dsp_overrun_count;

/** Ciclos de CPU del ultimo bloque y el maximo (presupuesto: 8,4 M = 100 ms). */
extern volatile uint32_t dsp_cycles_last;
extern volatile uint32_t dsp_cycles_max;

#endif /* ECG_DSP_H */
