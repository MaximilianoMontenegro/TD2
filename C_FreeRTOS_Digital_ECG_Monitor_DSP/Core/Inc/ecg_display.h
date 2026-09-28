/**
 ******************************************************************************
 * @file    ecg_display.h
 * @brief   Interfaz de usuario en la pantalla TFT ILI9341 (etapa ACTIVAR).
 *
 * @details Pantalla vertical de 240 x 320 dividida en dos:
 *          @verbatim
 *          y = 0..155    PANEL: FC, RR, QRS, R, VAR con su valor en vivo,
 *                        la referencia normal y el estado (NORMAL/ALTO/BAJO)
 *          y = 160..319  GRAFICO: un latido completo sobre grilla de papel
 *          @endverbatim
 *          Modo "latido": cada 1 s se dibuja el ultimo latido VALIDO con la
 *          onda R siempre en la misma columna (x = 96): el trazo queda quieto
 *          y centrado. Sin latidos validos durante 3 s se muestra la senal
 *          libre (ultimos 0,96 s) con la ganancia congelada.
 *
 *          Recibe los datos de la tarea DSP por un StreamBuffer de FreeRTOS.
 *
 * @author  Grupo TD II - UTN FRBA
 ******************************************************************************
 */
#ifndef ECG_DISPLAY_H
#define ECG_DISPLAY_H

#include "ecg_config.h"

/**
 * @brief  Inicializa el ILI9341 y dibuja la parte fija del panel.
 * @note   Llamar ANTES de osKernelInitialize(): usa HAL_Delay(), y una vez
 *         que se llama a cualquier funcion de FreeRTOS las interrupciones
 *         quedan enmascaradas hasta que arranca el planificador.
 */
void ECG_Display_Init(void);

/**
 * @brief  Crea el StreamBuffer por el que llegan los bloques de la tarea DSP.
 * @note   Llamar entre osKernelInitialize() y osKernelStart().
 */
void ECG_Display_CreateQueue(void);

/**
 * @brief  Envia un bloque a la pantalla sin bloquear (lo llama la tarea DSP).
 * @param  msg  Bloque con su indice absoluto.
 * @note   Envio ATOMICO: si no entra el mensaje completo se descarta entero.
 *         Un envio parcial dejaba al receptor desfasado.
 */
void ECG_Display_Push(const ecg_msg_t *msg);

/**
 * @brief  Cuerpo de la tarea de pantalla (no retorna).
 * @param  argument  No usado.
 * @note   Se llama desde StartDefaultTask() (tarea creada por CubeMX).
 */
void ECG_Display_Task(void *argument);

/** Bloques que la pantalla no alcanzo a leer (debe quedar en 0). */
extern volatile uint32_t disp_gap_count;

#endif /* ECG_DISPLAY_H */
