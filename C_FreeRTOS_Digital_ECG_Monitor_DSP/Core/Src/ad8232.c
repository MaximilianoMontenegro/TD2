/**
 ******************************************************************************
 * @file    ad8232.c
 * @brief   Front-end AD8232: adquisicion por TIM3 + ADC1 + DMA y pines de
 *          control. Ver ad8232.h para el conexionado y el esquema.
 * @author  Grupo TD II - UTN FRBA
 ******************************************************************************
 */
#include "ad8232.h"

/* ---------------------------------------------------------------------------
 * Variables privadas
 * ------------------------------------------------------------------------- */
static ADC_HandleTypeDef *s_hadc = NULL;   /**< ADC que muestrea la salida.  */
static TIM_HandleTypeDef *s_htim = NULL;   /**< Timer que dispara el ADC.    */
static TaskHandle_t       s_task = NULL;   /**< Tarea a notificar (DSP).     */

/** Buffer circular del DMA: dos mitades de ECG_ADC_HALF_LEN muestras. */
static uint16_t s_adc_buf[ECG_ADC_BUF_LEN];

/* ---------------------------------------------------------------------------
 * Funciones publicas
 * ------------------------------------------------------------------------- */
void AD8232_Init(ADC_HandleTypeDef *hadc, TIM_HandleTypeDef *htim)
{
    s_hadc = hadc;
    s_htim = htim;
    AD8232_SetShutdown(0u);
}

void AD8232_Start(TaskHandle_t task)
{
    /* La tarea se registra ANTES de habilitar el DMA: ninguna interrupcion
       puede notificar a un handle invalido. */
    s_task = task;
    (void)HAL_ADC_Start_DMA(s_hadc, (uint32_t *)s_adc_buf, ECG_ADC_BUF_LEN);
    (void)HAL_TIM_Base_Start(s_htim);
}

const uint16_t *AD8232_GetHalf(uint32_t half)
{
    return (half == 0u) ? &s_adc_buf[0] : &s_adc_buf[ECG_ADC_HALF_LEN];
}

uint8_t AD8232_LeadsOff(void)
{
    /* Lectura directa del registro IDR: se llama 10 veces por segundo y no
       justifica el costo de HAL_GPIO_ReadPin. */
    uint8_t lo_p = ((LO_P_GPIO_Port->IDR & LO_P_Pin) != 0u) ? 1u : 0u;
    uint8_t lo_n = ((LO_N_GPIO_Port->IDR & LO_N_Pin) != 0u) ? 1u : 0u;
    return (uint8_t)(lo_p | lo_n);
}

void AD8232_SetShutdown(uint8_t off)
{
    /* BSRR: escritura atomica. Parte alta = poner en 0, parte baja = en 1. */
    SDN_GPIO_Port->BSRR = (off != 0u) ? ((uint32_t)SDN_Pin << 16u) : (uint32_t)SDN_Pin;
}

/* ---------------------------------------------------------------------------
 * Interrupciones del DMA (redefinen las funciones debiles de la HAL)
 * ------------------------------------------------------------------------- */

/**
 * @brief  El DMA lleno la primera mitad del buffer: se avisa a la tarea DSP.
 * @param  hadc  ADC que genero la interrupcion.
 * @note   Corre en contexto de interrupcion: solo se usan funciones FromISR.
 *         Una notificacion directa es el mecanismo mas liviano de FreeRTOS
 *         para despertar una tarea desde una ISR.
 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    BaseType_t woken = pdFALSE;
    if ((hadc == s_hadc) && (s_task != NULL))
    {
        (void)xTaskNotifyFromISR(s_task, AD8232_NOTIF_HALF1, eSetBits, &woken);
    }
    portYIELD_FROM_ISR(woken);   /* si la tarea DSP quedo lista, corre ya */
}

/**
 * @brief  El DMA lleno la segunda mitad del buffer: se avisa a la tarea DSP.
 * @param  hadc  ADC que genero la interrupcion.
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    BaseType_t woken = pdFALSE;
    if ((hadc == s_hadc) && (s_task != NULL))
    {
        (void)xTaskNotifyFromISR(s_task, AD8232_NOTIF_HALF2, eSetBits, &woken);
    }
    portYIELD_FROM_ISR(woken);
}
