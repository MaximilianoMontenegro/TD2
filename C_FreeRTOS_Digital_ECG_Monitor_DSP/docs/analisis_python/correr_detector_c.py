"""
correr_detector_c.py — Compila y corre en la PC el detector C del firmware.
===========================================================================

Toma Core/Src/ecg_qrs.c (el MISMO archivo que se graba en la placa), lo
compila junto con validacion_c/harness.c y lo ejecuta sobre:
  * el registro real (datos/ECG_LOG.CSV o el que se indique con --csv)
  * los 8 casos de ECG sintético de la validación

Los resultados quedan en resultados/:
  * detector_c_registro.csv      una fila por muestra: bpm,sqi_ok,sqi_cc,r_abs,qrs_ms
  * detector_c_sintetico_N.csv   ídem para cada caso sintético

Requisitos: Python con numpy/scipy y el compilador de C "zig", que se instala
con pip (no hace falta instalar nada más):
    pip install ziglang

Uso:
    python correr_detector_c.py
    python correr_detector_c.py --csv D:/ECG_0003.CSV     (otro registro de la SD)
"""
import argparse, os, shutil, subprocess, sys
import numpy as np
from ecg_comun import (AQUI, DIR_RESULTADOS, CASOS_SINTETICOS, asegurar_carpetas, cargar_registro,
                       ecg_sintetico, cadena_filtros)

DIR_C = os.path.join(AQUI, "validacion_c")
EXE = os.path.join(DIR_C, "detector.exe" if sys.platform.startswith("win") else "detector")
# Carpeta del firmware (este paquete vive en <proyecto>/docs/analisis_python)
PROYECTO = os.path.abspath(os.path.join(AQUI, "..", ".."))


def copiar_fuentes_del_firmware():
    """Trae la versión actual de ecg_qrs.c/.h y ecg_config.h desde el proyecto.
    Si el paquete se copió a otra PC sin el proyecto, usa las copias incluidas."""
    for sub, f in [("Core/Src", "ecg_qrs.c"), ("Core/Inc", "ecg_qrs.h"), ("Core/Inc", "ecg_config.h")]:
        src = os.path.join(PROYECTO, sub, f)
        if os.path.exists(src):
            shutil.copy(src, os.path.join(DIR_C, f))


def compilar():
    copiar_fuentes_del_firmware()
    cmd = [sys.executable, "-m", "ziglang", "cc", "-O2", "-I", DIR_C,
           os.path.join(DIR_C, "harness.c"), os.path.join(DIR_C, "ecg_qrs.c"), "-o", EXE]
    print("Compilando:", " ".join(os.path.basename(c) for c in cmd[3:]))
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr)
        sys.exit("Error de compilación. ¿Está instalado ziglang?  ->  pip install ziglang")


def correr(ecg, nombre):
    """Ejecuta el detector C sobre un vector de ECG filtrado y devuelve su salida."""
    entrada = os.path.join(DIR_RESULTADOS, "_entrada.txt")
    salida = os.path.join(DIR_RESULTADOS, nombre)
    np.savetxt(entrada, ecg, fmt="%.3f")
    subprocess.run([EXE, entrada, salida], check=True)
    os.remove(entrada)
    return np.loadtxt(salida, delimiter=",", ndmin=2)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--csv", help="registro CSV de la SD (por defecto datos/ECG_LOG.CSV)")
    args = ap.parse_args()
    asegurar_carpetas()
    compilar()
    _, ecg = cargar_registro(args.csv)
    out = correr(ecg, "detector_c_registro.csv")
    print(f"Registro: {len(ecg)} muestras, FC válida el {100 * np.mean(out[:, 1] > 0):.1f} % del tiempo "
          f"-> resultados/detector_c_registro.csv")
    for k, caso in enumerate(CASOS_SINTETICOS):
        _, x, _ = ecg_sintetico(**caso)
        _, y = cadena_filtros(x)
        correr(y, f"detector_c_sintetico_{k}.csv")
    print(f"{len(CASOS_SINTETICOS)} casos sintéticos -> resultados/detector_c_sintetico_N.csv")


if __name__ == "__main__":
    main()
