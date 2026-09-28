"""
graficos_ecg.py — Gráficas del ECG real, del detector y del índice de calidad.
==============================================================================

Usa el registro grabado por el equipo en la microSD (datos/ECG_LOG.CSV) y el
detector de referencia en Python (detector_ref.py, igual al del firmware).
Genera en figuras/ (entre paréntesis, la figura equivalente del informe):

  fig01_ecg_ondas.png            (Figura 1)  4 s de señal limpia con las R detectadas y la FC;
                                             latido promedio con las ondas P, Q, R, S y T
  fig11_etapas_pan_tompkins.png  (Figura 11) ECG, pasa-banda, derivada², integrador y umbral
  fig13_plantilla_sqi.png        (Figura 13) plantilla y correlación: tramo limpio vs. ruido
  fig14_registro_completo.png    (Figura 14) registro completo con los tramos clasificados y
                                             la FC del detector anterior vs. el actual

Uso:
    python graficos_ecg.py
    python graficos_ecg.py --mostrar
    python graficos_ecg.py --csv D:/ECG_0002.CSV --limpio 30 45 --ruido 60 70

  --csv       otro registro de la SD (formato viejo o nuevo, se detecta solo)
  --limpio    tramo [s] con buena señal para las figuras 1, 11 y 13 (izquierda)
  --ruido     tramo [s] con ruido para la figura 13 (derecha)
  Con un registro distinto del original no se sombrean los tramos en la
  figura 14 (la clasificación TRAMOS de ecg_comun.py es solo del registro original).
"""
import argparse, os
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
from ecg_comun import (FS, COUNTS_PER_MV, TRAMOS, DIR_FIGURAS, asegurar_carpetas, cargar_registro,
                       filtrar_pasabanda)
from detector_ref import detectar, detector_anterior

plt.rcParams.update({"font.size": 8.5, "axes.grid": True, "grid.alpha": 0.3})
COLOR_TRAMO = {"limpio": "#bbf7d0", "sin_ecg": "#e5e7eb", "artefacto": "#fecaca"}


def ecg_con_ondas(ym, t, r_val, ini):
    """Figura 1: tira de 4 s + latido promedio con las ondas marcadas."""
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(7.2, 5.6), gridspec_kw={"height_ratios": [1, 1.25]})
    m = (t >= ini) & (t < ini + 4)
    a1.plot(t[m], ym[m], color="#15803d", lw=1)
    rr_ = r_val[(r_val >= ini * FS) & (r_val < (ini + 4) * FS)]
    a1.plot(rr_ / FS, ym[rr_], "v", color="#b91c1c", ms=5)
    if len(rr_) >= 2:
        r0, r1 = rr_[0] / FS, rr_[1] / FS; yy = ym[rr_].max() + 0.12
        a1.annotate("", xy=(r1, yy), xytext=(r0, yy), arrowprops=dict(arrowstyle="<->", color="#1d4ed8"))
        a1.text((r0 + r1) / 2, yy + 0.04, f"R-R = {r1 - r0:.2f} s  →  FC = 60 / R-R = {60 / (r1 - r0):.0f} lpm",
                ha="center", color="#1d4ed8", fontsize=8)
    a1.set_xlabel("tiempo [s]"); a1.set_ylabel("mV"); a1.set_ylim(ym[m].min() - 0.2, ym[m].max() + 0.4)
    a1.set_title("Registro real (microSD, 250 Hz, ECG filtrado) — 4 s de señal limpia", fontsize=9.5, fontweight="bold")

    W0, W1 = int(0.40 * FS), int(0.60 * FS)
    segs = np.array([ym[r - W0:r + W1] for r in r_val if r - W0 > 0 and r + W1 < len(ym)])
    prom = segs.mean(axis=0); tb = np.arange(-W0, W1) / FS * 1000
    for s in segs:
        a2.plot(tb, s, color="#86efac", lw=0.6)
    a2.plot(tb, prom, color="#166534", lw=1.8, label=f"promedio de {len(segs)} latidos")

    def marcar(lo, hi, fn, nombre, dy):
        i = np.where((tb >= lo) & (tb <= hi))[0]; k = i[fn(prom[i])]
        a2.plot(tb[k], prom[k], "o", color="#b91c1c", ms=4)
        a2.text(tb[k], prom[k] + dy, nombre, ha="center", fontsize=10, fontweight="bold", color="#b91c1c")
    marcar(-300, -80, np.argmax, "P", 0.06); marcar(-60, -5, np.argmin, "Q", -0.14); marcar(-5, 5, np.argmax, "R", 0.05)
    marcar(5, 70, np.argmin, "S", -0.14); marcar(150, 450, np.argmax, "T", 0.06)
    a2.axvspan(-45, 45, color="#fde68a", alpha=0.4)
    a2.text(0, prom.min() - 0.12, "complejo QRS\n(despolarización ventricular)", ha="center", va="top", fontsize=7.5)
    a2.set_xlabel("tiempo respecto de la onda R [ms]"); a2.set_ylabel("mV"); a2.legend(loc="upper right", fontsize=7.5)
    a2.set_ylim(prom.min() - 0.4, prom.max() + 0.25)
    a2.set_title("Ondas del latido: P (aurículas), QRS (ventrículos), T (repolarización)", fontsize=9.5, fontweight="bold")
    fig.tight_layout()
    return fig


def etapas_pan_tompkins(ym, det, res, r_val, ini):
    """Figura 11: las 4 etapas del detector sobre 5 s de señal."""
    a0, a1 = int(ini * FS), int((ini + 5) * FS); idx = np.arange(a0, a1); tt = idx / FS
    d = np.zeros(len(det)); d[4:] = (2 * det[4:] + det[3:-1] - det[1:-3] - 2 * det[:-4]) / 8.0
    fig, ax = plt.subplots(4, 1, figsize=(7.2, 7.0), sharex=True)
    ax[0].plot(tt, ym[idx], color="#15803d", lw=0.9); ax[0].set_ylabel("mV")
    ax[0].set_title("1) ECG filtrado (el de la pantalla)", fontsize=9, fontweight="bold")
    ax[1].plot(tt, det[idx] / COUNTS_PER_MV, color="#7c3aed", lw=0.9); ax[1].set_ylabel("mV")
    ax[1].set_title("2) Pasa-banda 5–15 Hz: resalta el QRS, atenúa P y T", fontsize=9, fontweight="bold")
    ax[2].plot(tt, d[idx] ** 2, color="#ea580c", lw=0.9); ax[2].set_ylabel("counts²")
    ax[2].set_title("3) Derivada al cuadrado: pendiente del QRS, siempre positiva", fontsize=9, fontweight="bold")
    ax[3].plot(tt, res["integ"][idx], color="#0f172a", lw=1.1, label="integrador (RMS en 150 ms)")
    ax[3].plot(tt, res["umbral"][idx], color="#dc2626", ls="--", lw=1, label="umbral adaptativo")
    for r in r_val[(r_val >= a0) & (r_val < a1)]:
        for a in ax:
            a.axvline(r / FS, color="#b91c1c", lw=0.6, alpha=0.5)
    ax[3].set_title("4) Integrador y umbral de Hamilton-Tompkins; líneas rojas = ondas R detectadas",
                    fontsize=9, fontweight="bold")
    ax[3].legend(fontsize=7, loc="upper right"); ax[3].set_xlabel("tiempo [s]  (registro real)")
    fig.tight_layout()
    return fig


def plantilla_sqi(ym, r_todos, limpio, ruido):
    """Figura 13: 5 'latidos' superpuestos, su plantilla y la correlación media."""
    def recortes(rs):
        S = []
        for r in rs:
            if r - 25 >= 0 and r + 16 <= len(ym):
                s = ym[r - 25:r + 16]; S.append(s - s.mean())
        return np.array(S)

    fig, ax = plt.subplots(1, 2, figsize=(7.2, 2.8))
    tk = (np.arange(41) - 25) * 1000 / FS
    for a, (lo, hi), titulo, col in [(ax[0], limpio, "Señal limpia", "#15803d"), (ax[1], ruido, "Ruido", "#b91c1c")]:
        rs = r_todos[(r_todos > lo * FS) & (r_todos < hi * FS)][:5]
        S = recortes(rs)
        if len(S) >= 2:
            tp = S.mean(axis=0)
            cc = np.mean([np.dot(q, tp) / (np.linalg.norm(q) * np.linalg.norm(tp) + 1e-12) for q in S])
            for q in S:
                a.plot(tk, q, color=col, alpha=0.45, lw=0.9)
            a.plot(tk, tp, color="k", lw=1.8, label="plantilla (promedio)")
            a.set_title(f"{titulo} ({lo}–{hi} s)\ncorrelación media = {cc:.2f}  →  "
                        f"{'VÁLIDO' if cc > 0.66 else 'RECHAZADO'}", fontsize=9, fontweight="bold")
            a.legend(fontsize=7)
        else:
            a.set_title(f"{titulo} ({lo}–{hi} s): el detector no aceptó latidos", fontsize=9)
        a.set_xlabel("ms respecto de la R")
    ax[0].set_ylabel("mV")
    fig.tight_layout()
    return fig


def registro_completo(t, ym, bpm_ant, bpm_nuevo, sombrear):
    """Figura 14: registro completo y FC mostrada por cada detector."""
    fig, ax = plt.subplots(3, 1, figsize=(7.4, 6.4), sharex=True, gridspec_kw={"height_ratios": [1.3, 1, 1]})
    if sombrear:
        for a in ax:
            for lo, hi, k in TRAMOS:
                a.axvspan(lo, hi, color=COLOR_TRAMO[k], alpha=0.8, lw=0)
    ax[0].plot(t, ym, color="#0f172a", lw=0.3); ax[0].set_ylim(-3, 3); ax[0].set_ylabel("mV")
    ax[0].set_title("Registro completo" + (": verde = ECG limpio, gris = sin ECG (contacto), rojo = artefactos"
                                           if sombrear else ""), fontsize=9, fontweight="bold")
    for a, b, col, titulo in [(ax[1], bpm_ant, "#b91c1c", "Detector anterior: siempre muestra una FC (incluso con ruido)"),
                              (ax[2], bpm_nuevo, "#15803d", "Detector actual con SQI: FC solo con señal limpia (el resto muestra ---)")]:
        v = b.astype(float).copy(); v[v <= 0] = np.nan
        a.plot(t, v, color=col, lw=1); a.set_ylim(30, 240); a.set_ylabel("FC [lpm]")
        a.set_title(titulo, fontsize=9, fontweight="bold")
    ax[2].set_xlabel("tiempo [s]")
    fig.tight_layout()
    return fig


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--csv", help="registro CSV de la SD (por defecto datos/ECG_LOG.CSV)")
    ap.add_argument("--limpio", nargs=2, type=float, default=[62, 78], metavar=("INI", "FIN"))
    ap.add_argument("--ruido", nargs=2, type=float, default=[106, 128], metavar=("INI", "FIN"))
    ap.add_argument("--mostrar", action="store_true", help="abrir las figuras en pantalla")
    args = ap.parse_args()
    if not args.mostrar:
        matplotlib.use("Agg")
    asegurar_carpetas()

    t, ecg = cargar_registro(args.csv)
    det = filtrar_pasabanda(ecg)
    print("Corriendo el detector de referencia (tarda unos segundos)...")
    res = detectar(det, ecg)
    r_todos = np.array([l["r"] for l in res["latidos"]], dtype=int)
    r_val = np.array([l["r"] for l in res["latidos"] if l["valido"]], dtype=int)

    lo, hi = args.limpio
    r_limpio = r_val[(r_val > lo * FS) & (r_val < hi * FS)]
    if len(r_limpio) < 3:
        raise SystemExit(f"No hay latidos válidos entre {lo} y {hi} s: elegir otro tramo con --limpio")
    # Polaridad: la R debe apuntar hacia arriba, como en la pantalla.
    signo = -1.0 if np.median(ecg[r_limpio]) < 0 else 1.0
    ym = signo * ecg / COUNTS_PER_MV

    figs = [("fig01_ecg_ondas.png", ecg_con_ondas(ym, t, r_limpio, r_limpio[0] / FS - 0.5)),
            ("fig11_etapas_pan_tompkins.png", etapas_pan_tompkins(ym, det, res, r_val, r_limpio[0] / FS - 0.5)),
            ("fig13_plantilla_sqi.png", plantilla_sqi(ym, r_todos, (int(lo), int(hi)), tuple(int(v) for v in args.ruido)))]
    print("Corriendo el detector anterior para comparar...")
    figs.append(("fig14_registro_completo.png",
                 registro_completo(t, ym, detector_anterior(det, ecg), res["bpm"], sombrear=args.csv is None)))
    for nombre, fig in figs:
        fig.savefig(os.path.join(DIR_FIGURAS, nombre), dpi=200, bbox_inches="tight")
        print("  figuras/" + nombre)
    if args.mostrar:
        plt.show()


if __name__ == "__main__":
    main()
