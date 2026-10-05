"""
analizar_registro.py — Análisis de una sesión nueva grabada en la microSD.
==========================================================================

Pensado para después de cada prueba con la placa: toma un ECG_nnnn.CSV de la
tarjeta y responde tres preguntas:

  1. ¿Cómo fue la señal?   % del tiempo con electrodo suelto, con ruido y
                           con señal limpia (columna sqi del equipo).
  2. ¿Qué midió el equipo? FC, R-R, QRS y onda R (mediana, mínimo y máximo)
                           solo en los tramos con señal limpia.
  3. ¿La placa calculó bien? Corre en la PC el mismo detector (detector_ref.py)
                           sobre el ECG grabado y compara su FC con la que el
                           equipo guardó en la columna bpm.

Genera:
  figuras/sesion_<nombre>.png      ECG completo + FC del equipo y de la PC + calidad
  resultados/sesion_<nombre>.txt   el resumen que se imprime en pantalla

Uso:
    python analizar_registro.py D:/ECG_0001.CSV
    python analizar_registro.py D:/ECG_0001.CSV --mostrar
    python analizar_registro.py D:/ECG_0001.CSV --desde 30 --hasta 40   (zoom del ECG)

Necesita el formato nuevo del CSV (columnas muestra,ecg,bpm,rr_ms,qrs_ms,r_mv,sqi),
que es el que graba el firmware actual.

Cómo leer la comparación con la PC: el equipo escribe en cada fila la FC
vigente en el momento de grabar el bloque (hasta ~0,4 s después de la muestra)
y la PC parte del ECG ya redondeado a enteros, así que pueden diferir algunas
décimas de segundo en los cambios. Se compara segundo a segundo: más de 90 %
de coincidencia (±2 lpm) indica que el firmware hace lo mismo que el modelo.
Pueden aparecer diferencias de fracciones de segundo durante tramos con ruido:
la placa usa además datos que no quedan en el CSV (la saturación del ADC en la
señal sin filtrar y los valores sin redondear).
"""
import argparse, os
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
from ecg_comun import FS, COUNTS_PER_MV, DIR_FIGURAS, DIR_RESULTADOS, asegurar_carpetas, filtrar_pasabanda
from detector_ref import detectar

plt.rcParams.update({"font.size": 8.5, "axes.grid": True, "grid.alpha": 0.3})


def leer(ruta):
    d = np.genfromtxt(ruta, delimiter=",", names=True)
    falta = [c for c in ("ecg", "bpm", "rr_ms", "qrs_ms", "r_mv", "sqi") if c not in d.dtype.names]
    if falta:
        raise SystemExit(f"El CSV no tiene las columnas {falta}: ¿es de una versión anterior del firmware?")
    return {k: d[k].astype(float) for k in d.dtype.names}


def tramos_cero(ecg, minimo=FS):
    """Muestras con electrodo suelto: el equipo graba 0 exacto durante LEAD OFF."""
    cero = ecg == 0
    suelto = np.zeros_like(cero)
    i = 0
    while i < len(cero):
        if cero[i]:
            j = i
            while j < len(cero) and cero[j]:
                j += 1
            if j - i >= minimo / 10:          # al menos 100 ms seguidos en 0
                suelto[i:j] = True
            i = j
        else:
            i += 1
    return suelto


def detectar_como_la_placa(ecg, suelto):
    """Corre el detector reiniciándolo después de cada LEAD OFF, como el firmware.

    Con un electrodo suelto la placa pone los filtros y el detector a cero
    (ECG_DSP_ResetFilters) y vuelve a aprender durante 2 s al reconectar. Para
    imitarlo, se procesa por separado cada tramo con los electrodos conectados.
    """
    n = len(ecg)
    bpm = np.zeros(n); ok = np.zeros(n, dtype=int); latidos = []
    i = 0
    while i < n:
        if suelto[i]:
            i += 1
            continue
        j = i
        while j < n and not suelto[j]:
            j += 1
        tramo = ecg[i:j]
        r = detectar(filtrar_pasabanda(tramo), tramo)
        bpm[i:j] = r["bpm"]; ok[i:j] = r["ok"]
        latidos += [dict(l, r=l["r"] + i) for l in r["latidos"]]
        i = j
    return {"bpm": bpm, "ok": ok, "latidos": latidos}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", help="archivo ECG_nnnn.CSV de la microSD")
    ap.add_argument("--desde", type=float, help="inicio del zoom del ECG [s]")
    ap.add_argument("--hasta", type=float, help="fin del zoom del ECG [s]")
    ap.add_argument("--mostrar", action="store_true", help="abrir la figura en pantalla")
    args = ap.parse_args()
    if not args.mostrar:
        matplotlib.use("Agg")
    asegurar_carpetas()
    nombre = os.path.splitext(os.path.basename(args.csv))[0]

    d = leer(args.csv)
    ecg = d["ecg"]; n = len(ecg); t = np.arange(n) / FS
    suelto = tramos_cero(ecg)
    ok = (d["sqi"] > 0) & ~suelto
    ruido = ~ok & ~suelto

    print("Corriendo el detector de la PC sobre el ECG grabado...")
    pc = detectar_como_la_placa(ecg, suelto)

    L = []
    def p(s=""):
        print(s); L.append(s)

    p(f"Sesión {nombre}: {n / FS:.1f} s ({n / FS / 60:.1f} min)")
    p("")
    p("1) Calidad de la señal (según el equipo)")
    p(f"   electrodo suelto (LEAD OFF): {100 * suelto.mean():5.1f} % del tiempo")
    p(f"   ruido / sin señal útil     : {100 * ruido.mean():5.1f} %")
    p(f"   señal limpia (OK)          : {100 * ok.mean():5.1f} %")
    p("")
    p("2) Mediciones del equipo durante la señal limpia")
    if ok.sum() > FS:
        for col, nom, unidad, ref in [("bpm", "FC", "lpm", "60 a 100"), ("rr_ms", "R-R", "ms", "600 a 1000"),
                                      ("qrs_ms", "QRS", "ms", "< 120"), ("r_mv", "Onda R", "mV", "0,5 a 2,0")]:
            v = d[col][ok]; v = v[v > 0]
            if len(v):
                p(f"   {nom:7s}: mediana {np.median(v):7.2f} {unidad:3s} | mín {v.min():7.2f} | máx {v.max():7.2f}"
                  f"   (referencia {ref})")
    else:
        p("   No hubo tramos con señal limpia: revisar electrodos, gel y que la persona esté quieta.")
    p("")
    p("3) Comparación placa vs. PC (FC segundo a segundo)")
    seg = int(n // FS)
    eq = np.array([np.median(d["bpm"][int(k * FS):int((k + 1) * FS)]) for k in range(seg)])
    pcs = np.array([np.median(pc["bpm"][int(k * FS):int((k + 1) * FS)]) for k in range(seg)])
    mismo_estado = np.mean((eq > 0) == (pcs > 0))
    ambos = (eq > 0) & (pcs > 0)
    p(f"   coinciden en mostrar / no mostrar FC: {100 * mismo_estado:5.1f} % de los segundos")
    if ambos.any():
        p(f"   cuando ambos muestran FC, difieren ≤ 2 lpm el {100 * np.mean(np.abs(eq[ambos] - pcs[ambos]) <= 2):5.1f} %")
    p("   (más de 90 % = el firmware hace lo mismo que el modelo; ver la explicación al principio del script)")

    # ---- figura ----
    zoom = args.desde is not None and args.hasta is not None
    fig, ax = plt.subplots(4 if zoom else 3, 1, figsize=(7.6, 8.2 if zoom else 6.4),
                           gridspec_kw={"height_ratios": ([1.2, 1.2, 1, 0.5] if zoom else [1.2, 1, 0.5])})
    a = 0
    if zoom:
        m = (t >= args.desde) & (t < args.hasta)
        ax[0].plot(t[m], ecg[m] / COUNTS_PER_MV, color="#15803d", lw=0.9)
        for r in [l["r"] for l in pc["latidos"] if l["valido"] and args.desde * FS <= l["r"] < args.hasta * FS]:
            ax[0].axvline(r / FS, color="#b91c1c", lw=0.6, alpha=0.6)
        ax[0].set_title(f"Zoom {args.desde:g}–{args.hasta:g} s (líneas rojas: R detectadas por la PC)", fontsize=9, fontweight="bold")
        ax[0].set_ylabel("mV"); a = 1
    ax[a].plot(t, ecg / COUNTS_PER_MV, color="#0f172a", lw=0.3); ax[a].set_ylim(-3, 3); ax[a].set_ylabel("mV")
    ax[a].set_title(f"Sesión {nombre}: ECG filtrado grabado por el equipo", fontsize=9, fontweight="bold")
    v1 = d["bpm"].copy(); v1[v1 <= 0] = np.nan
    v2 = pc["bpm"].astype(float).copy(); v2[v2 <= 0] = np.nan
    ax[a + 1].plot(t, v1, color="#1d4ed8", lw=2.2, alpha=0.6, label="equipo (columna bpm)")
    ax[a + 1].plot(t, v2, color="#ea580c", lw=0.9, label="PC (detector_ref.py)")
    ax[a + 1].set_ylabel("FC [lpm]"); ax[a + 1].legend(fontsize=7, loc="upper right")
    ax[a + 1].set_title("Frecuencia cardíaca mostrada (vacío = la pantalla muestra ---)", fontsize=9, fontweight="bold")
    estado = np.where(suelto, 0, np.where(ok, 2, 1))
    ax[a + 2].imshow(estado[np.newaxis, :], aspect="auto", cmap=matplotlib.colors.ListedColormap(["#ef4444", "#facc15", "#22c55e"]),
                     vmin=0, vmax=2, extent=[0, t[-1], 0, 1], interpolation="nearest")
    ax[a + 2].set_yticks([]); ax[a + 2].grid(False); ax[a + 2].set_xlabel("tiempo [s]")
    ax[a + 2].set_title("Estado: rojo = LEAD OFF, amarillo = RUIDO, verde = OK", fontsize=9, fontweight="bold")
    for k in range(a, a + 2):
        ax[k].sharex(ax[a + 2])
    fig.tight_layout()
    out_fig = os.path.join(DIR_FIGURAS, f"sesion_{nombre}.png")
    fig.savefig(out_fig, dpi=200, bbox_inches="tight")
    with open(os.path.join(DIR_RESULTADOS, f"sesion_{nombre}.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")
    print(f"\n-> figuras/sesion_{nombre}.png  y  resultados/sesion_{nombre}.txt")
    if args.mostrar:
        plt.show()


if __name__ == "__main__":
    main()
