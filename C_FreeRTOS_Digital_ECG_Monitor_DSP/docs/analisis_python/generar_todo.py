"""
generar_todo.py — Genera todas las gráficas y la validación de una vez.
=======================================================================

Ejecuta, en orden:
  1. graficos_filtros.py     -> figuras 8, 9 y 10
  2. graficos_ecg.py         -> figuras 1, 11, 13 y 14
  3. correr_detector_c.py    -> compila y corre el código C del firmware
                                (se saltea si no está instalado ziglang)
  4. validacion.py           -> resultados/validacion.txt

Uso:
    python generar_todo.py

Tarda alrededor de un minuto (el detector en Python procesa 62 000 muestras
una por una, igual que el microcontrolador).
"""
import importlib.util, subprocess, sys, os

AQUI = os.path.dirname(os.path.abspath(__file__))


def correr(script):
    print(f"\n=== {script} ===")
    r = subprocess.run([sys.executable, os.path.join(AQUI, script)], cwd=AQUI)
    return r.returncode == 0


if __name__ == "__main__":
    correr("graficos_filtros.py")
    correr("graficos_ecg.py")
    if importlib.util.find_spec("ziglang") is not None:
        correr("correr_detector_c.py")
    else:
        print("\n(ziglang no está instalado: se saltea la prueba del código C. Para incluirla: pip install ziglang)")
    correr("validacion.py")
    print("\nListo. Figuras en figuras/ y resultados en resultados/.")
