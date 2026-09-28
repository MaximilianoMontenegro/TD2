/**
 ******************************************************************************
 * @file    ecg_dsp.c
 * @brief   Tarea DSP: decimacion, filtrado y deteccion de QRS por bloques.
 *
 * @details Diseno de los filtros (fs = 250 Hz). Los coeficientes se
 *          calcularon con Python/SciPy y se verificaron por simulacion:
 *          - Pasa-altos Butterworth de 2do orden, fc = 0,5 Hz, en forma
 *            directa II transpuesta (DF2T): la recomendada en punto flotante,
 *            usa 2 estados por seccion en vez de 4.
 *          - Pasa-bajos FIR de fase lineal, 83 coeficientes, ventana de
 *            Hamming, fc = 43 Hz: pasa <= 38 Hz (rizado 0,02 dB) y atenua
 *            >= 51 dB desde 48 Hz (50 Hz: -59 dB). Retardo constante de
 *            41 muestras (164 ms): no deforma el QRS ni el segmento ST.
 *          - Alternativa anterior (ECG_LP_FIR = 0): notch de 50 Hz (Q = 8) +
 *            Butterworth de 4to orden en 40 Hz, ambos IIR en DF1.
 *          - Pasa-banda 5-15 Hz (f0 = 8,66 Hz, Q = 0,866) solo para detectar:
 *            es la banda donde el QRS domina sobre la onda T y el ruido.
 *
 * @author  Grupo TD II - UTN FRBA
 ******************************************************************************
 */
#include "ecg_dsp.h"
#include "ad8232.h"
#include "ecg_qrs.h"
#include "ecg_display.h"
#include "ecg_logger.h"
#include "arm_math.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Configuracion
 * ------------------------------------------------------------------------- */
#define ECG_LP_FIR          1     /**< 1 = FIR de fase lineal, 0 = notch + IIR.  */
#define FIR_NUM_TAPS       83u    /**< Coeficientes del FIR pasa-bajos.          */
#define ADC_SAT_MIN        13u    /**< Muestras en el riel para declarar
                                       saturacion (> medio bloque = 50 ms; una
                                       R alta recortada dura menos).           */
#define UART_PLOT_TIMEOUT  50u    /**< ms: un UART trabado no frena la cadena.   */

/* ---------------------------------------------------------------------------
 * Variables publicas de diagnostico
 * ------------------------------------------------------------------------- */
volatile uint32_t ecg_abs_base      = 0u;
volatile uint8_t  ecg_lead_status   = 0u;
volatile uint8_t  ecg_adc_sat       = 0u;
volatile uint32_t dsp_overrun_count = 0u;
volatile uint32_t dsp_cycles_last   = 0u;
volatile uint32_t dsp_cycles_max    = 0u;

/* ---------------------------------------------------------------------------
 * Filtros (coeficientes en el formato de CMSIS: {b0, b1, b2, -a1, -a2})
 * ------------------------------------------------------------------------- */

/** Pasa-altos Butterworth 2do orden, 0,5 Hz (DF2T). Polos muy cerca de z = 1:
    los coeficientes van con la precision completa de float32. */
static arm_biquad_cascade_df2T_instance_f32 hp_filter;
static float32_t       hp_state[2];
static const float32_t hp_coeffs[5] = { 0.99115360f, -1.98230719f, 0.99115360f,
                                        1.98222893f, -0.98238545f };

/** Notch 50 Hz, Q = 8 (DF1). Un Q mas alto deja 145 ms de "ringing" despues
    de cada QRS; con Q = 8 baja a ~50 ms y cubre 47-53 Hz. Solo si ECG_LP_FIR = 0. */
static arm_biquad_casd_df1_inst_f32 notch_filter;
static float32_t notch_state[4];
static float32_t notch_coeffs[5] = { 0.9438954f, -0.5833578f, 0.9438954f,
                                     0.5833578f, -0.8877909f };

/** Pasa-bajos Butterworth 4to orden, 40 Hz = 2 biquads (DF1). Solo si ECG_LP_FIR = 0. */
static arm_biquad_casd_df1_inst_f32 lp_filter;
static float32_t lp_state[8];
static float32_t lp_coeffs[10] = {
    0.130380f, 0.260760f, 0.130380f, 0.602063f, -0.123563f,   /* Q = 0,5412 */
    0.175407f, 0.350814f, 0.175407f, 0.809946f, -0.511573f    /* Q = 1,3066 */
};

#if ECG_LP_FIR
/** Pasa-bajos FIR de fase lineal (ventana de Hamming, 83 coeficientes). Es
    simetrico: el orden invertido que pide CMSIS es el mismo. */
static arm_fir_instance_f32 lp_fir;
static float32_t lp_fir_state[FIR_NUM_TAPS + ECG_BLOCK_LEN - 1u];
static const float32_t lp_fir_coeffs[FIR_NUM_TAPS] = {
     1.994867343e-04f, -4.434846633e-04f, -6.733264890e-04f, -1.731523225e-04f,  6.587077514e-04f,
     9.371166234e-04f,  1.457422040e-04f, -1.105944044e-03f, -1.412907150e-03f, -4.630894182e-05f,
     1.864917343e-03f,  2.097453922e-03f, -2.160717413e-04f, -3.019208787e-03f, -2.970878500e-03f,
     7.544526597e-04f,  4.659675062e-03f,  3.997383174e-03f, -1.710327808e-03f, -6.894744001e-03f,
    -5.127301440e-03f,  3.269502660e-03f,  9.874539450e-03f,  6.300350185e-03f, -5.700041540e-03f,
    -1.384568214e-02f, -7.449834608e-03f,  9.445580654e-03f,  1.928426325e-02f,  8.507483639e-03f,
    -1.537901629e-02f, -2.726811543e-02f, -9.408515878e-03f,  2.563545294e-02f,  4.080199450e-02f,
     1.009653416e-02f, -4.745249078e-02f, -7.209175825e-02f, -1.052783057e-02f,  1.315804273e-01f,
     2.806755304e-01f,  3.442607224e-01f,  2.806755304e-01f,  1.315804273e-01f, -1.052783057e-02f,
    -7.209175825e-02f, -4.745249078e-02f,  1.009653416e-02f,  4.080199450e-02f,  2.563545294e-02f,
    -9.408515878e-03f, -2.726811543e-02f, -1.537901629e-02f,  8.507483639e-03f,  1.928426325e-02f,
     9.445580654e-03f, -7.449834608e-03f, -1.384568214e-02f, -5.700041540e-03f,  6.300350185e-03f,
     9.874539450e-03f,  3.269502660e-03f, -5.127301440e-03f, -6.894744001e-03f, -1.710327808e-03f,
     3.997383174e-03f,  4.659675062e-03f,  7.544526597e-04f, -2.970878500e-03f, -3.019208787e-03f,
    -2.160717413e-04f,  2.097453922e-03f,  1.864917343e-03f, -4.630894182e-05f, -1.412907150e-03f,
    -1.105944044e-03f,  1.457422040e-04f,  9.371166234e-04f,  6.587077514e-04f, -1.731523225e-04f,
    -6.733264890e-04f, -4.434846633e-04f,  1.994867343e-04f
};
#endif

/** Pasa-banda 5-15 Hz (DF2T), solo para el camino de deteccion. */
static arm_biquad_cascade_df2T_instance_f32 qrs_bp_filter;
static float32_t       qrs_bp_state[2];
static const float32_t qrs_bp_coeffs[5] = { 0.110874f, 0.0f, -0.110874f,
                                            1.736345f, -0.778251f };

/* ---------------------------------------------------------------------------
 * Estado privado
 * ------------------------------------------------------------------------- */
static float32_t dsp_a[ECG_BLOCK_LEN];     /**< Bloque decimado / pasa-altos.  */
static float32_t dsp_b[ECG_BLOCK_LEN];     /**< ECG filtrado (de pantalla).    */
static float32_t qrs_bp[ECG_BLOCK_LEN];    /**< Senal de deteccion 5-15 Hz.    */
static uint8_t   hp_prime = 1u;            /**< Pre-cargar el HP en el proximo bloque. */
static TaskHandle_t        s_task = NULL;
static UART_HandleTypeDef *s_huart_plot = NULL;

/* ---------------------------------------------------------------------------
 * Funciones privadas
 * ------------------------------------------------------------------------- */

/**
 * @brief  Borra el estado de todos los filtros y del detector.
 * @note   Se llama con el electrodo suelto: sin esto, al reconectar, el
 *         transitorio guardado en el pasa-altos tardaba segundos en irse.
 */
static void ECG_DSP_ResetFilters(void)
{
    memset(notch_state,  0, sizeof(notch_state));
    memset(hp_state,     0, sizeof(hp_state));
    memset(lp_state,     0, sizeof(lp_state));
#if ECG_LP_FIR
    memset(lp_fir_state, 0, sizeof(lp_fir_state));
#endif
    memset(qrs_bp_state, 0, sizeof(qrs_bp_state));
    hp_prime = 1u;
    ECG_QRS_Reset();
}

/**
 * @brief  Envia un bloque por UART como texto, una muestra por linea.
 * @param  p  Muestras a enviar.
 * @param  n  Cantidad de muestras.
 * @note   Conversion a ASCII a mano y una sola transmision por bloque (un
 *         printf por muestra hacia 25 llamadas bloqueantes). A 921600 baudios
 *         un bloque tarda ~1,6 ms.
 */
static void ECG_DSP_PlotUart(const int16_t *p, uint16_t n)
{
    static char txbuf[ECG_BLOCK_LEN * 8u];
    char *w = txbuf;

    if (s_huart_plot == NULL) { return; }
    for (uint16_t i = 0u; i < n; i++)
    {
        int32_t v = p[i];
        char    tmp[6];
        uint8_t k = 0u;
        if (v < 0) { *w++ = '-'; v = -v; }
        do { tmp[k++] = (char)('0' + (v % 10)); v /= 10; } while (v != 0);
        while (k != 0u) { *w++ = tmp[--k]; }
        *w++ = '\r';
        *w++ = '\n';
    }
    (void)HAL_UART_Transmit(s_huart_plot, (uint8_t *)txbuf, (uint16_t)(w - txbuf),
                            UART_PLOT_TIMEOUT);
}

/**
 * @brief  Procesa un bloque: 200 muestras crudas -> 25 muestras filtradas.
 * @param  pRaw  Mitad del buffer del DMA (ECG_ADC_HALF_LEN muestras).
 * @param  pOut  Salida: ECG_BLOCK_LEN muestras filtradas en counts.
 */
static void ECG_DSP_ProcessBlock(const uint16_t *pRaw, int16_t *pOut)
{
    if (AD8232_LeadsOff() != 0u)
    {
        /* Electrodo suelto: la salida no tiene sentido. Filtros a cero. */
        ECG_DSP_ResetFilters();
        memset(pOut, 0, ECG_BLOCK_LEN * sizeof(int16_t));
        ecg_lead_status = 1u;
    }
    else
    {
        ecg_lead_status = 0u;
        uint32_t t0 = DWT->CYCCNT;

        /* 1. Decimacion por promedio: 8 crudas -> 1 util. El promedio es un
              pasa-bajos (anti-alias) y reduce el ruido del ADC en raiz(8).
              Se resta la continua (Vs/2) ANTES de filtrar. */
        uint8_t n_rail = 0u;
        for (uint16_t i = 0u; i < ECG_BLOCK_LEN; i++)
        {
            const uint16_t *s = &pRaw[i * ECG_OSR];
            uint32_t acc = 0u;
            for (uint8_t k = 0u; k < ECG_OSR; k++) { acc += s[k]; }
            if ((acc <= (AD8232_RAIL_LO * ECG_OSR)) || (acc >= (AD8232_RAIL_HI * ECG_OSR))) { n_rail++; }
            dsp_a[i] = ((float32_t)acc * (1.0f / (float32_t)ECG_OSR)) - AD8232_VREF_COUNTS;
        }
        ecg_adc_sat = (n_rail >= ADC_SAT_MIN) ? 1u : 0u;

        /* 2. Pre-carga del pasa-altos (al arrancar y al reconectar): estado
              de regimen para la continua real de la entrada, sin el escalon
              que tardaba 1-2 s en irse. DF2T con salida 0 y entrada x0:
              d2 = b2 x0 ; d1 = b1 x0 + d2. */
        if (hp_prime != 0u)
        {
            hp_prime    = 0u;
            hp_state[1] = hp_coeffs[2] * dsp_a[0];
            hp_state[0] = (hp_coeffs[1] * dsp_a[0]) + hp_state[1];
        }

        /* 3. Pasa-altos 0,5 Hz (in-place) y pasa-bajos. */
        arm_biquad_cascade_df2T_f32(&hp_filter, dsp_a, dsp_a, ECG_BLOCK_LEN);
#if ECG_LP_FIR
        arm_fir_f32(&lp_fir, dsp_a, dsp_b, ECG_BLOCK_LEN);
#else
        arm_biquad_cascade_df1_f32(&notch_filter, dsp_a, dsp_b, ECG_BLOCK_LEN);
        arm_biquad_cascade_df1_f32(&lp_filter,    dsp_b, dsp_b, ECG_BLOCK_LEN);
#endif

        /* 4. Camino de deteccion, independiente del de pantalla. */
        arm_biquad_cascade_df2T_f32(&qrs_bp_filter, dsp_b, qrs_bp, ECG_BLOCK_LEN);
        ECG_QRS_Process(qrs_bp, dsp_b, ECG_BLOCK_LEN, ecg_abs_base, ecg_adc_sat);

        /* 5. float -> int16 con redondeo y saturacion (instruccion SSAT): un
              cast directo fuera de rango es comportamiento indefinido en C. */
        for (uint16_t i = 0u; i < ECG_BLOCK_LEN; i++)
        {
            float32_t v = dsp_b[i];
            int32_t   r = (int32_t)(v + ((v >= 0.0f) ? 0.5f : -0.5f));
            pOut[i] = (int16_t)__SSAT(r, 16);
        }

        /* Costo del bloque (incluye interrupciones: es una cota superior). */
        uint32_t dt = DWT->CYCCNT - t0;
        dsp_cycles_last = dt;
        if (dt > dsp_cycles_max) { dsp_cycles_max = dt; }
    }

    /* 6. Salidas (etapa ACTIVAR): pantalla, SD y UART. Ninguna bloquea. */
    {
        static ecg_msg_t msg;
        msg.base = ecg_abs_base;
        memcpy(msg.s, pOut, sizeof(msg.s));
        ECG_Display_Push(&msg);
    }
    ECG_Logger_Push(pOut);
    ECG_DSP_PlotUart(pOut, ECG_BLOCK_LEN);
    ecg_abs_base += ECG_BLOCK_LEN;
}

/**
 * @brief  Cuerpo de la tarea DSP (no retorna).
 * @param  argument  No usado.
 * @details Inicializa los filtros y el contador de ciclos, arranca la
 *          adquisicion y luego duerme hasta cada notificacion del DMA.
 *          Si llegan las dos mitades juntas, la tarea se atraso un bloque:
 *          se procesan ambas en orden y se cuenta el evento.
 */
static void ECG_DSP_Task(void *argument)
{
    uint32_t notified;
    int16_t  out_block[ECG_BLOCK_LEN];
    (void)argument;

    arm_biquad_cascade_df1_init_f32(&notch_filter, 1u, notch_coeffs, notch_state);
    arm_biquad_cascade_df2T_init_f32(&hp_filter,   1u, hp_coeffs,    hp_state);
    arm_biquad_cascade_df1_init_f32(&lp_filter,    2u, lp_coeffs,    lp_state);
    arm_biquad_cascade_df2T_init_f32(&qrs_bp_filter, 1u, qrs_bp_coeffs, qrs_bp_state);
#if ECG_LP_FIR
    arm_fir_init_f32(&lp_fir, FIR_NUM_TAPS, lp_fir_coeffs, lp_fir_state, ECG_BLOCK_LEN);
#endif
    ECG_QRS_Reset();

    /* Contador de ciclos del DWT para medir el costo del procesamiento. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    AD8232_Start(s_task);

    for (;;)
    {
        (void)xTaskNotifyWait(0u, 0xFFFFFFFFu, &notified, portMAX_DELAY);

        if ((notified & (AD8232_NOTIF_HALF1 | AD8232_NOTIF_HALF2))
                     == (AD8232_NOTIF_HALF1 | AD8232_NOTIF_HALF2))
        {
            dsp_overrun_count++;
        }
        if ((notified & AD8232_NOTIF_HALF1) != 0u)
        {
            ECG_DSP_ProcessBlock(AD8232_GetHalf(0u), out_block);
        }
        if ((notified & AD8232_NOTIF_HALF2) != 0u)
        {
            ECG_DSP_ProcessBlock(AD8232_GetHalf(1u), out_block);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Funciones publicas
 * ------------------------------------------------------------------------- */
void ECG_DSP_Start(UART_HandleTypeDef *huart_plot)
{
    s_huart_plot = huart_plot;
    configASSERT(xTaskCreate(ECG_DSP_Task, "DSP", ECG_DSP_TASK_STACK, NULL,
                             ECG_DSP_TASK_PRIO, &s_task) == pdPASS);
}
