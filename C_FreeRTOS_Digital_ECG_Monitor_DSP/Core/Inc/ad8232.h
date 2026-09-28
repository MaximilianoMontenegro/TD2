/**
 ******************************************************************************
 * @file    ad8232.h
 * @brief   Front-end analogico AD8232 y adquisicion de la senal (etapa ESCRUTAR).
 *
 * @details El modulo AD8232 amplifica (~1100 veces) y filtra la diferencia de
 *          potencial entre los electrodos RA y LA; el electrodo LL es la
 *          referencia activa (pierna derecha). Su salida analogica, centrada
 *          en Vs/2 = 1,65 V, entra al ADC1 del STM32.
 *
 *          Conexionado (placa Nucleo-F446RE):
 *          | Senal AD8232 | Pin MCU  | Funcion                                 |
 *          |--------------|----------|-----------------------------------------|
 *          | OUTPUT       | PC0 (A5) | ADC1 canal 10                           |
 *          | LO+          | PC10     | entrada, alto = electrodo LA suelto     |
 *          | LO-          | PC11     | entrada, alto = electrodo RA suelto     |
 *          | SDN (negado) | PC12     | salida, bajo = modulo apagado           |
 *
 *          Adquisicion sin intervencion de la CPU:
 *          @verbatim
 *          TIM3 (TRGO 2 kHz) --dispara--> ADC1 --pide--> DMA2 Stream0
 *              DMA circular de 400 muestras: interrumpe en la mitad y al final
 *              ISR -> xTaskNotifyFromISR(tarea DSP, mitad 1 o mitad 2)
 *          @endverbatim
 *          Mientras la CPU procesa una mitad, el DMA llena la otra.
 *
 * @author  Grupo TD II - UTN FRBA
 ******************************************************************************
 */
#ifndef AD8232_H
#define AD8232_H

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ecg_config.h"

/** Salida del AD8232 en reposo (REFOUT = Vs/2 = 1,65 V) en counts. */
#define AD8232_VREF_COUNTS   2048.0f

/** Promedio de 8 crudas <= a esto: salida pegada al riel de 0 V (~80 mV). */
#define AD8232_RAIL_LO        100u

/** Promedio de 8 crudas >= a esto: salida pegada al riel de 3,3 V. */
#define AD8232_RAIL_HI       3995u

/** Bit de notificacion: el DMA termino la primera mitad del buffer. */
#define AD8232_NOTIF_HALF1   0x01u

/** Bit de notificacion: el DMA termino la segunda mitad del buffer. */
#define AD8232_NOTIF_HALF2   0x02u

/**
 * @brief  Guarda los perifericos a usar y enciende el AD8232 (SDN en alto).
 * @param  hadc  ADC ya configurado por CubeMX (ADC1, canal 10, disparo TIM3).
 * @param  htim  Timer que dispara las conversiones (TIM3 a 2 kHz).
 * @note   No arranca la adquisicion: eso lo hace AD8232_Start() desde la
 *         tarea DSP, cuando ya existe la tarea a notificar.
 */
void AD8232_Init(ADC_HandleTypeDef *hadc, TIM_HandleTypeDef *htim);

/**
 * @brief  Arranca el DMA circular y el timer: comienza el muestreo.
 * @param  task  Tarea que recibe una notificacion por cada mitad del buffer.
 */
void AD8232_Start(TaskHandle_t task);

/**
 * @brief  Devuelve la mitad del buffer del DMA que ya esta completa.
 * @param  half  0 = primera mitad, 1 = segunda mitad.
 * @return Puntero a ECG_ADC_HALF_LEN muestras crudas de 12 bits.
 */
const uint16_t *AD8232_GetHalf(uint32_t half);

/**
 * @brief  Lee los comparadores de electrodo suelto del AD8232.
 * @return 1 si algun electrodo esta desconectado (LO+ o LO- en alto), 0 si no.
 */
uint8_t AD8232_LeadsOff(void);

/**
 * @brief  Apaga o enciende el AD8232 con su pin SDN (activo en bajo).
 * @param  off  1 = apagado (consumo < 1 uA), 0 = funcionando.
 */
void AD8232_SetShutdown(uint8_t off);

#endif /* AD8232_H */
