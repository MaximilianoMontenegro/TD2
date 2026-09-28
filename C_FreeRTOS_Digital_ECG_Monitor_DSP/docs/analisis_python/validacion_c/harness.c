/*
 * harness.c — Banco de prueba en PC del detector del firmware (ecg_qrs.c).
 *
 * Lee un archivo de texto con una muestra de ECG filtrado por línea (counts,
 * 250 Hz), le aplica el MISMO pasa-banda 5-15 Hz que el firmware y corre
 * ECG_QRS_Process() muestra a muestra. Escribe una línea por muestra:
 *
 *     bpm,sqi_ok,sqi_cc,r_abs,qrs_ms
 *
 *   bpm    : FC publicada (0 = la pantalla muestra "---")
 *   sqi_ok : 1 = señal confiable
 *   sqi_cc : correlación media con la plantilla x 100
 *   r_abs  : índice de la onda R cuando se publica un latido válido nuevo, -1 si no
 *   qrs_ms : duración del QRS
 *
 * No hace falta compilarlo a mano: lo compila y ejecuta correr_detector_c.py.
 * A mano (con el compilador zig instalado por pip):
 *     python -m ziglang cc -O2 -I. harness.c ecg_qrs.c -o detector.exe
 *     detector.exe entrada.txt salida.csv
 */
#include <stdio.h>
#include <stdlib.h>
#include "ecg_qrs.h"

#define MAX_MUESTRAS 2000000

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uso: %s entrada.txt salida.csv\n", argv[0]); return 1; }
    FILE *fi = fopen(argv[1], "r");
    FILE *fo = fopen(argv[2], "w");
    if (!fi || !fo) { fprintf(stderr, "no se pudo abrir un archivo\n"); return 1; }

    static float ecg[MAX_MUESTRAS], det[MAX_MUESTRAS];
    char linea[128];
    int n = 0;
    while (n < MAX_MUESTRAS && fgets(linea, sizeof linea, fi)) {
        char *fin;
        float v = strtof(linea, &fin);
        if (fin != linea) { ecg[n++] = v; }
    }

    /* Pasa-banda 5-15 Hz en forma directa II transpuesta, igual que
       arm_biquad_cascade_df2T_f32 en el firmware. */
    const float b0 = 0.110874f, b1 = 0.0f, b2 = -0.110874f, a1 = 1.736345f, a2 = -0.778251f;
    float d1 = 0.0f, d2 = 0.0f;
    for (int i = 0; i < n; i++) {
        float x = ecg[i], y = b0 * x + d1;
        d1 = b1 * x + a1 * y + d2;
        d2 = b2 * x + a2 * y;
        det[i] = y;
    }

    ECG_QRS_Reset();
    uint32_t seq = ecg_beat_seq;
    for (int i = 0; i < n; i++) {
        ECG_QRS_Process(&det[i], &ecg[i], 1, (uint32_t)i, 0);
        int nuevo = (ecg_beat_seq != seq);
        seq = ecg_beat_seq;
        fprintf(fo, "%u,%u,%u,%d,%u\n", ecg_bpm, ecg_sqi_ok, ecg_sqi_cc,
                nuevo ? (int)ecg_beat_r_abs : -1, ecg_qrs_ms);
    }
    fclose(fo);
    fclose(fi);
    return 0;
}
