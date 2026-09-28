"""
validacion.py — Tabla de validación del detector (Tabla 7 del informe).
=======================================================================

Para cada tipo de tramo del registro real (limpio / sin ECG / artefacto)
calcula, en % del tiempo:
  * "mostrada": cuánto tiempo el equipo muestra una FC (en vez de "---")
  * "errónea" : cuánto tiempo muestra una FC fuera de 55–85 lpm (la real ≈ 68)

y lo hace para:
  1. el detector anterior (sin índice de calidad)
  2. el detector de referencia en Python (detector_ref.py)
  3. el código C del firmware, si antes se corrió correr_detector_c.py

Además mide el acierto con los 8 ECG sintéticos (FC real conocida) y cuánto
coinciden la referencia en Python y el código C, muestra a muestra.

Uso:
    python correr_detector_c.py      (opcional, para incluir el código C)
    python validacion.py

Resultado en pantalla y en resultados/validacion.txt.
"""
import os
import numpy as np
from ecg_comun import (FS, TRAMOS, FC_REAL_RANGO, CASOS_SINTETICOS, DIR_RESULTADOS, asegurar_carpetas,
                       cargar_registro, filtrar_pasabanda, ecg_sintetico, cadena_filtros)
from detector_ref import detectar, detector_anterior


def puntaje(bpm):
    """% del tiempo con FC mostrada y con FC errónea, por tipo de tramo."""
    t = np.arange(len(bpm)) / FS
    acum = {}
    for lo, hi, tipo in TRAMOS:
        m = (t >= lo) & (t < hi); b = bpm[m]
        mostrada = np.mean(b > 0)
        erronea = np.mean((b > 0) & ((b < FC_REAL_RANGO[0]) | (b > FC_REAL_RANGO[1])))
        acum.setdefault(tipo, []).append((hi - lo, mostrada, erronea))
    out = {}
    for tipo, v in acum.items():
        w = np.array(v); dur = w[:, 0].sum()
        out[tipo] = (100 * np.sum(w[:, 0] * w[:, 1]) / dur, 100 * np.sum(w[:, 0] * w[:, 2]) / dur)
    return out


def main():
    asegurar_carpetas()
    lineas = []
    def p(s=""):
        print(s); lineas.append(s)

    t, ecg = cargar_registro()
    det = filtrar_pasabanda(ecg)
    print("Corriendo detectores sobre el registro real...")
    fuentes = [("Detector anterior", detector_anterior(det, ecg))]
    ref = detectar(det, ecg)
    fuentes.append(("Referencia Python", ref["bpm"]))
    csv_c = os.path.join(DIR_RESULTADOS, "detector_c_registro.csv")
    bpm_c = None
    if os.path.exists(csv_c):
        bpm_c = np.loadtxt(csv_c, delimiter=",", ndmin=2)[:, 0]
        fuentes.append(("Código C (firmware)", bpm_c))

    p(f"Registro real: {t[-1]:.0f} s. FC errónea = fuera de {FC_REAL_RANGO[0]}-{FC_REAL_RANGO[1]} lpm.")
    p("")
    p(f"{'':22s}| {'limpio':^23s} | {'sin ECG':^23s} | {'artefacto':^23s}")
    p(f"{'':22s}| {'mostrada':>10s} {'errónea':>11s} | {'mostrada':>10s} {'errónea':>11s} | {'mostrada':>10s} {'errónea':>11s}")
    for nombre, bpm in fuentes:
        r = puntaje(bpm)
        p(f"{nombre:22s}| " + " | ".join(f"{r[k][0]:9.1f}% {r[k][1]:10.1f}%" for k in ("limpio", "sin_ecg", "artefacto")))

    if bpm_c is not None:
        n = min(len(bpm_c), len(ref["bpm"]))
        p("")
        p(f"Coincidencia Python vs. C (FC publicada, muestra a muestra): "
          f"{100 * np.mean(bpm_c[:n] == ref['bpm'][:n]):.2f} %")

    p("")
    p("ECG sintéticos (FC real conocida; errónea = más de 10 % de diferencia; se ignoran los primeros 15 s):")
    for k, caso in enumerate(CASOS_SINTETICOS):
        _, x, _ = ecg_sintetico(**caso)
        _, y = cadena_filtros(x)
        b = detectar(filtrar_pasabanda(y), y)["bpm"][int(15 * FS):]
        real = caso.get("fc", 65)
        mostrada = 100 * np.mean(b > 0)
        erronea = 100 * np.mean((b > 0) & (np.abs(b - real) > 0.1 * real))
        p(f"  {str(caso):48s} FC real {real:3d} | mostrada {mostrada:5.1f} % | errónea {erronea:4.1f} %")

    with open(os.path.join(DIR_RESULTADOS, "validacion.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(lineas) + "\n")
    print("\n-> resultados/validacion.txt")


if __name__ == "__main__":
    main()
