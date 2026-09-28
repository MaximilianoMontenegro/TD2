"""
graficos_filtros.py — Gráficas de los filtros digitales del equipo.
===================================================================

Genera en figuras/ (entre paréntesis, la figura equivalente del informe):

  fig08_respuesta_filtros.png  (Figura 8)  respuesta en frecuencia de:
        a) el promedio de 8 muestras de la decimación (a 2 kHz)
        b) el pasa-altos Butterworth de 0,5 Hz
        c) el pasa-bajos FIR de 83 coeficientes vs. la alternativa IIR anterior
        d) el pasa-banda de 5–15 Hz del detector
  fig09_coeficientes_fir.png   (Figura 9)  coeficientes del FIR (simetría = fase lineal)
  fig10_efecto_filtros.png     (Figura 10) una señal sintética con 50 Hz, deriva y ruido,
                                           antes y después de cada filtro

Los coeficientes son los mismos del firmware (ver ecg_comun.py).

Uso:
    python graficos_filtros.py
    python graficos_filtros.py --mostrar      (además abre las ventanas de matplotlib)

Para probar otro diseño, cambiar los coeficientes en ecg_comun.py (por ejemplo
FIR_TAPS o la frecuencia de corte de firwin) y volver a correr este script.
"""
import argparse, os
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
from scipy import signal
from ecg_comun import (FS, FS_ADC, COUNTS_PER_MV, HP_B, HP_A, FIR_B, FIR_TAPS, BP_B, BP_A, NOTCH_B, NOTCH_A,
                       LP1_B, LP1_A, LP2_B, LP2_A, DIR_FIGURAS, asegurar_carpetas, ecg_sintetico, cadena_filtros)

plt.rcParams.update({"font.size": 8.5, "axes.grid": True, "grid.alpha": 0.3})


def db(h):
    return 20 * np.log10(np.maximum(np.abs(h), 1e-6))


def respuesta_filtros():
    fig, ax = plt.subplots(2, 2, figsize=(7.4, 5.6))
    # a) promedio de 8 muestras = FIR de 8 coeficientes iguales a 1/8, a 2000 Hz
    f, h = signal.freqz(np.ones(8) / 8, 1, worN=4096, fs=FS_ADC)
    ax[0, 0].plot(f, db(h), color="#1d4ed8"); ax[0, 0].axvline(FS / 2, color="k", ls="--", lw=0.8)
    ax[0, 0].text(FS / 2 + 5, -8, "nueva Nyquist\n125 Hz", fontsize=7); ax[0, 0].set_xlim(0, 1000); ax[0, 0].set_ylim(-50, 3)
    ax[0, 0].set_title("a) Decimación: promedio de 8 a 2 kHz", fontsize=9, fontweight="bold")
    ax[0, 0].set_xlabel("Hz"); ax[0, 0].set_ylabel("dB")
    # b) pasa-altos (eje logarítmico: interesa lo que pasa cerca de 0,5 Hz)
    f, h = signal.freqz(HP_B, HP_A, worN=np.logspace(-2, 1.5, 800), fs=FS)
    ax[0, 1].semilogx(f, db(h), color="#1d4ed8"); ax[0, 1].axvline(0.5, color="k", ls="--", lw=0.8)
    ax[0, 1].text(0.55, -30, "fc = 0,5 Hz\n(−3 dB)", fontsize=7); ax[0, 1].set_ylim(-60, 3)
    ax[0, 1].set_title("b) Pasa-altos Butterworth 2º orden", fontsize=9, fontweight="bold"); ax[0, 1].set_xlabel("Hz (escala log)")
    # c) FIR actual vs. notch + Butterworth anterior
    f, h = signal.freqz(FIR_B, 1, worN=4096, fs=FS)
    _, hn = signal.freqz(NOTCH_B, NOTCH_A, worN=4096, fs=FS)
    _, h1 = signal.freqz(LP1_B, LP1_A, worN=4096, fs=FS)
    _, h2 = signal.freqz(LP2_B, LP2_A, worN=4096, fs=FS)
    ax[1, 0].plot(f, db(h), color="#15803d", lw=1.4, label=f"FIR {FIR_TAPS} coef. (actual)")
    ax[1, 0].plot(f, db(hn * h1 * h2), color="#b91c1c", lw=1, ls="--", label="notch + Butterworth (anterior)")
    ax[1, 0].axvline(50, color="k", ls=":", lw=0.8); ax[1, 0].text(51, -95, "50 Hz", fontsize=7)
    ax[1, 0].set_ylim(-110, 5); ax[1, 0].set_xlim(0, 125); ax[1, 0].legend(fontsize=7, loc="lower left")
    ax[1, 0].set_title("c) Pasa-bajos: FIR de fase lineal vs. IIR", fontsize=9, fontweight="bold")
    ax[1, 0].set_xlabel("Hz"); ax[1, 0].set_ylabel("dB")
    # d) pasa-banda de detección (escala lineal)
    f, h = signal.freqz(BP_B, BP_A, worN=4096, fs=FS)
    ax[1, 1].plot(f, np.abs(h), color="#7c3aed"); ax[1, 1].axvspan(5, 15, color="#ddd6fe", alpha=0.6)
    ax[1, 1].text(10, 0.3, "QRS", ha="center", fontsize=8); ax[1, 1].text(2.5, 0.85, "T, P\ny deriva", fontsize=7, ha="center")
    ax[1, 1].text(40, 0.6, "ruido muscular\ny 50 Hz", fontsize=7, ha="center"); ax[1, 1].set_xlim(0, 60)
    ax[1, 1].set_title("d) Pasa-banda 5–15 Hz (solo detección)", fontsize=9, fontweight="bold")
    ax[1, 1].set_xlabel("Hz"); ax[1, 1].set_ylabel("|H| (lineal)")
    fig.tight_layout()
    return fig


def coeficientes_fir():
    fig, ax = plt.subplots(figsize=(7.2, 2.4))
    ax.stem(np.arange(FIR_TAPS), FIR_B, basefmt=" ", linefmt="#15803d", markerfmt="o")
    c = (FIR_TAPS - 1) // 2
    ax.axvline(c, color="k", ls="--", lw=0.8)
    ax.text(c + 1, FIR_B.max() * 0.9, f"centro: n = {c}\nretardo = {c} / 250 Hz = {1000 * c / FS:.0f} ms", fontsize=7.5)
    ax.set_title("Coeficientes del FIR (simétricos → fase lineal: todas las frecuencias se retrasan igual)",
                 fontsize=9, fontweight="bold")
    ax.set_xlabel("n (coeficiente)")
    return fig


def efecto_filtros():
    t, x, _ = ecg_sintetico(segundos=12, fc=70, ruido=15, modulacion=0.0, semilla=3)
    hp, y = cadena_filtros(x)
    retardo = (FIR_TAPS - 1) // 2
    m = (t >= 6) & (t < 10)
    fig, ax = plt.subplots(3, 1, figsize=(7.2, 5.2), sharex=True)
    ax[0].plot(t[m], x[m] / COUNTS_PER_MV, color="#64748b", lw=0.7)
    ax[0].set_title("Entrada: ECG + 50 Hz + deriva de línea de base (0,3 Hz) + ruido", fontsize=9, fontweight="bold")
    ax[1].plot(t[m], hp[m] / COUNTS_PER_MV, color="#1d4ed8", lw=0.7)
    ax[1].set_title("Después del pasa-altos 0,5 Hz: línea de base centrada en 0", fontsize=9, fontweight="bold")
    ax[2].plot(t[m] - retardo / FS, y[m] / COUNTS_PER_MV, color="#15803d", lw=1)
    ax[2].set_title(f"Después del FIR (corregido el retardo de {1000 * retardo / FS:.0f} ms): sin 50 Hz ni ruido",
                    fontsize=9, fontweight="bold")
    for a in ax:
        a.set_ylabel("mV")
    ax[2].set_xlabel("tiempo [s]  (señal sintética de prueba)")
    fig.tight_layout()
    return fig


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mostrar", action="store_true", help="abrir las figuras en pantalla")
    args = ap.parse_args()
    if not args.mostrar:
        matplotlib.use("Agg")
    asegurar_carpetas()
    for nombre, fn in [("fig08_respuesta_filtros.png", respuesta_filtros),
                       ("fig09_coeficientes_fir.png", coeficientes_fir),
                       ("fig10_efecto_filtros.png", efecto_filtros)]:
        fig = fn()
        fig.savefig(os.path.join(DIR_FIGURAS, nombre), dpi=200, bbox_inches="tight")
        print("  figuras/" + nombre)
    if args.mostrar:
        plt.show()


if __name__ == "__main__":
    main()
