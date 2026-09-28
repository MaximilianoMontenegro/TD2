/**
 ******************************************************************************
 * @file    ecg_display.c
 * @brief   Tarea de pantalla: panel de parametros y grafico del latido.
 *
 * @details Escala del grafico (como el papel de ECG, ampliado):
 *          - Horizontal: 1 pixel = 1 muestra = 4 ms; grilla fina cada 10 px
 *            (40 ms) y gruesa cada 50 px (200 ms).
 *          - Vertical, ganancia X1: 100 px = 1 mV (10 px = 0,1 mV). La
 *            ganancia automatica elige entre X0,5 y X8.
 *          El panel solo redibuja lo que cambio (cache): cada caracter cuesta
 *          varios ms en el bus paralelo y redibujar todo saturaba la tarea.
 *
 * @author  Grupo TD II - UTN FRBA
 ******************************************************************************
 */
#include "ecg_display.h"
#include "ecg_dsp.h"
#include "ecg_qrs.h"
#include "ili9341.h"
#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "task.h"

/* ---------------------------------------------------------------------------
 * Configuracion
 * ------------------------------------------------------------------------- */
#define DISPLAY_ROTATION    0      /**< 0 o 2: si se ve cabeza abajo, cambiar.     */
#define PANEL_H           156      /**< Franja de datos: filas 0..155.             */
#define GRAPH_Y           160      /**< Grafico: filas 160..319 (= ECG_GRID_TOP).  */
#define GRAPH_H           160      /**< Alto del grafico en pixeles.               */
#define GRAPH_W           240      /**< Ancho del grafico = muestras dibujadas.    */
#define ECG_INVERT          1      /**< 1 si la R apunta hacia abajo (polaridad).  */

#define ECG_RING_LEN      512u     /**< Anillo de 2 s de senal (potencia de 2).    */
#define BEAT_PRE           96u     /**< Muestras antes de la R (384 ms: onda P).   */
#define BEAT_POST   ((uint32_t)GRAPH_W - BEAT_PRE)  /**< 576 ms despues: onda T.   */
#define ECG_REFRESH       250u     /**< Refresco del trazo: 250 muestras = 1 s.    */
#define ECG_FREE_RUN      750u     /**< Sin latido valido en 3 s: senal libre.     */
#define STREAM_MSGS         8u     /**< Capacidad del StreamBuffer en mensajes.    */

#define ECG_PX_PER_MV   100.0f     /**< Ganancia X1: 10 px = 0,1 mV (10 mm/mV).    */
#define ECG_GRID_BOLD   0xC800u    /**< Rojo claro (RGB565): cuadro grande.        */

/* ---------------------------------------------------------------------------
 * Variables
 * ------------------------------------------------------------------------- */
volatile uint32_t disp_gap_count = 0u;
static StreamBufferHandle_t s_stream = NULL;   /**< DSP -> pantalla. */

/* ---------------------------------------------------------------------------
 * Funciones privadas
 * ------------------------------------------------------------------------- */

/**
 * @brief  Dibuja el estado de un parametro frente a su rango normal [lo, hi].
 * @param  y      Fila de la pantalla.
 * @param  shown  Cache: estado que ya esta dibujado (se actualiza).
 * @param  valid  0 = sin dato confiable ("---").
 * @param  v      Valor medido.
 * @param  lo     Limite inferior normal.
 * @param  hi     Limite superior normal.
 */
static void ECG_Display_DrawStatus(uint16_t y, int8_t *shown, uint8_t valid,
                                   float v, float lo, float hi)
{
    static const char *const txt[] = { "---    ", "BAJO   ", "ALTO   ", "NORMAL " };
    int8_t st = (valid == 0u) ? 0 : ((v < lo) ? 1 : ((v > hi) ? 2 : 3));

    if (st == *shown) { return; }
    *shown = st;
    ILI9341_WriteString(150, y, txt[st],
                        (st == 3) ? ILI9341_GREEN : ((st == 0) ? ILI9341_WHITE : ILI9341_YELLOW),
                        ILI9341_BLUE, 2);
}

/**
 * @brief  Escribe un valor numerico en el panel, o "---" si no es valido.
 * @param  x, y     Posicion.
 * @param  v        Valor; negativo = no valido.
 * @param  scale    El valor mostrado es v / scale (1 o 100).
 * @param  dec      Decimales.
 * @param  unit     Unidad ("MS", "MV", "%", "").
 * @param  width    Ancho fijo del campo en caracteres.
 * @param  size     Tamano de letra (2 o 3).
 */
static void ECG_Display_Value(uint16_t x, uint16_t y, int32_t v, float scale,
                              uint8_t dec, const char *unit, uint8_t width, uint8_t size)
{
    if (v < 0)
    {
        ILI9341_WriteString(x, y, (width <= 3u) ? "---" : "---    ",
                            ILI9341_WHITE, ILI9341_BLUE, size);
    }
    else
    {
        ILI9341_UpdateValueFixed(x, y, (float)v / scale, dec, unit, width,
                                 ILI9341_WHITE, ILI9341_BLUE, size);
    }
}

/**
 * @brief  Actualiza el panel de datos (solo lo que cambio).
 * @param  gidx  Indice de la ganancia actual del grafico.
 * @details Sin senal limpia (SQI de ecg_qrs.c) no se muestra ningun valor:
 *          antes el ruido producia FC de 175 y VAR de 99 % como si fueran reales.
 */
static void ECG_Display_UpdatePanel(uint8_t gidx)
{
    static const char *const gain_txt[] = { "X0.5", "X1  ", "X1.5", "X2  ",
                                            "X3  ", "X4  ", "X6  ", "X8  " };
    static int32_t c_gain = -1, c_lead = -1, c_bpm = -2, c_rr = -2;
    static int32_t c_qrs = -2, c_rmv = -2, c_var = -2, c_rit = -1;
    static int8_t  c_st[4] = { -1, -1, -1, -1 };

    uint8_t lead = ecg_lead_status;
    uint8_t ok   = ((ecg_sqi_ok != 0u) && (lead == 0u)) ? 1u : 0u;
    int32_t v;

    if ((int32_t)gidx != c_gain)
    {
        c_gain = gidx;
        ILI9341_WriteString(48, 3, gain_txt[gidx], ILI9341_WHITE, ILI9341_BLUE, 2);
    }

    /* Estado de la senal: 0 = electrodo suelto, 1 = ruido, 2 = limpia. */
    v = (lead != 0u) ? 0 : ((ok != 0u) ? 2 : 1);
    if (v != c_lead)
    {
        c_lead = v;
        ILI9341_WriteString(144, 3,
                            (v == 0) ? "LEAD OFF" : ((v == 1) ? "   RUIDO" : "      OK"),
                            (v == 0) ? ILI9341_RED : ((v == 1) ? ILI9341_YELLOW : ILI9341_GREEN),
                            ILI9341_BLUE, 2);
    }

    /* Valores en vivo (-1 = "---"). */
    v = ok ? (int32_t)ecg_bpm : -1;
    if (v != c_bpm) { c_bpm = v; ECG_Display_Value(46, 26, v, 1.0f, 0u, "", 3u, 3u); }

    v = ok ? (int32_t)ecg_rr_ms : -1;
    if (v != c_rr)  { c_rr = v;  ECG_Display_Value(46, 62, v, 1.0f, 0u, "MS", 7u, 2u); }

    v = (ok && (ecg_qrs_ms > 0u)) ? (int32_t)ecg_qrs_ms : -1;
    if (v != c_qrs) { c_qrs = v; ECG_Display_Value(46, 87, v, 1.0f, 0u, "MS", 7u, 2u); }

    v = ok ? (int32_t)((ecg_r_mv * 100.0f) + 0.5f) : -1;
    if (v != c_rmv) { c_rmv = v; ECG_Display_Value(46, 112, v, 100.0f, 2u, "MV", 7u, 2u); }

    v = ok ? (int32_t)ecg_rr_var_pct : -1;
    if (v != c_var) { c_var = v; ECG_Display_Value(46, 137, v, 1.0f, 0u, "%", 7u, 2u); }

    /* Estado frente a la referencia normal del adulto en reposo. */
    ECG_Display_DrawStatus( 36, &c_st[0], ok, (float)ecg_bpm,   60.0f,  100.0f);
    ECG_Display_DrawStatus( 66, &c_st[1], ok, (float)ecg_rr_ms, 600.0f, 1000.0f);
    ECG_Display_DrawStatus( 91, &c_st[2], (uint8_t)(ok && (ecg_qrs_ms > 0u)),
                           (float)ecg_qrs_ms, 0.0f, 119.0f);
    ECG_Display_DrawStatus(116, &c_st[3], ok, ecg_r_mv, 0.5f, 2.0f);

    v = (ok == 0u) ? 0 : ((ecg_regular != 0u) ? 1 : 2);
    if (v != c_rit)
    {
        c_rit = v;
        if (v == 0)      { ILI9341_WriteString(150, 141, "---    ", ILI9341_WHITE,  ILI9341_BLUE, 2); }
        else if (v == 1) { ILI9341_WriteString(150, 141, "REGULAR", ILI9341_GREEN,  ILI9341_BLUE, 2); }
        else             { ILI9341_WriteString(150, 141, "IRREG. ", ILI9341_YELLOW, ILI9341_BLUE, 2); }
    }
}

/**
 * @brief  Dibuja GRAPH_W muestras consecutivas del anillo.
 * @param  ring   Anillo de ECG_RING_LEN muestras.
 * @param  start  Indice absoluto de la primera muestra a dibujar.
 * @param  gidx   Ganancia actual (entrada/salida).
 * @param  adapt  1 = recalcular la ganancia (latido valido); 0 = congelada
 *                (senal libre, ruido o artefacto: antes el ruido subia la
 *                ganancia a X4-X8 y se veia amplificado).
 * @details El trazo se centra verticalmente por (max + min) / 2 de la ventana.
 *          La ganancia sale del mayor pico a pico de los ultimos 4 dibujos
 *          (no salta con la respiracion): sube a la mayor que deje el trazo
 *          en <= 80 % de la altura y baja solo si se pasa del 95 %.
 */
static void ECG_Display_DrawWindow(const int16_t *ring, uint32_t start, uint8_t *gidx, uint8_t adapt)
{
    static const float gains[] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f };
    static float   p2p_hist[4];
    static uint8_t ph = 0u;
    const uint8_t  gmax = (uint8_t)((sizeof(gains) / sizeof(gains[0])) - 1u);

    int16_t vmin = ring[start & (ECG_RING_LEN - 1u)];
    int16_t vmax = vmin;
    for (uint16_t c = 1u; c < GRAPH_W; c++)
    {
        int16_t v = ring[(start + c) & (ECG_RING_LEN - 1u)];
        if (v < vmin) { vmin = v; }
        if (v > vmax) { vmax = v; }
    }

    if (adapt != 0u)
    {
        p2p_hist[ph++ & 3u] = (float)(vmax - vmin);
        float p2p = p2p_hist[0];
        for (uint8_t k = 1u; k < 4u; k++) { if (p2p_hist[k] > p2p) { p2p = p2p_hist[k]; } }
        float p2p_px = (p2p / ECG_COUNTS_PER_MV) * ECG_PX_PER_MV;

        uint8_t best = 0u;
        for (uint8_t g = 0u; g <= gmax; g++)
        {
            if ((p2p_px * gains[g]) <= (0.80f * (float)GRAPH_H)) { best = g; }
        }
        if ((best > *gidx) || ((p2p_px * gains[*gidx]) > (0.95f * (float)GRAPH_H))) { *gidx = best; }
    }

    const float kpx    = ECG_PX_PER_MV * gains[*gidx] / ECG_COUNTS_PER_MV;
    const float centro = 0.5f * ((float)vmax + (float)vmin);
    int16_t yPrev = 0;

    for (uint16_t c = 0u; c < GRAPH_W; c++)
    {
        float dy = ((float)ring[(start + c) & (ECG_RING_LEN - 1u)] - centro) * kpx;
#if ECG_INVERT
        dy = -dy;
#endif
        /* Pantalla vertical: y crece hacia abajo, la R queda arriba. */
        int32_t y = (GRAPH_Y + (GRAPH_H / 2)) - (int32_t)dy;
        if (y < GRAPH_Y)                 { y = GRAPH_Y; }
        if (y > (GRAPH_Y + GRAPH_H - 1)) { y = GRAPH_Y + GRAPH_H - 1; }
        if (c == 0u)                     { yPrev = (int16_t)y; }

        /* Grilla de papel: linea fina cada 10 px (40 ms), gruesa cada 50 px (200 ms). */
        uint8_t vgrid = ((c % 50u) == 0u) ? 2u : (((c % 10u) == 0u) ? 1u : 0u);

        ILI9341_DrawGraphColumn(c, yPrev, (int16_t)y, vgrid,
                                ILI9341_GREEN, ECG_GRID_BOLD, ILI9341_BLACK);
        yPrev = (int16_t)y;
    }
}

/* ---------------------------------------------------------------------------
 * Funciones publicas
 * ------------------------------------------------------------------------- */
void ECG_Display_Init(void)
{
    ILI9341_Init();
    ILI9341_SetRotation(DISPLAY_ROTATION);
    ILI9341_FillScreen(ILI9341_BLACK);

    ILI9341_FillRectangle(0, 0, GRAPH_W, PANEL_H, ILI9341_BLUE);
    ILI9341_DrawHLine(0, 20, GRAPH_W, ILI9341_WHITE);
    ILI9341_DrawVLine(146, 22, PANEL_H - 22, ILI9341_WHITE);

    ILI9341_WriteString(4, 3, "ECG", ILI9341_WHITE, ILI9341_BLUE, 2);

    /* Nombres de los parametros. */
    ILI9341_WriteString(4,   30, "FC",  ILI9341_CYAN, ILI9341_BLUE, 2);
    ILI9341_WriteString(104, 40, "LPM", ILI9341_CYAN, ILI9341_BLUE, 1);
    ILI9341_WriteString(4,   62, "RR",  ILI9341_CYAN, ILI9341_BLUE, 2);
    ILI9341_WriteString(4,   87, "QRS", ILI9341_CYAN, ILI9341_BLUE, 2);
    ILI9341_WriteString(4,  112, "R",   ILI9341_CYAN, ILI9341_BLUE, 2);
    ILI9341_WriteString(4,  137, "VAR", ILI9341_CYAN, ILI9341_BLUE, 2);

    /* Referencias normales en adulto en reposo (derivacion I / II). */
    ILI9341_WriteString(150,  24, "60<=FC<=100",   ILI9341_WHITE, ILI9341_BLUE, 1);
    ILI9341_WriteString(150,  57, "600<=RR<=1000", ILI9341_WHITE, ILI9341_BLUE, 1);
    ILI9341_WriteString(150,  82, "QRS < 120 MS",  ILI9341_WHITE, ILI9341_BLUE, 1);
    ILI9341_WriteString(150, 107, "0.5<=R<=2.0MV", ILI9341_WHITE, ILI9341_BLUE, 1);
    ILI9341_WriteString(150, 132, "VAR RR < 20%",  ILI9341_WHITE, ILI9341_BLUE, 1);

    HAL_Delay(20);
}

void ECG_Display_CreateQueue(void)
{
    s_stream = xStreamBufferCreate(sizeof(ecg_msg_t) * STREAM_MSGS, sizeof(ecg_msg_t));
    configASSERT(s_stream != NULL);
}

void ECG_Display_Push(const ecg_msg_t *msg)
{
    if ((s_stream != NULL) &&
        (xStreamBufferSpacesAvailable(s_stream) >= sizeof(ecg_msg_t)))
    {
        (void)xStreamBufferSend(s_stream, msg, sizeof(ecg_msg_t), 0u);
    }
}

void ECG_Display_Task(void *argument)
{
    static int16_t   ring[ECG_RING_LEN];
    static ecg_msg_t msg;
    uint32_t latest = 0u, last_draw = 0u;
    uint32_t pend_r = 0u, ready_r = 0u, drawn_r = 0u, gap_end = 0u;
    uint32_t seen_seq = ecg_beat_seq;
    uint8_t  pend = 0u, ready = 0u, gidx = 1u;
    (void)argument;

    for (;;)
    {
        /* Duerme hasta que llega un bloque (cada 100 ms). */
        if (xStreamBufferReceive(s_stream, &msg, sizeof(msg), portMAX_DELAY) != sizeof(msg))
        {
            continue;
        }

        /* Bloques perdidos: el hueco se rellena con el ultimo valor. Si no,
           quedaria senal de hace 2 s en el anillo y se dibujaria basura. */
        if ((latest != 0u) && (msg.base != latest))
        {
            uint32_t gap  = msg.base - latest;
            int16_t  last = ring[(latest - 1u) & (ECG_RING_LEN - 1u)];
            if (gap > ECG_RING_LEN) { gap = ECG_RING_LEN; }
            for (uint32_t k = 0u; k < gap; k++)
            {
                ring[(msg.base - gap + k) & (ECG_RING_LEN - 1u)] = last;
            }
            gap_end = msg.base;
            disp_gap_count++;
        }

        for (uint16_t i = 0u; i < ECG_BLOCK_LEN; i++)
        {
            ring[(msg.base + i) & (ECG_RING_LEN - 1u)] = msg.s[i];
        }
        latest = msg.base + ECG_BLOCK_LEN;

        /* Nuevo latido valido publicado por el detector. */
        uint32_t seq = ecg_beat_seq;
        if (seq != seen_seq)
        {
            seen_seq = seq;
            __DMB();                  /* leer la posicion despues del contador */
            pend_r = ecg_beat_r_abs;
            pend   = 1u;
        }

        /* El latido esta listo cuando llego toda su ventana (hasta la onda T). */
        if ((pend != 0u) && (latest >= (pend_r + BEAT_POST)))
        {
            ready_r = pend_r;
            ready   = 1u;
            pend    = 0u;
        }

        /* Refresco cada 1 s. */
        if ((latest - last_draw) >= ECG_REFRESH)
        {
            last_draw = latest;

            if ((ready != 0u) && ((latest - ready_r) < ECG_FREE_RUN))
            {
                /* Solo latidos enteros en el anillo y sin huecos dentro. */
                if ((ready_r != drawn_r) && (ready_r >= BEAT_PRE) &&
                    ((ready_r - BEAT_PRE) >= gap_end) &&
                    ((latest - (ready_r - BEAT_PRE)) <= ECG_RING_LEN))
                {
                    ECG_Display_DrawWindow(ring, ready_r - BEAT_PRE, &gidx, 1u);
                    drawn_r = ready_r;
                }
            }
            else if (latest >= GRAPH_W)
            {
                ECG_Display_DrawWindow(ring, latest - GRAPH_W, &gidx, 0u);
            }

            ECG_Display_UpdatePanel(gidx);
        }
    }
}
