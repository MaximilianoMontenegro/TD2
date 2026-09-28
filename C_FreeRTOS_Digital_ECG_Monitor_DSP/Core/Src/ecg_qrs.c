/**
 ******************************************************************************
 * @file    ecg_qrs.c
 * @brief   Detector de QRS e indice de calidad de senal (SQI), muestra a muestra.
 *
 * @details Algoritmo (fs = 250 Hz):
 *          @verbatim
 *          pDet (5-15 Hz) -> derivada de 5 puntos -> cuadrado -> ventana movil
 *          de 150 ms -> raiz (RMS de la pendiente) -> umbral adaptativo ->
 *          reglas de decision -> latido -> R-R, R, QRS -> SQI -> publicacion
 *          @endverbatim
 *
 *          Base bibliografica:
 *          - Pan & Tompkins, IEEE TBME 32(3), 1985: derivada, cuadrado,
 *            integrador de 150 ms, periodo refractario de 200 ms y
 *            discriminacion de onda T por pendiente.
 *          - Hamilton & Tompkins, IEEE TBME 33(12), 1986: niveles de QRS y de
 *            ruido como MEDIANA de los ultimos 8 picos,
 *            umbral = ruido + 0,3125 (QRS - ruido) y busqueda hacia atras
 *            ("searchback") a 1,5 R-R.
 *          - Orphanidou et al., IEEE JBHI 19(3), 2015: la FC se publica solo
 *            si es fisiologica (40-180 lpm), R-R max / min < 2,2 y la forma
 *            de los QRS se repite (correlacion media con su promedio > 0,66).
 *
 *          Agregados propios, validados con el registro real ECG_LOG.CSV:
 *          - Rechazo de artefactos: pico > 2,5 veces la mediana de los QRS.
 *            Si aparecen 2 seguidos, la amplitud cambio de verdad y se
 *            re-aprende.
 *          - Saturacion (ADC en el riel o |ECG| > 1,8 mV): 1 s sin aceptar.
 *          - Nivel de ruido acotado a la mitad del nivel de QRS.
 *          - La FC vence si pasan 3 s sin latido valido.
 *
 * @author  Grupo TD II - UTN FRBA
 ******************************************************************************
 */
#include "ecg_qrs.h"
#include <math.h>
#include <string.h>

#if defined(__arm__)
#include "main.h"
#define QRS_BARRIER()   __DMB()          /**< Posicion escrita antes que el contador. */
#else
#define QRS_BARRIER()   ((void)0)        /**< Compilacion de prueba en PC. */
#endif

/* ---------------------------------------------------------------------------
 * Parametros
 * ------------------------------------------------------------------------- */
#define QRS_WIN_LEN      ECG_MS_TO_SAMPLES(150)   /**< Integrador: 150 ms = 37.        */
#define QRS_REFRACT      ECG_MS_TO_SAMPLES(200)   /**< Periodo refractario: 200 ms.    */
#define QRS_LEARN        ECG_MS_TO_SAMPLES(2000)  /**< Aprendizaje inicial: 2 s.       */
#define QRS_TWAVE_WIN    ECG_MS_TO_SAMPLES(360)   /**< Candidato a < 360 ms: posible T. */
#define QRS_TH           0.3125f  /**< Hamilton: ruido + 0,3125 (QRS - ruido).         */
#define QRS_SB_K         1.5f     /**< Hamilton: searchback a 1,5 R-R sin latido.      */
#define QRS_ARTIF_K      2.5f     /**< Pico > 2,5 x mediana de QRS = artefacto.        */
#define QRS_ART_RELEARN  2u       /**< 2 "artefactos" seguidos: cambio de amplitud.    */
#define QRS_LVL_N        8u       /**< Picos para las medianas de nivel (Hamilton).    */
#define QRS_SQI_N        5u       /**< Latidos en la ventana del SQI.                  */
#define QRS_SQI_MIN      4u       /**< Minimo de latidos para evaluar el SQI.          */
#define QRS_CC_MIN       0.66f    /**< Orphanidou: correlacion media minima.           */
#define QRS_HR_MIN       40.0f    /**< Orphanidou: FC minima fisiologica [lpm].        */
#define QRS_HR_MAX       180.0f   /**< Orphanidou: FC maxima fisiologica [lpm].        */
#define QRS_RR_RATIO     2.2f     /**< Orphanidou: R-R max / R-R min.                  */
#define QRS_RR_MIN       ECG_MS_TO_SAMPLES(200)   /**< R-R minimo (300 lpm).           */
#define QRS_RR_MAX       ECG_MS_TO_SAMPLES(3000)  /**< R-R maximo (20 lpm).            */
#define QRS_SNIP_PRE     ECG_MS_TO_SAMPLES(100)   /**< Plantilla: 100 ms antes de R.   */
#define QRS_SNIP_POST    ECG_MS_TO_SAMPLES(60)    /**< ... y 60 ms despues.            */
#define QRS_SNIP_LEN     (QRS_SNIP_PRE + QRS_SNIP_POST + 1u)
#define QRS_HIST_LEN     512u     /**< Historia de 2 s (potencia de 2).                */
#define QRS_SAT_COUNTS   2500.0f  /**< |ECG filtrado| > 1,8 mV: fuera de rango.        */
#define QRS_SAT_HOLD     ECG_MS_TO_SAMPLES(1000)  /**< 1 s sin aceptar tras saturar.   */
#define QRS_TIMEOUT      ECG_MS_TO_SAMPLES(3000)  /**< 3 s sin latido: FC invalida.    */
#define QRS_SB_MARGIN    60u      /**< Margen para buscar la R en la historia.         */
#define QRS_W_RUN        3u       /**< Muestras isoelectricas para cerrar el QRS.      */
#define QRS_IRREG_PCT    20.0f    /**< Dispersion R-R > 20 %: irregular.               */
#define QRS_SQ_SCALE     16.0f    /**< 4 bits fraccionarios al pasar a entero.         */
#define QRS_SQ_MAX       1.0e8f   /**< Tope por muestra: la suma de 37 no desborda.    */

#ifndef ECG_QRS_STRICT_REF
/** 1 = igual al prototipo en Python (sin vencimiento de la FC). Solo validacion. */
#define ECG_QRS_STRICT_REF   0
#endif

/** Muestra de la historia: back = 0 es la mas reciente. */
#define QRS_H(back)  hist[(hist_idx + QRS_HIST_LEN - 1u - (uint32_t)(back)) & (QRS_HIST_LEN - 1u)]

/* ---------------------------------------------------------------------------
 * Resultados publicos
 * ------------------------------------------------------------------------- */
volatile uint16_t ecg_bpm        = 0u;
volatile uint16_t ecg_rr_ms      = 0u;
volatile uint16_t ecg_qrs_ms     = 0u;
volatile uint8_t  ecg_regular    = 1u;
volatile float    ecg_r_mv       = 0.0f;
volatile uint8_t  ecg_rr_var_pct = 0u;
volatile uint8_t  ecg_sqi_ok     = 0u;
volatile uint8_t  ecg_sqi_cc     = 0u;
volatile uint32_t ecg_beat_count = 0u;
volatile uint32_t ecg_beat_r_abs = 0u;
volatile uint32_t ecg_beat_seq   = 0u;

/* ---------------------------------------------------------------------------
 * Estado privado
 * ------------------------------------------------------------------------- */
static float    x5[5];                          /**< Entrada de la derivada.            */
static uint32_t win[QRS_WIN_LEN];               /**< Ventana movil en ENTEROS: la suma  */
static uint16_t win_idx;                        /**< corrida en float deriva con horas. */
static uint64_t win_sum;
static float    i1, i2;                         /**< Integrador en n-1 y n-2.           */
static uint32_t n;                              /**< Muestras desde el reset.           */
static uint32_t last;                           /**< Muestra del ultimo QRS.            */
static uint32_t sat_until;                      /**< Fin de la ventana de saturacion.   */
static float    lpk;                            /**< Maximo durante el aprendizaje.     */

static float    qpk[QRS_LVL_N], npk[QRS_LVL_N]; /**< Ultimos picos de QRS y de ruido.   */
static uint8_t  qpk_n, qpk_i, npk_n, npk_i;
static float    q_med, nz_med;                  /**< Sus medianas.                      */

static uint16_t rr[QRS_SQI_N];                  /**< Ultimos R-R validos [muestras].    */
static uint8_t  rr_n, rr_i;
static float    rr_med;

static uint8_t  cand;                           /**< 1 = hay un candidato en curso.     */
static float    cpk, cs, qs;                    /**< Pico y pendiente del candidato y del ultimo QRS. */
static uint32_t cpos;
static float    sbpk, sbs;                      /**< Mayor pico bajo el umbral (searchback). */
static uint32_t sbpos;
static uint8_t  art_cnt, art_i;                 /**< Artefactos seguidos.               */
static float    art_pk[QRS_ART_RELEARN];

static float    hist[QRS_HIST_LEN];             /**< ECG de pantalla de los ultimos 2 s. */
static uint16_t hist_idx;
static float    snip[QRS_SQI_N][QRS_SNIP_LEN];  /**< QRS de la ventana del SQI.          */
static uint8_t  snip_n, snip_i;
static float    cc_win;                         /**< Correlacion media con la plantilla. */
static float    w_avg;                          /**< Ancho del QRS promediado.           */

/* ---------------------------------------------------------------------------
 * Funciones privadas
 * ------------------------------------------------------------------------- */

/**
 * @brief  Mediana de hasta 8 valores (ordenamiento por insercion).
 * @param  v    Valores.
 * @param  cnt  Cantidad (0..8).
 * @return La mediana; 0 si cnt = 0.
 */
static float QRS_Median(const float *v, uint8_t cnt)
{
    float s[QRS_LVL_N];
    for (uint8_t k = 0u; k < cnt; k++)
    {
        float  x = v[k];
        int8_t j = (int8_t)k - 1;
        while ((j >= 0) && (s[j] > x)) { s[j + 1] = s[j]; j--; }
        s[j + 1] = x;
    }
    if (cnt == 0u) { return 0.0f; }
    return ((cnt & 1u) != 0u) ? s[cnt / 2u] : (0.5f * (s[(cnt / 2u) - 1u] + s[cnt / 2u]));
}

/** @brief Agrega un pico de QRS y recalcula la mediana. @param v Pico. */
static void QRS_PushQ(float v)
{
    qpk[qpk_i] = v;
    qpk_i = (uint8_t)((qpk_i + 1u) % QRS_LVL_N);
    if (qpk_n < QRS_LVL_N) { qpk_n++; }
    q_med = QRS_Median(qpk, qpk_n);
}

/** @brief Agrega un pico de ruido y recalcula la mediana. @param v Pico. */
static void QRS_PushN(float v)
{
    npk[npk_i] = v;
    npk_i = (uint8_t)((npk_i + 1u) % QRS_LVL_N);
    if (npk_n < QRS_LVL_N) { npk_n++; }
    nz_med = QRS_Median(npk, npk_n);
}

/** @brief Marca la FC como no confiable (la pantalla muestra "---"). */
static void QRS_Invalidate(void)
{
    ecg_sqi_ok = 0u;
    ecg_bpm    = 0u;
}

/**
 * @brief  Ubica la onda R en la historia.
 * @param  lag  Muestras desde el pico del integrador hasta ahora.
 * @param  amp  Salida: |amplitud| de la R en counts.
 * @return Cuantas muestras atras esta la R.
 * @note   El maximo del integrador llega 15-45 muestras despues de la R.
 */
static uint16_t QRS_LocateR(uint32_t lag, float *amp)
{
    uint32_t rb = lag + 15u, end = lag + 45u;
    float    rmax = 0.0f;
    if (end > (QRS_HIST_LEN - QRS_SNIP_PRE - 2u)) { end = QRS_HIST_LEN - QRS_SNIP_PRE - 2u; }
    for (uint32_t b = lag + 15u; b <= end; b++)
    {
        float a = fabsf(QRS_H(b));
        if (a > rmax) { rmax = a; rb = b; }
    }
    *amp = rmax;
    return (uint16_t)rb;
}

/**
 * @brief  Guarda el QRS actual (-100..+60 ms, sin continua) y recalcula la
 *         correlacion media de los ultimos QRS con su promedio.
 * @param  rb  Posicion de la R (muestras atras).
 * @details Plantilla de Orphanidou: promedio de los QRS de la ventana. Si
 *          los latidos se parecen entre si, la correlacion es ~1; con ruido
 *          o artefactos la forma cambia de un "latido" a otro y cae.
 */
static void QRS_Template(uint16_t rb)
{
    if ((rb < QRS_SNIP_POST) || ((rb + QRS_SNIP_PRE) >= QRS_HIST_LEN)) { return; }

    float *s = snip[snip_i];
    float  mean = 0.0f;
    for (uint16_t k = 0u; k < QRS_SNIP_LEN; k++)
    {
        s[k] = QRS_H(rb + QRS_SNIP_PRE - k);
        mean += s[k];
    }
    mean *= 1.0f / (float)QRS_SNIP_LEN;
    for (uint16_t k = 0u; k < QRS_SNIP_LEN; k++) { s[k] -= mean; }
    snip_i = (uint8_t)((snip_i + 1u) % QRS_SQI_N);
    if (snip_n < QRS_SQI_N) { snip_n++; }
    if (snip_n < QRS_SQI_MIN) { return; }

    float tp[QRS_SNIP_LEN], ett = 0.0f, acc = 0.0f;
    for (uint16_t k = 0u; k < QRS_SNIP_LEN; k++)
    {
        float a = 0.0f;
        for (uint8_t j = 0u; j < snip_n; j++) { a += snip[j][k]; }
        tp[k] = a / (float)snip_n;
        ett  += tp[k] * tp[k];
    }
    for (uint8_t j = 0u; j < snip_n; j++)
    {
        float eqq = 0.0f, eqt = 0.0f;
        for (uint16_t k = 0u; k < QRS_SNIP_LEN; k++)
        {
            eqq += snip[j][k] * snip[j][k];
            eqt += snip[j][k] * tp[k];
        }
        acc += eqt / (sqrtf(eqq * ett) + 1e-9f);   /* coeficiente de correlacion */
    }
    cc_win = acc / (float)snip_n;
}

/**
 * @brief  Mide la duracion del QRS alrededor de la R.
 * @param  rb  Posicion de la R (muestras atras).
 * @details Inicio y fin = primer tramo ISOELECTRICO (QRS_W_RUN muestras
 *          seguidas con pendiente < 15 % de la maxima) a cada lado de la R.
 *          Se promedia exponencialmente (70 % historia, 30 % nuevo).
 */
static void QRS_Width(uint16_t rb)
{
    float dmax = 0.0f;
    for (int32_t b = (int32_t)rb - 15; b <= ((int32_t)rb + 15); b++)
    {
        if ((b < 1) || (b > ((int32_t)QRS_HIST_LEN - 2))) { continue; }
        float dd = 0.5f * fabsf(QRS_H(b + 1) - QRS_H(b - 1));
        if (dd > dmax) { dmax = dd; }
    }
    if (dmax <= 0.0f) { return; }

    float   thr_d = 0.15f * dmax;
    int32_t on = (int32_t)rb + 30, off = (int32_t)rb - 30;
    uint8_t c = 0u;
    for (int32_t b = (int32_t)rb + 3; (b < ((int32_t)rb + 30)) && (b < ((int32_t)QRS_HIST_LEN - 1)); b++)
    {
        float dd = 0.5f * fabsf(QRS_H(b + 1) - QRS_H(b - 1));
        c = (dd < thr_d) ? (uint8_t)(c + 1u) : 0u;
        if (c >= QRS_W_RUN) { on = b - (int32_t)(QRS_W_RUN - 1u); break; }
    }
    c = 0u;
    for (int32_t b = (int32_t)rb - 3; (b > ((int32_t)rb - 30)) && (b >= 1); b--)
    {
        float dd = 0.5f * fabsf(QRS_H(b + 1) - QRS_H(b - 1));
        c = (dd < thr_d) ? (uint8_t)(c + 1u) : 0u;
        if (c >= QRS_W_RUN) { off = b + (int32_t)(QRS_W_RUN - 1u); break; }
    }

    int32_t width = on - off;
    if ((width >= 10) && (width <= 40))            /* 40-160 ms plausibles */
    {
        w_avg = (w_avg == 0.0f) ? (float)width : ((0.3f * (float)width) + (0.7f * w_avg));
        ecg_qrs_ms = (uint16_t)((w_avg * (1000.0f / (float)ECG_FS_HZ)) + 0.5f);
    }
}

/**
 * @brief  Latido aceptado: niveles, R-R, plantilla, SQI y publicacion.
 * @param  pos      Muestra del pico del integrador.
 * @param  pk       Valor de ese pico.
 * @param  slope    Pendiente maxima del QRS.
 * @param  lag      Muestras desde el pico hasta ahora.
 * @param  abs_now  Indice absoluto de la muestra actual.
 */
static void QRS_Accept(uint32_t pos, float pk, float slope, uint32_t lag, uint32_t abs_now)
{
    uint32_t r = pos - last;
    last = pos;
    qs   = slope;
    QRS_PushQ(pk);
    ecg_beat_count++;

    float    amp;
    uint16_t rb = QRS_LocateR(lag, &amp);
    QRS_Template(rb);
    QRS_Width(rb);

    if ((r >= QRS_RR_MIN) && (r <= QRS_RR_MAX))
    {
        rr[rr_i] = (uint16_t)r;
        rr_i = (uint8_t)((rr_i + 1u) % QRS_SQI_N);
        if (rr_n < QRS_SQI_N) { rr_n++; }
        float f[QRS_SQI_N];
        for (uint8_t k = 0u; k < rr_n; k++) { f[k] = (float)rr[k]; }
        rr_med = QRS_Median(f, rr_n);
    }

    /* SQI de Orphanidou. */
    uint8_t ok = 0u;
    float   rmin = 1e9f, rmax = 0.0f, racc = 0.0f;
    if ((rr_n >= QRS_SQI_MIN) && (snip_n >= QRS_SQI_MIN))
    {
        for (uint8_t k = 0u; k < rr_n; k++)
        {
            float v = (float)rr[k];
            if (v < rmin) { rmin = v; }
            if (v > rmax) { rmax = v; }
            racc += v;
        }
        float hr = 60.0f * (float)ECG_FS_HZ / rr_med;
        ok = ((hr >= QRS_HR_MIN) && (hr <= QRS_HR_MAX) &&
              (rmax < (float)QRS_RR_MAX) && (rmax < (QRS_RR_RATIO * rmin)) &&
              (cc_win > QRS_CC_MIN) && (n > sat_until)) ? 1u : 0u;
    }
    ecg_sqi_cc = (uint8_t)((cc_win > 0.0f) ? ((cc_win * 100.0f) + 0.5f) : 0.0f);

    if (ok == 0u) { QRS_Invalidate(); return; }

    float var = 100.0f * (rmax - rmin) / (racc / (float)rr_n);
    ecg_rr_ms      = (uint16_t)((rr_med * (1000.0f / (float)ECG_FS_HZ)) + 0.5f);
    ecg_bpm        = (uint16_t)((60.0f * (float)ECG_FS_HZ / rr_med) + 0.5f);
    ecg_rr_var_pct = (uint8_t)((var > 99.0f) ? 99.0f : var);
    ecg_regular    = (var < QRS_IRREG_PCT) ? 1u : 0u;
    ecg_r_mv       = amp / ECG_COUNTS_PER_MV;
    ecg_sqi_ok     = 1u;

    /* Publicacion para la pantalla: primero la posicion, despues el contador. */
    ecg_beat_r_abs = abs_now - rb;
    QRS_BARRIER();
    ecg_beat_seq++;
}

/* ---------------------------------------------------------------------------
 * Funciones publicas
 * ------------------------------------------------------------------------- */
void ECG_QRS_Reset(void)
{
    memset(x5, 0, sizeof(x5));
    memset(win, 0, sizeof(win));
    memset(hist, 0, sizeof(hist));
    win_idx = 0u;  win_sum = 0u;  i1 = 0.0f;  i2 = 0.0f;
    n = 0u;  last = 0u;  sat_until = 0u;  lpk = 0.0f;
    qpk_n = 0u; qpk_i = 0u; npk_n = 0u; npk_i = 0u;  q_med = 0.0f;  nz_med = 0.0f;
    rr_n = 0u;  rr_i = 0u;  rr_med = 0.0f;
    cand = 0u;  cpk = 0.0f;  cs = 0.0f;  qs = 0.0f;  cpos = 0u;
    sbpk = 0.0f;  sbs = 0.0f;  sbpos = 0u;
    art_cnt = 0u;  art_i = 0u;
    hist_idx = 0u;  snip_n = 0u;  snip_i = 0u;  cc_win = 0.0f;  w_avg = 0.0f;
    ecg_rr_ms = 0u;  ecg_qrs_ms = 0u;  ecg_regular = 1u;  ecg_r_mv = 0.0f;
    ecg_rr_var_pct = 0u;  ecg_sqi_cc = 0u;
    QRS_Invalidate();
}

void ECG_QRS_Process(const float *pDet, const float *pEcg, uint16_t len,
                     uint32_t abs_base, uint8_t adc_sat)
{
    for (uint16_t i = 0u; i < len; i++)
    {
        uint32_t abs_now = abs_base + i;

        /* La historia guarda la senal de pantalla, no la de deteccion: el
           pasa-banda de 5-15 Hz deforma el QRS. */
        hist[hist_idx] = pEcg[i];
        hist_idx = (uint16_t)((hist_idx + 1u) & (QRS_HIST_LEN - 1u));
        if ((adc_sat != 0u) || (fabsf(pEcg[i]) > QRS_SAT_COUNTS)) { sat_until = n + QRS_SAT_HOLD; }

        /* 1. Derivada de 5 puntos: realza la pendiente rapida del QRS.
              y[n] = (2x[n] + x[n-1] - x[n-3] - 2x[n-4]) / 8          */
        x5[4] = x5[3];  x5[3] = x5[2];  x5[2] = x5[1];  x5[1] = x5[0];  x5[0] = pDet[i];
        float d  = ((2.0f * x5[0]) + x5[1] - x5[3] - (2.0f * x5[4])) * 0.125f;
        float ad = fabsf(d);

        /* 2. Cuadrado (a entero) y 3. ventana movil de 150 ms (suma corrida). */
        float sq = d * d * QRS_SQ_SCALE;
        if (sq > QRS_SQ_MAX) { sq = QRS_SQ_MAX; }
        uint32_t sqi = (uint32_t)(sq + 0.5f);
        win_sum -= win[win_idx];
        win[win_idx] = sqi;
        win_sum += sqi;
        if (++win_idx >= QRS_WIN_LEN) { win_idx = 0u; }

        /* 4. RMS de la pendiente: se umbrala en AMPLITUD, no en energia. Un
              latido de la mitad de amplitud vale la mitad y no un cuarto: la
              modulacion respiratoria ya no lo deja bajo el umbral. */
        float integ = sqrtf((float)win_sum * (1.0f / ((float)QRS_WIN_LEN * QRS_SQ_SCALE)));
        n++;

#if !ECG_QRS_STRICT_REF
        /* La FC vence si no hubo latido valido en 3 s o si el ADC satura. */
        if ((ecg_sqi_ok != 0u) && (((n - last) > QRS_TIMEOUT) || (n <= sat_until))) { QRS_Invalidate(); }
#endif

        uint8_t is_peak = ((i1 > i2) && (integ <= i1)) ? 1u : 0u;   /* maximo local en n-1 */
        float   pk_prev = i1;
        i2 = i1;
        i1 = integ;

        /* 5. Aprendizaje: 2 s solo midiendo el maximo. */
        if (n < QRS_LEARN)
        {
            if (integ > lpk) { lpk = integ; }
            continue;
        }

        /* 6. Umbral adaptativo de Hamilton-Tompkins. */
        float q  = (qpk_n != 0u) ? q_med : lpk;
        float nz = (npk_n != 0u) ? nz_med : 0.0f;
        if (nz > (0.5f * q)) { nz = 0.5f * q; }
        float thr = nz + (QRS_TH * (q - nz));

        if (cand == 0u)
        {
            if ((integ > thr) && ((n - last) > QRS_REFRACT))
            {
                cand = 1u;  cpk = integ;  cpos = n;  cs = ad;   /* empieza un candidato */
                continue;
            }
            /* Nivel de ruido: todos los maximos locales que no llegan al umbral. */
            if ((is_peak != 0u) && (pk_prev <= thr) && (pk_prev < q)) { QRS_PushN(pk_prev); }

            /* Searchback: mayor pico bajo el umbral desde el ultimo QRS. Si
               pasan 1,5 R-R sin latido, ese pico se acepta si supera la
               mitad del umbral (latido chico que no llego). */
            if ((n - last) > QRS_REFRACT)
            {
                if (integ > sbpk) { sbpk = integ; sbpos = n; }
                if (ad > sbs)     { sbs = ad; }
            }
            if ((rr_n != 0u) && (sbpk > (0.5f * thr)) && ((float)(n - last) > (QRS_SB_K * rr_med)))
            {
                if ((sbpk < (QRS_ARTIF_K * q)) && (((n - sbpos) + QRS_SB_MARGIN) < QRS_HIST_LEN))
                {
                    QRS_Accept(sbpos, sbpk, sbs, n - sbpos, abs_now);
                }
                sbpk = 0.0f;  sbs = 0.0f;
            }
            continue;
        }

        /* 7. Candidato en curso: se sigue el PICO del integrador y se decide
              cuando baja del umbral (o a los 150 ms). */
        if (integ > cpk) { cpk = integ; cpos = n; }
        if (ad > cs)     { cs = ad; }
        if ((integ > thr) && ((n - cpos) < QRS_WIN_LEN)) { continue; }
        cand = 0u;

        if (n <= sat_until) { sbpk = 0.0f;  sbs = 0.0f;  continue; }   /* saturado */

        /* 8. Artefacto: mucho mayor que los QRS recientes. Si se repite, la
              amplitud cambio de verdad (otra posicion, otro contacto). */
        if ((qpk_n != 0u) && (cpk > (QRS_ARTIF_K * q)))
        {
            art_pk[art_i] = cpk;
            art_i = (uint8_t)((art_i + 1u) % QRS_ART_RELEARN);
            if (++art_cnt < QRS_ART_RELEARN) { sbpk = 0.0f;  sbs = 0.0f;  continue; }
            memcpy(qpk, art_pk, sizeof(art_pk));
            qpk_n = QRS_ART_RELEARN;  qpk_i = QRS_ART_RELEARN % QRS_LVL_N;
            q_med = QRS_Median(qpk, qpk_n);
        }
        art_cnt = 0u;

        /* 9. Onda T: cerca del QRS anterior y con menos de la mitad de su
              pendiente (Pan-Tompkins). Cuenta como ruido. */
        if (((cpos - last) < QRS_TWAVE_WIN) && (qs > 0.0f) && (cs < (0.5f * qs)))
        {
            if (cpk < q) { QRS_PushN(cpk); }
            continue;
        }

        sbpk = 0.0f;  sbs = 0.0f;
        QRS_Accept(cpos, cpk, cs, n - cpos, abs_now);
    }
}
