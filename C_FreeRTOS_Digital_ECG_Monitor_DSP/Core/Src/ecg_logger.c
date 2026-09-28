/**
 ******************************************************************************
 * @file    ecg_logger.c
 * @brief   Tarea de registro del ECG en microSD (CSV sobre FatFs).
 * @author  Grupo TD II - UTN FRBA
 ******************************************************************************
 */
#include "ecg_logger.h"
#include "ecg_qrs.h"
#include "fatfs.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "task.h"
#include <stdio.h>

/* ---------------------------------------------------------------------------
 * Configuracion
 * ------------------------------------------------------------------------- */
#define LOG_LINE_MAX      48u    /**< Peor caso de una fila del CSV [caracteres]. */
#define LOG_SYNC_BLOCKS   10u    /**< f_sync cada 10 bloques = ~1 s.              */
#define LOG_QUEUE_BLOCKS   4u    /**< Capacidad de la cola en bloques (400 ms).   */
#define LOG_RETRY_MS    2000u    /**< Reintento de montaje si no hay tarjeta.     */
#define LOG_MAX_FILES   9999u    /**< ECG_0001.CSV ... ECG_9999.CSV.              */

/* ---------------------------------------------------------------------------
 * Variables
 * ------------------------------------------------------------------------- */
volatile uint8_t  sd_logging_ok   = 0u;
volatile int      sd_fresult      = 0;
volatile uint32_t sd_write_errors = 0u;
volatile uint16_t sd_file_num     = 0u;

static StreamBufferHandle_t s_stream = NULL;   /**< DSP -> registro. */
static FIL                  s_file;            /**< Archivo abierto.  */

/* ---------------------------------------------------------------------------
 * Funciones privadas
 * ------------------------------------------------------------------------- */

/**
 * @brief  Monta la tarjeta y crea el proximo archivo libre ECG_nnnn.CSV.
 * @details Reintenta cada 2 s: la tarjeta puede insertarse despues del
 *          arranque. FA_CREATE_NEW falla con FR_EXIST si el nombre ya esta
 *          usado: asi nunca se pisa un registro anterior.
 */
static void ECG_Logger_OpenFile(void)
{
    static char fname[13];   /* "ECG_0001.CSV" + '\0' (formato 8.3, sin LFN) */

    for (;;)
    {
        sd_fresult = f_mount(&USERFatFS, USERPath, 1);
        if (sd_fresult == FR_OK)
        {
            for (uint16_t k = 1u; k <= LOG_MAX_FILES; k++)
            {
                (void)sprintf(fname, "ECG_%04u.CSV", (unsigned)k);
                sd_fresult = f_open(&s_file, fname, FA_CREATE_NEW | FA_WRITE);
                if (sd_fresult != FR_EXIST) { sd_file_num = k; break; }
            }
            if (sd_fresult == FR_OK) { return; }
            (void)f_mount(NULL, USERPath, 0);
        }
        osDelay(LOG_RETRY_MS);
    }
}

/**
 * @brief  Cuerpo de la tarea de registro (no retorna).
 * @param  argument  No usado.
 */
static void ECG_Logger_Task(void *argument)
{
    static const char header[] = "muestra,ecg,bpm,rr_ms,qrs_ms,r_mv,sqi\r\n";
    static char line[ECG_BLOCK_LEN * LOG_LINE_MAX];
    int16_t  block[ECG_BLOCK_LEN];
    UINT     bw;
    uint32_t idx = 0u;
    uint8_t  sync_div = 0u;
    (void)argument;

    ECG_Logger_OpenFile();
    (void)f_write(&s_file, header, sizeof(header) - 1u, &bw);
    (void)f_sync(&s_file);
    sd_logging_ok = 1u;

    /* Descarta lo acumulado mientras se montaba: el archivo arranca limpio. */
    (void)xStreamBufferReset(s_stream);

    for (;;)
    {
        size_t n = xStreamBufferReceive(s_stream, block, sizeof(block), portMAX_DELAY);
        if (n != sizeof(block)) { continue; }

        /* 25 filas de texto en RAM y una sola escritura por bloque. */
        char *w = line;
        for (uint16_t i = 0u; i < ECG_BLOCK_LEN; i++)
        {
            w += sprintf(w, "%lu,%d,%u,%u,%u,%.2f,%u\r\n",
                         (unsigned long)(idx++), block[i],
                         ecg_bpm, ecg_rr_ms, ecg_qrs_ms, ecg_r_mv, ecg_sqi_ok);
        }

        UINT len = (UINT)(w - line);
        if ((f_write(&s_file, line, len, &bw) != FR_OK) || (bw != len))
        {
            sd_write_errors++;
        }

        /* f_sync ~1 vez por segundo: si se corta la alimentacion se pierde
           como maximo el ultimo segundo, sin reescribir la FAT cada 100 ms. */
        if (++sync_div >= LOG_SYNC_BLOCKS)
        {
            (void)f_sync(&s_file);
            sync_div = 0u;
        }
    }
}

/* ---------------------------------------------------------------------------
 * Funciones publicas
 * ------------------------------------------------------------------------- */
void ECG_Logger_CreateQueue(void)
{
    s_stream = xStreamBufferCreate(ECG_BLOCK_LEN * sizeof(int16_t) * LOG_QUEUE_BLOCKS,
                                   ECG_BLOCK_LEN * sizeof(int16_t));
    configASSERT(s_stream != NULL);
}

void ECG_Logger_Start(void)
{
    configASSERT(xTaskCreate(ECG_Logger_Task, "LOG", ECG_LOG_TASK_STACK, NULL,
                             ECG_LOG_TASK_PRIO, NULL) == pdPASS);
}

void ECG_Logger_Push(const int16_t *block)
{
    const size_t bytes = ECG_BLOCK_LEN * sizeof(int16_t);
    if ((s_stream != NULL) && (xStreamBufferSpacesAvailable(s_stream) >= bytes))
    {
        (void)xStreamBufferSend(s_stream, block, bytes, 0u);
    }
}
