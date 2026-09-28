"""
detector_ref.py — Detector de QRS + índice de calidad (SQI) en Python.
======================================================================

Es una copia, muestra a muestra, de la lógica de Core/Src/ecg_qrs.c del
firmware, escrita en Python para poder graficar sus señales internas
(integrador, umbral) y ensayar cambios de parámetros sin grabar la placa.
Con el registro real coincide con el código C en más del 99,8 % de las
muestras (lo verifica validacion.py).

También incluye el detector ANTERIOR (sin índice de calidad) para comparar.

Uso:
    from detector_ref import detectar, detector_anterior
    res = detectar(det, ecg)          # det = pasa-banda 5–15 Hz, ecg = ECG filtrado
    res["bpm"]      -> FC publicada en cada muestra (0 = "---")
    res["ok"]       -> 1 si la señal es confiable (SQI) en cada muestra
    res["latidos"]  -> lista de dicts {r, valido, searchback, cc}
    res["integ"], res["umbral"] -> señales internas (para graficar)

También se puede correr solo para ver un resumen:
    python detector_ref.py
"""
import math
import numpy as np
from ecg_comun import FS

# ---- Parámetros (mismos valores que ecg_qrs.c) ----
WIN = 37            # integrador: 150 ms
REFRACT = 50        # período refractario: 200 ms
LEARN = 500         # aprendizaje: 2 s
TWAVE = 90          # ventana de onda T: 360 ms
TH = 0.3125         # Hamilton: ruido + 0,3125 (QRS - ruido)
SB_K = 1.5          # searchback a 1,5 R-R
ARTIF_K = 2.5       # artefacto: pico > 2,5 x mediana de QRS
ART_RELEARN = 2     # 2 artefactos seguidos: re-aprender
LVL_N = 8           # picos para las medianas de nivel
SQI_N = 5           # latidos en la ventana del SQI
SQI_MIN = 4
CC_MIN = 0.66       # Orphanidou: correlación mínima
PRE, POST = 25, 15  # plantilla: 100 ms antes y 60 ms después de la R
HL = 512            # historia: 2 s
SAT_COUNTS = 2500.0 # |ECG| > 1,8 mV = fuera de rango
SAT_HOLD = 250      # 1 s sin aceptar tras saturar
TIMEOUT = 750       # 3 s sin latido válido: FC inválida


def _mediana(v):
    s = sorted(v); n = len(s)
    if n == 0:
        return 0.0
    return s[n // 2] if n & 1 else 0.5 * (s[n // 2 - 1] + s[n // 2])


def detectar(det, ecg, adc_sat=None, vencimiento=True):
    """Corre el detector sobre toda la señal.

    det         : señal de detección (pasa-banda 5–15 Hz), counts.
    ecg         : ECG filtrado de pantalla, counts (para ubicar la R y la plantilla).
    adc_sat     : opcional, arreglo de 0/1 con saturación del ADC por muestra.
    vencimiento : True = como el firmware (la FC vence a los 3 s sin latido
                  válido o con saturación). False = prototipo original.
    """
    N = len(det)
    bpm_out = np.zeros(N); ok_out = np.zeros(N, dtype=int)
    integ_out = np.full(N, np.nan); thr_out = np.full(N, np.nan)
    latidos = []

    x = [0.0] * 5; win = [0] * WIN; wi = 0; ws = 0
    qpk, npk, rr = [], [], []
    n = 0; last = 0; lpk = 0.0; sat_until = -1
    cand = 0; cpk = 0.0; cpos = 0; cs = 0.0; qs = 0.0
    sbpk = 0.0; sbpos = 0; sbs = 0.0
    i1 = i2 = 0.0
    art_cnt = 0; art_pk = []
    hist = [0.0] * HL; hi = 0
    snips = []; cc_win = 0.0
    st = {"bpm": 0, "ok": 0}

    def H(b):                                  # b = 0 es la muestra más reciente
        return hist[(hi + HL - 1 - b) % HL]

    def ubicar_r(lag):
        rb = lag + 15; rmax = 0.0
        for b in range(lag + 15, min(lag + 46, HL - PRE - 1)):
            a = abs(H(b))
            if a > rmax:
                rmax = a; rb = b
        return rb

    def aceptar(i, pos, pk, pend, lag, searchback):
        nonlocal last, qs, cc_win
        r = pos - last; last = pos; qs = pend
        qpk.append(pk); del qpk[:-LVL_N]
        rb = ubicar_r(lag)
        if rb - POST >= 0 and rb + PRE < HL:
            sn = np.array([H(b) for b in range(rb + PRE, rb - POST - 1, -1)])
            snips.append(sn - sn.mean()); del snips[:-SQI_N]
            if len(snips) >= SQI_MIN:
                tp = np.mean(snips, axis=0); c = []
                for q in snips:
                    den = math.sqrt(float(np.dot(q, q)) * float(np.dot(tp, tp))) + 1e-9
                    c.append(float(np.dot(q, tp)) / den)
                cc_win = float(np.mean(c))
        if 50 <= r <= 750:
            rr.append(r); del rr[:-SQI_N]
        ok = 0
        if len(rr) >= SQI_MIN and len(snips) >= SQI_MIN:
            fc = 60 * FS / _mediana(rr)
            ok = int(40 <= fc <= 180 and max(rr) < 750 and max(rr) < 2.2 * min(rr)
                     and cc_win > CC_MIN and n > sat_until)
        st["ok"] = ok
        st["bpm"] = int(60 * FS / _mediana(rr) + 0.5) if ok else 0
        latidos.append({"r": i - rb, "valido": ok, "searchback": searchback, "cc": cc_win})

    for i in range(N):
        hist[hi] = ecg[i]; hi = (hi + 1) % HL
        if abs(ecg[i]) > SAT_COUNTS or (adc_sat is not None and adc_sat[i]):
            sat_until = n + SAT_HOLD
        # derivada de 5 puntos, cuadrado entero, ventana de 150 ms, raíz
        x = [det[i]] + x[:4]
        d = (2 * x[0] + x[1] - x[3] - 2 * x[4]) * 0.125; ad = abs(d)
        q16 = int(min(d * d * 16.0, 1e8) + 0.5)
        ws -= win[wi]; win[wi] = q16; ws += q16; wi = (wi + 1) % WIN
        integ = math.sqrt(ws / (WIN * 16.0)); n += 1
        integ_out[i] = integ
        if vencimiento and st["ok"] and ((n - last) > TIMEOUT or n <= sat_until):
            st["ok"] = 0; st["bpm"] = 0
        is_peak = (i1 > i2) and (integ <= i1); pk_prev = i1
        i2 = i1; i1 = integ

        while True:                            # "bloque" de decisión (break = continue del C)
            if n < LEARN:
                lpk = max(lpk, integ); break
            q = _mediana(qpk) if qpk else lpk
            nz = min(_mediana(npk) if npk else 0.0, 0.5 * q)
            thr = nz + TH * (q - nz)
            thr_out[i] = thr
            if not cand:
                if integ > thr and n - last > REFRACT:
                    cand = 1; cpk = integ; cpos = n; cs = ad; break
                if is_peak and pk_prev <= thr and pk_prev < q:
                    npk.append(pk_prev); del npk[:-LVL_N]
                if n - last > REFRACT:
                    if integ > sbpk: sbpk = integ; sbpos = n
                    if ad > sbs: sbs = ad
                if rr and sbpk > 0.5 * thr and (n - last) > SB_K * _mediana(rr):
                    if sbpk < ARTIF_K * q and (n - sbpos) + 60 < HL:
                        aceptar(i, sbpos, sbpk, sbs, n - sbpos, True)
                    sbpk = 0.0; sbs = 0.0
                break
            if integ > cpk: cpk = integ; cpos = n
            if ad > cs: cs = ad
            if integ > thr and n - cpos < WIN:
                break
            cand = 0
            if n <= sat_until:
                sbpk = 0.0; sbs = 0.0; break
            if qpk and cpk > ARTIF_K * q:
                art_cnt += 1; art_pk.append(cpk); del art_pk[:-ART_RELEARN]
                if art_cnt < ART_RELEARN:
                    sbpk = 0.0; sbs = 0.0; break
                qpk[:] = list(art_pk)
            art_cnt = 0
            if (cpos - last) < TWAVE and qs > 0 and cs < 0.5 * qs:
                if cpk < q:
                    npk.append(cpk); del npk[:-LVL_N]
                break
            sbpk = 0.0; sbs = 0.0
            aceptar(i, cpos, cpk, cs, n - cpos, False)
            break
        bpm_out[i] = st["bpm"]; ok_out[i] = st["ok"]

    return {"bpm": bpm_out, "ok": ok_out, "latidos": latidos, "integ": integ_out, "umbral": thr_out}


def detector_anterior(det, ecg):
    """Detector de la versión anterior del firmware (sin índice de calidad).

    Pan-Tompkins con niveles por promedio exponencial, umbral al 25 %,
    searchback a 1,66 R-R y FC = mediana de 8 R-R. Siempre publica una FC,
    aunque la señal sea ruido: es lo que se corrigió con el SQI.
    Devuelve la FC mostrada en cada muestra.
    """
    x = [0.0] * 5; win = [0] * WIN; wi = 0; ws = 0; spk = npk = 0.0; n = 0; last = 0
    rr = []; med = 0.0; bpm = 0
    cand = 0; cpk = 0.0; cpos = 0; cs = 0.0; qs = 0.0; sbpk = 0.0; sbpos = 0; sbs = 0.0
    out = np.zeros(len(det))

    def aceptar(pos, pk, pend, sb):
        nonlocal last, qs, spk, sbpk, sbs, med, bpm
        r = pos - last; last = pos; qs = pend
        spk = (0.25 * pk + 0.75 * spk) if sb else (0.125 * pk + 0.875 * spk)
        sbpk = 0.0; sbs = 0.0
        if 50 <= r <= 750:
            rr.append(r); del rr[:-8]
            med = _mediana(rr); bpm = int(60 * FS / med + 0.5)

    for i in range(len(det)):
        out[i] = bpm
        x = [det[i]] + x[:4]; d = (2 * x[0] + x[1] - x[3] - 2 * x[4]) * 0.125; ad = abs(d)
        q16 = int(min(d * d * 16.0, 1e8) + 0.5); ws -= win[wi]; win[wi] = q16; ws += q16; wi = (wi + 1) % WIN
        integ = math.sqrt(ws / (WIN * 16.0)); n += 1
        if n < LEARN:
            spk = max(spk, integ); npk = 0.125 * integ + 0.875 * npk; continue
        thr = npk + 0.25 * (spk - npk)
        if not cand:
            if integ > thr and n - last > REFRACT:
                cand = 1; cpk = integ; cpos = n; cs = ad
            else:
                if integ <= thr: npk = 0.125 * integ + 0.875 * npk
                if n - last > REFRACT:
                    if integ > sbpk: sbpk = integ; sbpos = n
                    if ad > sbs: sbs = ad
                if rr and sbpk > 0.5 * thr and (n - last) > 1.66 * med:
                    aceptar(sbpos, sbpk, sbs, True)
            continue
        if integ > cpk: cpk = integ; cpos = n
        if ad > cs: cs = ad
        if integ > thr and n - cpos < WIN: continue
        cand = 0
        if cpos - last < TWAVE and qs > 0 and cs < 0.5 * qs:
            npk = 0.125 * cpk + 0.875 * npk; continue
        aceptar(cpos, cpk, cs, False)
    return out


if __name__ == "__main__":
    from ecg_comun import cargar_registro, filtrar_pasabanda
    t, ecg = cargar_registro()
    res = detectar(filtrar_pasabanda(ecg), ecg)
    val = [l for l in res["latidos"] if l["valido"]]
    print(f"Registro de {t[-1]:.0f} s: {len(res['latidos'])} latidos aceptados, {len(val)} con señal válida.")
    print(f"FC mostrada el {100 * np.mean(res['bpm'] > 0):.1f} % del tiempo.")
