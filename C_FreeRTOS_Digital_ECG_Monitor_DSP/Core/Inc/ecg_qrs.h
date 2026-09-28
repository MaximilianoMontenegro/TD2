/**
 ******************************************************************************
 * @file    ecg_qrs.h
 * @brief   Detector de complejos QRS e indice de calidad de senal (SQI).
 *
 * @details Recibe, muestra a muestra y a 250 Hz, dos senales:
 *          - pDet: ECG filtrado pasa-banda 5-15 Hz (para detectar).
 *          - pEcg: ECG filtrado de pantalla (para medir la R, el ancho del
 *            QRS y armar la plantilla de calidad).
 *          y publica la frecuencia cardiaca y los parametros del latido SOLO
 *          si la senal es confiable (ecg_sqi_ok = 1).
 *
 *          Uso desde la tarea DSP (una vez por bloque):
 *          @code
 *          ECG_QRS_Process(senal_5_15Hz, ecg_filtrado, 25, indice_abs, adc_saturado);
 *          if (ecg_sqi_ok) { mostrar(ecg_bpm); }
 *          @endcode
 *          Con electrodo suelto: ECG_QRS_Reset().
 *
 *          No depende de la HAL: se compila igual en la PC para validarlo con
 *          registros reales (ver el informe, seccion de validacion).
 *
 * @author  Grupo TD II - UTN FRBA
 ******************************************************************************
 */
#ifndef ECG_QRS_H
#define ECG_QRS_H

#include <stdint.h>
#include "ecg_config.h"

/**
 * @brief  Vuelve el detector al estado inicial (2 s de aprendizaje).
 */
void ECG_QRS_Reset(void);

/**
 * @brief  Procesa un bloque de muestras.
 * @param  pDet      Senal de deteccion (pasa-banda 5-15 Hz), n muestras.
 * @param  pEcg      ECG filtrado de pantalla en counts, n muestras.
 * @param  n         Cantidad de muestras del bloque.
 * @param  abs_base  Indice absoluto de pDet[0] / pEcg[0].
 * @param  adc_sat   1 si el ADC estuvo en el riel en este bloque.
 */
void ECG_QRS_Process(const float *pDet, const float *pEcg, uint16_t n,
                     uint32_t abs_base, uint8_t adc_sat);

/* ---- Resultados (validos solo con ecg_sqi_ok = 1) ---- */
extern volatile uint16_t ecg_bpm;          /**< Frecuencia cardiaca [lpm]; 0 = sin dato.   */
extern volatile uint16_t ecg_rr_ms;        /**< Intervalo R-R (mediana) [ms].              */
extern volatile uint16_t ecg_qrs_ms;       /**< Duracion del QRS [ms].                     */
extern volatile uint8_t  ecg_regular;      /**< 1 = ritmo regular (VAR < 20 %).            */
extern volatile float    ecg_r_mv;         /**< Amplitud de la onda R [mV].                */
extern volatile uint8_t  ecg_rr_var_pct;   /**< Variabilidad R-R: (max - min) / media [%]. */
extern volatile uint8_t  ecg_sqi_ok;       /**< 1 = senal limpia, datos confiables.        */
extern volatile uint8_t  ecg_sqi_cc;       /**< Correlacion media con la plantilla x100.   */
extern volatile uint32_t ecg_beat_count;   /**< Latidos aceptados desde el arranque.       */

/** Posicion absoluta de la R del ultimo latido VALIDO (para la pantalla). */
extern volatile uint32_t ecg_beat_r_abs;
/** Contador que cambia con cada latido valido nuevo (se escribe despues de la posicion). */
extern volatile uint32_t ecg_beat_seq;

#endif /* ECG_QRS_H */
