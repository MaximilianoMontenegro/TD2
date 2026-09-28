"""
ecg_comun.py — Funciones y constantes compartidas por todos los scripts.
=========================================================================

No se ejecuta solo: lo importan los demás scripts. Contiene:

  * Los coeficientes de los filtros, IDÉNTICOS a los del firmware (ecg_dsp.c).
  * La lectura del registro CSV que graba el equipo en la microSD.
  * Un generador de ECG sintético (latidos conocidos + ruido) para pruebas.
  * La clasificación de los tramos del registro real de 249 s
    (limpio / sin ECG / artefacto) usada en la validación.

Uso desde otro script:
    from ecg_comun import cargar_registro, filtrar_pasabanda, FS
    t, ecg = cargar_registro()          # registro por defecto (datos/ECG_LOG.CSV)
    det = filtrar_pasabanda(ecg)        # señal de detección 5–15 Hz
"""
import os
import numpy as np
from scipy import signal

# --------------------------------------------------------------------------
# Constantes del equipo (ver ecg_config.h en el firmware)
# --------------------------------------------------------------------------
FS = 250.0                  # frecuencia de trabajo [Hz] (2000 Hz / 8)
FS_ADC = 2000.0             # frecuencia de muestreo del ADC [Hz]
COUNTS_PER_MV = 1365.0      # calibración del AD8232: counts del ADC por mV en la piel

AQUI = os.path.dirname(os.path.abspath(__file__))
CSV_DEFECTO = os.path.join(AQUI, "datos", "ECG_LOG.CSV")
DIR_FIGURAS = os.path.join(AQUI, "figuras")
DIR_RESULTADOS = os.path.join(AQUI, "resultados")

# --------------------------------------------------------------------------
# Coeficientes de los filtros (copiados de ecg_dsp.c)
# Formato scipy: b = numerador, a = denominador con a[0] = 1.
# (CMSIS guarda {b0, b1, b2, -a1, -a2}: por eso los signos de a1 y a2 cambian.)
# --------------------------------------------------------------------------
HP_B = [0.99115360, -1.98230719, 0.99115360]        # pasa-altos Butterworth 2º orden, 0,5 Hz
HP_A = [1.0, -1.98222893, 0.98238545]

FIR_TAPS = 83                                       # pasa-bajos FIR, Hamming, fc = 43 Hz
FIR_B = signal.firwin(FIR_TAPS, 43.0, window="hamming", fs=FS)

BP_B = [0.110874, 0.0, -0.110874]                   # pasa-banda 5–15 Hz (solo detección)
BP_A = [1.0, -1.736345, 0.778251]

# Alternativa IIR anterior (ECG_LP_FIR = 0 en el firmware)
NOTCH_B = [0.9438954, -0.5833578, 0.9438954]        # notch 50 Hz, Q = 8
NOTCH_A = [1.0, -0.5833578, 0.8877909]
LP1_B, LP1_A = [0.130380, 0.260760, 0.130380], [1.0, -0.602063, 0.123563]   # Butterworth 4º orden,
LP2_B, LP2_A = [0.175407, 0.350814, 0.175407], [1.0, -0.809946, 0.511573]   # 40 Hz (2 biquads)

# --------------------------------------------------------------------------
# Tramos del registro real datos/ECG_LOG.CSV, clasificados mirando la señal y
# el video de la prueba. (inicio [s], fin [s], tipo)
#   "limpio"   : ECG claro, FC real ≈ 68 lpm
#   "sin_ecg"  : contacto intermitente, solo ruido
#   "artefacto": movimiento / presión sobre los electrodos
# Si se analiza otro registro hay que armar su propia lista.
# --------------------------------------------------------------------------
TRAMOS = [(60, 78, "limpio"), (143, 150.5, "limpio"), (242.6, 247.8, "limpio"),
          (84, 88, "sin_ecg"), (96, 100, "sin_ecg"), (105, 130, "sin_ecg"),
          (160, 168, "sin_ecg"), (190, 222, "sin_ecg"),
          (25, 34, "artefacto"), (88, 96, "artefacto"), (170, 182, "artefacto")]
FC_REAL_RANGO = (55, 85)   # en el registro real, una FC fuera de este rango es errónea


def cargar_registro(ruta=None):
    """Lee un CSV grabado por el equipo.

    Acepta los dos formatos que tuvo el firmware:
      * viejo: muestra,adc,bpm,rr_ms,qrs_ms,r_mv
      * nuevo: muestra,ecg,bpm,rr_ms,qrs_ms,r_mv,sqi
    La columna adc / ecg es el ECG YA FILTRADO por el equipo, en counts.

    Devuelve (t, ecg): tiempo en segundos y ECG en counts (float).
    """
    ruta = ruta or CSV_DEFECTO
    d = np.genfromtxt(ruta, delimiter=",", names=True)
    col = "ecg" if "ecg" in d.dtype.names else "adc"
    ecg = d[col].astype(float)
    t = np.arange(len(ecg)) / FS
    return t, ecg


def filtrar_pasabanda(ecg):
    """Pasa-banda 5–15 Hz del camino de detección (igual que el firmware)."""
    return signal.lfilter(BP_B, BP_A, ecg)


def cadena_filtros(x_crudo):
    """Filtrado de pantalla del firmware: pasa-altos 0,5 Hz + FIR 43 Hz.

    x_crudo: señal a 250 Hz en counts, ya sin la continua de 2048.
    Devuelve (despues_del_pasaaltos, ecg_filtrado). El FIR retrasa 41 muestras.
    """
    hp = signal.lfilter(HP_B, HP_A, x_crudo)
    return hp, signal.lfilter(FIR_B, 1.0, hp)


def ecg_sintetico(segundos=90, fc=65, R=340, T=110, ruido=15, modulacion=0.5, semilla=3,
                  caidas=0.0, artefactos=0):
    """ECG sintético a 250 Hz con latidos en posiciones conocidas.

    Cada latido es una suma de gaussianas (P, Q, R, S, T). Se agregan:
    arritmia sinusal respiratoria (±6 %), modulación respiratoria de la
    amplitud de la R (modulacion), 50 Hz de 150 counts, deriva de línea de base
    de 0,3 Hz, ruido blanco, latidos "caídos" al 30 % (caidas = probabilidad) y
    artefactos grandes.

    Devuelve (t, x, tiempos_de_R): x en counts, SIN filtrar.
    """
    t = np.arange(0, segundos, 1 / FS)
    rng = np.random.default_rng(semilla)
    x = np.zeros_like(t)
    latidos = []
    tb = 0.3
    g = lambda a, m, s: a * np.exp(-((t - m) / s) ** 2 / 2)
    while tb < segundos:
        a = R * (1 + modulacion * np.sin(2 * np.pi * 0.25 * tb))
        if rng.random() < caidas:
            a *= 0.3
        x += (g(0.08 * R, tb - 0.17, 0.025) + g(-0.1 * a, tb - 0.025, 0.010) + g(a, tb, 0.011)
              + g(-0.25 * a, tb + 0.025, 0.010) + g(T, tb + 0.28, 0.05))
        latidos.append(tb)
        tb += 60 / fc * (1 + 0.06 * np.sin(2 * np.pi * 0.25 * tb))
    x += ruido * rng.standard_normal(t.size) + 150 * np.sin(2 * np.pi * 50.3 * t) + 80 * np.sin(2 * np.pi * 0.3 * t)
    for _ in range(artefactos):
        c = rng.uniform(10, segundos - 5)
        x += 600 * np.exp(-((t - c) / 0.15) ** 2)
    return t, x, np.array(latidos)


# Casos sintéticos de la validación (sección 7.6 del informe)
CASOS_SINTETICOS = [dict(modulacion=0.0), dict(modulacion=0.5), dict(modulacion=0.6, caidas=0.1),
                    dict(modulacion=0.4, artefactos=4), dict(modulacion=0.6, caidas=0.15, ruido=25),
                    dict(modulacion=0.3, T=250, ruido=20), dict(modulacion=0.5, fc=110, T=150),
                    dict(modulacion=0.5, fc=45)]


def asegurar_carpetas():
    os.makedirs(DIR_FIGURAS, exist_ok=True)
    os.makedirs(DIR_RESULTADOS, exist_ok=True)
