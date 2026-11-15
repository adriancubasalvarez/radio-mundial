"""Analiza una grabación de la sesión de medida y saca la respuesta del altavoz.

Uso:
  python analizar.py GRABACION.wav --nombre jvc_cerca_I --tipo cerca
  python analizar.py GRABACION.wav --nombre jvc_50cm_I  --tipo lejos [--ventana 5]
  python analizar.py GRABACION.wav --nombre jvc_sala_I  --tipo sala

Tipos de medida:
  cerca  micrófono a ~1 cm del centro del cono (campo cercano). Da el grave
         sin la influencia de la habitación; válida hasta ~1 kHz. De aquí
         salen la frecuencia de corte, el Qtc de la caja y la distorsión.
  lejos  micrófono a 50 cm en el eje. Se recorta la respuesta al impulso
         antes del primer rebote (--ventana, en ms) para quitar la sala:
         válida desde ~1000/ventana Hz hacia arriba (medios y agudos).
  sala   como «lejos» pero sin recortar: lo que se oye en esa habitación.

Resultados en resultados/<nombre>/:
  informe.pdf      resumen y gráficos
  respuesta.png    distorsion.png   impulso.png
  respuesta.csv    frecuencia y nivel (suavizado a 1/6 de octava)
  distorsion.csv   THD, 2º y 3er armónico según la frecuencia (tipo cerca)
  tonos.csv        nivel, THD y THD+N de los tonos fijos de 30 a 190 Hz
  resumen.json     los números principales

Solo necesita numpy y matplotlib. El micrófono no está calibrado: los
niveles son relativos (0 dB = nivel medio de la zona plana).
"""
import argparse
import datetime
import json
import pathlib
import sys

import numpy as np

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages

from comun import (AMPLITUD, CARPETA, F1, F2, FS, HUECO_TONO, L_BARRIDO, N_BARRIDOS,
                   PERIODO_BARRIDO, T_TONO, TONOS, barrido, inicio_tonos, leer_wav)

NFFT = 1 << 18                       # resolución de 0,18 Hz
F_SALIDA = 20.0 * 2 ** (np.arange(0, 480) / 48)   # 20 Hz .. 20 kHz, 1/48 de octava
ARMONICOS = (2, 3, 4, 5)


# ----------------------------------------------------------------- utilidades

def suavizar(f_lin, p_lin, f_out, fraccion=6):
    """Media de potencia en bandas de 1/fraccion de octava centradas en f_out."""
    acum = np.concatenate([[0.0], np.cumsum(p_lin)])
    lo = np.searchsorted(f_lin, f_out * 2 ** (-0.5 / fraccion))
    hi = np.searchsorted(f_lin, f_out * 2 ** (0.5 / fraccion))
    lo = np.minimum(lo, len(p_lin) - 1)
    hi = np.minimum(np.maximum(hi, lo + 1), len(p_lin))
    return (acum[hi] - acum[lo]) / (hi - lo)


def db(p):
    return 10 * np.log10(np.maximum(p, 1e-30))


def ventana(n_pre, n_post, caida=0.5):
    """Media Hann de subida en n_pre muestras, plana y media Hann de bajada
    en la última fracción «caida» de n_post."""
    sube = 0.5 - 0.5 * np.cos(np.pi * np.arange(n_pre) / max(n_pre, 1))
    n_baja = max(int(n_post * caida), 1)
    baja = 0.5 + 0.5 * np.cos(np.pi * np.arange(n_baja) / n_baja)
    return np.concatenate([sube, np.ones(n_post - n_baja), baja])


def trozo(h, centro, n_pre, n_post, caida=0.5):
    ini = centro - n_pre
    seg = np.zeros(n_pre + n_post)
    a, b = max(ini, 0), min(ini + n_pre + n_post, len(h))
    if b > a:
        seg[a - ini:b - ini] = h[a:b]
    return seg * ventana(n_pre, n_post, caida)


def espectro(seg):
    return np.abs(np.fft.rfft(seg, NFFT)) ** 2


F_LIN = np.fft.rfftfreq(NFFT, 1 / FS)


def modelo_paso_alto(f, fc, q):
    """Magnitud en dB de un paso alto de 2º orden (caja cerrada)."""
    x = f / fc
    return 20 * np.log10(x ** 2 / np.sqrt((1 - x ** 2) ** 2 + (x / q) ** 2))


def ajustar_caja(f, nivel, f_lo, f_hi):
    """Busca fc y Qtc del paso alto de 2º orden que mejor encaja con la curva."""
    m = (f >= f_lo) & (f <= f_hi)
    fm, nm = f[m], nivel[m]
    fcs = np.geomspace(25, 300, 400)
    qs = np.linspace(0.3, 5.0, 471)
    x = fm[None, None, :] / fcs[:, None, None]
    mod = 20 * np.log10(x ** 2 / np.sqrt((1 - x ** 2) ** 2 + (x / qs[None, :, None]) ** 2))
    g = np.mean(nm[None, None, :] - mod, axis=2)
    err = np.sqrt(np.mean((nm[None, None, :] - mod - g[:, :, None]) ** 2, axis=2))
    i, j = np.unravel_index(np.argmin(err), err.shape)
    return dict(fc=float(fcs[i]), qtc=float(qs[j]), ganancia=float(g[i, j]),
                error_db=float(err[i, j]), f_lo=f_lo, f_hi=f_hi)


def cruce_por_debajo(f, nivel, umbral, f_desde):
    """Primera frecuencia, bajando desde f_desde, en la que la curva cae por
    debajo de «umbral» (interpolada). None si no cae."""
    idx = np.where(f <= f_desde)[0][::-1]
    for a, b in zip(idx[:-1], idx[1:]):          # a: más aguda, b: más grave
        if nivel[a] >= umbral > nivel[b]:
            t = (nivel[a] - umbral) / (nivel[a] - nivel[b])
            return float(f[a] * (f[b] / f[a]) ** t)
    return None


# ----------------------------------------------------------------- análisis

def remuestrear(y, m):
    """Cambia el número de muestras de y a m en el dominio de la frecuencia
    (sin la pérdida de agudos de una interpolación lineal)."""
    n = len(y)
    Y = np.fft.rfft(y)
    k = min(len(Y), m // 2 + 1)
    return np.fft.irfft(Y[:k], m) * (m / n)


def cargar(ruta, canal):
    x, fs = leer_wav(ruta)
    if canal is None:
        canal = int(np.argmax(np.sum(x ** 2, axis=0)))
    y = x[:, canal]
    avisos = []
    if fs != FS:
        avisos.append(f"La grabación está a {fs} Hz; se ha convertido a {FS} Hz.")
        y = remuestrear(y, int(round(len(y) * FS / fs)))
    pico = float(np.max(np.abs(y)))
    saturadas = int(np.sum(np.abs(y) >= 0.98))
    if saturadas > 10:
        avisos.append(f"SATURACIÓN: {saturadas} muestras tocan el máximo. Baja el volumen "
                      "o aleja el micrófono y repite la medida.")
    elif pico < 0.01:
        avisos.append("Nivel de grabación muy bajo (pico < -40 dBFS): sube el volumen.")
    return y, fs, canal, pico, avisos


def deconvolucionar(y):
    """Respuesta al impulso de toda la grabación frente al barrido (división
    espectral regularizada). Los barridos aparecen como picos y la distorsión
    armónica como picos más pequeños justo antes de cada uno."""
    ref = AMPLITUD * barrido()
    n = 1 << int(np.ceil(np.log2(len(y) + len(ref))))
    X = np.fft.rfft(ref, n)
    Y = np.fft.rfft(y, n)
    f = np.fft.rfftfreq(n, 1 / FS)
    p = np.abs(X) ** 2
    banda = (f >= F1 * 1.2) & (f <= min(F2, FS / 2) * 0.97)
    eps = np.where(banda, 1e-6, 1e3) * np.max(p[banda])
    return np.fft.irfft(Y * np.conj(X) / (p + eps), n)


def buscar_barridos(h):
    """Posición de los picos de los barridos y relación de relojes."""
    env = np.abs(h)
    p = int(np.argmax(env))
    paso = PERIODO_BARRIDO * FS
    margen = int(0.2 * FS)
    candidatos = []
    for k in range(-(N_BARRIDOS - 1), N_BARRIDOS):
        c = int(round(p + k * paso))
        a, b = max(c - margen, 0), min(c + margen, len(h))
        if b - a < margen:
            continue
        q = a + int(np.argmax(env[a:b]))
        if env[q] > 0.3 * env[p]:
            candidatos.append(q)
    candidatos = sorted(set(candidatos))
    # Nos quedamos con la serie de N_BARRIDOS consecutivos que contenga p
    picos = [q for q in candidatos if abs(q - p) <= (N_BARRIDOS - 1) * paso + margen]
    picos = picos[:N_BARRIDOS]
    relacion = 1.0
    if len(picos) >= 2:
        relacion = (picos[-1] - picos[0]) / ((len(picos) - 1) * paso)
    return picos, relacion


def espectro_medio(h, centros, n_pre, n_post, caida=0.5):
    """Media de la potencia de los barridos. Se promedia la potencia y no la
    forma de onda porque los picos no caen en muestras exactas (los relojes
    no coinciden) y promediar ondas desfasadas se comería los agudos."""
    return np.mean([espectro(trozo(h, c, n_pre, n_post, caida)) for c in centros], axis=0)


def analizar_tonos(y, inicio, relacion):
    filas = []
    for j, f in enumerate(TONOS):
        t0 = inicio + relacion * (inicio_tonos() + j * (T_TONO + HUECO_TONO)) * FS
        a = int(t0 + 0.3 * FS)
        n = int(1.0 * FS)
        if a + n > len(y) or a < 0:
            break
        seg = y[a:a + n] * np.hanning(n)
        nf = 4 * FS
        P = np.abs(np.fft.rfft(seg, nf)) ** 2
        fr = np.fft.rfftfreq(nf, 1 / FS)
        f_real = f / relacion
        m = (fr > f_real * 0.97) & (fr < f_real * 1.03)
        f0 = fr[m][np.argmax(P[m])]

        def potencia(fx, ancho=3.0):
            mm = (fr > fx - ancho) & (fr < fx + ancho)
            return float(np.sum(P[mm]))

        p1 = potencia(f0)
        armonicos = [potencia(k * f0) for k in ARMONICOS]
        util = (fr >= 15) & (fr <= 20000)
        total = float(np.sum(P[util]))
        filas.append(dict(f=f, nivel_db=db(p1), thd=100 * np.sqrt(sum(armonicos) / p1),
                          thd_n=100 * np.sqrt(max(total - p1, 0) / p1),
                          h2_db=db(armonicos[0] / p1), h3_db=db(armonicos[1] / p1)))
    return filas


def analizar(ruta, nombre, tipo, ventana_ms, canal, f_max_cerca):
    y, fs_orig, canal, pico, avisos = cargar(ruta, canal)
    h = deconvolucionar(y)
    picos, relacion = buscar_barridos(h)
    if not picos:
        raise SystemExit("No encuentro los barridos en la grabación.")
    if abs(relacion - 1) > 5e-6:
        # Los relojes del ESP32 y de la grabadora no van exactamente igual:
        # se corrige la velocidad de la grabación para no perder agudos.
        ppm_medida = (relacion - 1) * 1e6
        y = remuestrear(y, int(round(len(y) / relacion)))
        h = deconvolucionar(y)
        picos, relacion = buscar_barridos(h)
        relacion_total = (1 + ppm_medida * 1e-6) * relacion
    else:
        relacion_total = relacion
    if len(picos) < N_BARRIDOS:
        avisos.append(f"Solo se han encontrado {len(picos)} de {N_BARRIDOS} barridos.")
    ppm = (relacion_total - 1) * 1e6

    # Respuesta en frecuencia
    if tipo == "cerca":
        n_pre, n_post, caida = int(0.005 * FS), int(0.15 * FS), 0.5
        f_min, f_max = 20.0, f_max_cerca
    elif tipo == "lejos":
        n_pre, n_post, caida = int(0.001 * FS), int(ventana_ms / 1000 * FS), 0.3
        f_min, f_max = 1000.0 / ventana_ms, 20000.0
    else:
        n_pre, n_post, caida = int(0.005 * FS), int(0.3 * FS), 0.5
        f_min, f_max = 20.0, 20000.0
    p1 = espectro_medio(h, picos, n_pre, n_post, caida)
    nivel_abs = db(suavizar(F_LIN, p1, F_SALIDA))

    # Ruido: un tramo de la respuesta lejos de barridos y armónicos
    n_ruido = n_pre + n_post
    c_ruido = picos[0] + int(1.0 * FS)
    ruido = db(suavizar(F_LIN, espectro(trozo(h, c_ruido, n_pre, n_post, caida)), F_SALIDA))
    banda_snr = (F_SALIDA >= max(f_min, 100)) & (F_SALIDA <= min(f_max, 5000))
    snr = float(np.median(nivel_abs[banda_snr] - ruido[banda_snr]))
    if snr < 30:
        avisos.append(f"Relación señal/ruido baja ({snr:.0f} dB): sube el volumen o "
                      "reduce el ruido de la habitación.")

    # Referencia de 0 dB y ajuste de la caja
    caja = None
    valida = (F_SALIDA >= f_min) & (F_SALIDA <= f_max)
    if tipo == "cerca":
        caja = ajustar_caja(F_SALIDA, nivel_abs, 50.0, min(600.0, f_max))
        # 0 dB = nivel medio entre 250 y 600 Hz (la zona plana real, por
        # encima de la resonancia); el modelo se dibuja respecto a ella.
        zona = (F_SALIDA >= 250) & (F_SALIDA <= min(600.0, f_max))
        ref = float(np.mean(nivel_abs[zona]))
        caja["desplaz_db"] = caja["ganancia"] - ref
    else:
        zona = valida & (F_SALIDA >= max(300, f_min)) & (F_SALIDA <= 3000)
        ref = float(np.mean(nivel_abs[zona])) if np.any(zona) else float(np.max(nivel_abs))
    nivel = nivel_abs - ref
    ruido_rel = ruido - ref

    joroba = None
    if caja:
        m = (F_SALIDA >= caja["fc"] / 2) & (F_SALIDA <= caja["fc"] * 2) & valida
        if np.any(m):
            i = int(np.argmax(np.where(m, nivel, -99)))
            if nivel[i] > 1.5:
                joroba = dict(f=float(F_SALIDA[i]), db=float(nivel[i]))

    cortes = {}
    if tipo in ("cerca", "sala"):
        desde = 600.0 if tipo == "cerca" else 300.0
        for d in (3, 6, 10):
            cortes[f"F{d}"] = cruce_por_debajo(F_SALIDA, nivel, -d, desde)

    # Distorsión con el barrido (solo tiene sentido en campo cercano)
    dist = None
    if tipo == "cerca":
        n_pre_h, n_post_h = int(0.005 * FS), int(0.05 * FS)
        H1 = suavizar(F_LIN, espectro_medio(h, picos, n_pre_h, n_post_h), F_SALIDA)
        harm = {}
        for k in ARMONICOS:
            centros = [c - int(round(L_BARRIDO * np.log(k) * FS)) for c in picos]
            pk = espectro_medio(h, centros, n_pre_h, n_post_h)
            harm[k] = suavizar(F_LIN, pk, F_SALIDA * k)
            harm[k][F_SALIDA * k > 20000] = 0.0
        pr = suavizar(F_LIN, espectro(trozo(h, c_ruido, n_pre_h, n_post_h)), F_SALIDA * 2)
        suma = sum(harm.values())
        dist = dict(thd=100 * np.sqrt(suma / H1), h2=db(harm[2] / H1), h3=db(harm[3] / H1),
                    limite=100 * np.sqrt(pr / H1))

    # Tonos fijos
    # El pico de la respuesta al impulso marca dónde empieza el 1er barrido
    # en la grabación (más el retraso del sonido hasta el micrófono).
    tonos = analizar_tonos(y, picos[0], relacion)
    # Pasar el nivel de los tonos a la escala de la curva con la diferencia
    # mediana por encima de 100 Hz: sirven para comprobar la forma del grave.
    if tonos:
        ft = np.array([t["f"] for t in tonos])
        nt = np.array([t["nivel_db"] for t in tonos])
        curva = np.interp(ft, F_SALIDA, nivel)
        m = ft >= 100
        desplaz = float(np.median(curva[m] - nt[m])) if np.any(m) else 0.0
        for t in tonos:
            t["nivel_db"] += desplaz

    return dict(nombre=nombre, tipo=tipo, archivo=str(ruta), canal=canal, fs=fs_orig,
                pico_dbfs=float(db(pico ** 2)), picos=picos, deriva_ppm=float(ppm),
                snr_db=snr, f_min=f_min, f_max=f_max, ventana_ms=n_post / FS * 1000,
                caja=caja, joroba=joroba, cortes=cortes, avisos=avisos, nivel=nivel, ruido=ruido_rel,
                h=h, n_pre=n_pre, dist=dist, tonos=tonos)


# ----------------------------------------------------------------- informe

def propuesta_eq(caja):
    """Transformada de Linkwitz: llevar la caja (fc, Qtc) a Q = 0,707 con una
    frecuencia más baja. Cuánto refuerzo hace falta en el grave."""
    filas = []
    for refuerzo in (3, 6, 9, 12):
        fp = caja["fc"] / 10 ** (refuerzo / 40)
        filas.append(dict(refuerzo_db=refuerzo, f3_nueva=fp, potencia_x=10 ** (refuerzo / 10)))
    return filas


def texto_resumen(r):
    lin = [f"Medida: {r['nombre']}   (tipo {r['tipo']})",
           f"Archivo: {pathlib.Path(r['archivo']).name}   canal {r['canal']}   {r['fs']} Hz",
           f"Fecha del análisis: {datetime.date.today().isoformat()}",
           "",
           f"Pico de la grabación: {r['pico_dbfs']:.1f} dBFS",
           f"Relación señal/ruido (mediana): {r['snr_db']:.0f} dB",
           f"Barridos encontrados: {len(r['picos'])}   diferencia de relojes: {r['deriva_ppm']:+.0f} ppm",
           f"Curva válida de {r['f_min']:.0f} Hz a {r['f_max']:.0f} Hz "
           f"(ventana de {r['ventana_ms']:.1f} ms)", ""]
    c = r["caja"]
    if c:
        lin += ["Ajuste a una caja cerrada (paso alto de 2º orden), "
                f"entre {c['f_lo']:.0f} y {c['f_hi']:.0f} Hz:",
                f"   frecuencia de resonancia fc = {c['fc']:.0f} Hz",
                f"   Qtc = {c['qtc']:.2f}   (0,5 seco · 0,707 plano · >1 grave «hinchado»)",
                f"   error del ajuste = {c['error_db']:.1f} dB", ""]
    if r["cortes"]:
        partes = [f"{k} = {v:.0f} Hz" if v else f"{k} = —" for k, v in r["cortes"].items()]
        lin += ["Caída del grave (respecto a la zona plana): " + "   ".join(partes), ""]
    j = r.get("joroba")
    if j:
        lin += [f"Joroba en el grave: +{j['db']:.1f} dB a {j['f']:.0f} Hz sobre la zona plana",
                "   (suena «gordo» o retumbante). Se corrige con un filtro de pico de",
                f"   -{j['db']:.1f} dB a {j['f']:.0f} Hz y Q ≈ {c['qtc']:.1f}, o con la transformada de",
                "   Linkwitz de abajo, que hace las dos cosas a la vez.", ""]
    if c:
        lin.append("Ecualización posible (transformada de Linkwitz a Q = 0,707):")
        for e in propuesta_eq(c):
            lin.append(f"   +{e['refuerzo_db']:>2} dB en el grave → F3 ≈ {e['f3_nueva']:.0f} Hz"
                       f"   (×{e['potencia_x']:.0f} de potencia y más recorrido del cono)")
        lin += ["   Siempre con un paso alto de protección por debajo de la nueva F3.", ""]
    if r["tonos"]:
        peor = max(r["tonos"], key=lambda t: t["thd_n"])
        lin += [f"Tonos fijos: la mayor THD+N es {peor['thd_n']:.1f} % a {peor['f']:.0f} Hz.", ""]
    if r["avisos"]:
        lin += ["AVISOS:"] + [f"   • {a}" for a in r["avisos"]] + [""]
    lin += ["Micrófono sin calibrar (DJI Mic): niveles relativos. Por debajo de ~50 Hz",
            "el propio micrófono pierde nivel, así que el grave más profundo sale algo",
            "peor de lo que es. Las comparaciones entre medidas sí son fiables."]
    return "\n".join(lin)


def figura_respuesta(r):
    fig, ax = plt.subplots(figsize=(10, 5.5))
    valida = (F_SALIDA >= r["f_min"]) & (F_SALIDA <= r["f_max"])
    ax.semilogx(F_SALIDA, r["nivel"], color="#bbb", lw=1, label="fuera de la zona válida")
    ax.semilogx(F_SALIDA[valida], r["nivel"][valida], color="#1f77b4", lw=2,
                label="respuesta (1/6 de octava)")
    ax.semilogx(F_SALIDA, r["ruido"], color="#999", lw=0.8, ls=":", label="ruido de fondo")
    c = r["caja"]
    if c:
        ax.semilogx(F_SALIDA, modelo_paso_alto(F_SALIDA, c["fc"], c["qtc"]) + c["desplaz_db"], "--",
                    color="#d62728", lw=1.2,
                    label=f"caja cerrada fc = {c['fc']:.0f} Hz, Qtc = {c['qtc']:.2f}")
    if r["tonos"]:
        ax.semilogx([t["f"] for t in r["tonos"]], [t["nivel_db"] for t in r["tonos"]], "o",
                    ms=4, color="#2ca02c", label="tonos fijos")
    for i, (k, v) in enumerate(r["cortes"].items()):
        if v:
            ax.axvline(v, color="#ff7f0e", lw=0.8, ls="--")
            ax.text(v, -22 - 3 * i, f" {k} = {v:.0f} Hz", fontsize=8, color="#ff7f0e")
    ax.axhline(0, color="#444", lw=0.6)
    ax.axhline(-3, color="#444", lw=0.4, ls=":")
    ax.set_xlim(20, 20000)
    ax.set_ylim(-30, 15)
    ax.set_xlabel("Frecuencia (Hz)")
    ax.set_ylabel("Nivel relativo (dB)")
    ax.set_title(f"Respuesta en frecuencia — {r['nombre']} ({r['tipo']})")
    ax.grid(True, which="both", alpha=0.3)
    ax.set_xticks([20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000])
    ax.set_xticklabels(["20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k"])
    ax.legend(loc="lower right", fontsize=8)
    fig.tight_layout()
    return fig


def figura_distorsion(r):
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(10, 7.5))
    d = r["dist"]
    if d:
        m = (F_SALIDA >= 20) & (F_SALIDA <= 5000)
        a1.loglog(F_SALIDA[m], d["thd"][m], color="#d62728", lw=2, label="THD (2º a 5º armónico)")
        a1.loglog(F_SALIDA[m], 100 * 10 ** (d["h2"][m] / 20), color="#1f77b4", lw=1, label="2º armónico")
        a1.loglog(F_SALIDA[m], 100 * 10 ** (d["h3"][m] / 20), color="#2ca02c", lw=1, label="3er armónico")
        a1.loglog(F_SALIDA[m], d["limite"][m], color="#999", ls=":", label="límite por ruido")
        a1.set_xlim(20, 5000)
        a1.set_ylim(0.05, 100)
        a1.set_title("Distorsión armónica con el barrido (campo cercano)")
        a1.legend(fontsize=8)
    else:
        a1.text(0.5, 0.5, "La distorsión con el barrido se calcula solo en medidas «cerca».",
                ha="center", va="center", transform=a1.transAxes)
    a1.set_ylabel("%")
    a1.set_xlabel("Frecuencia (Hz)")
    a1.grid(True, which="both", alpha=0.3)
    if r["tonos"]:
        f = [t["f"] for t in r["tonos"]]
        x = np.arange(len(f))
        a2.bar(x - 0.2, [t["thd"] for t in r["tonos"]], 0.4, color="#d62728", label="THD")
        a2.bar(x + 0.2, [t["thd_n"] for t in r["tonos"]], 0.4, color="#ff7f0e", label="THD+N")
        a2.set_xticks(x)
        a2.set_xticklabels([f"{v:.0f}" for v in f], fontsize=8)
        a2.set_xlabel("Frecuencia del tono (Hz)")
        a2.set_ylabel("%")
        a2.set_title("Tonos fijos: distorsión y ruidos (un pico aislado suele ser algo que vibra)")
        a2.legend(fontsize=8)
        a2.grid(True, axis="y", alpha=0.3)
    fig.tight_layout()
    return fig


def figura_impulso(r):
    fig, ax = plt.subplots(figsize=(10, 4.5))
    h, p = r["h"], r["picos"][0]
    a, b = p - int(0.005 * FS), p + int(0.05 * FS)
    seg = h[a:b]
    t = (np.arange(a, b) - p) / FS * 1000
    env = 20 * np.log10(np.abs(seg) / np.max(np.abs(seg)) + 1e-9)
    ax.plot(t, env, color="#1f77b4", lw=0.8)
    if r["tipo"] == "lejos":
        ax.axvline(r["ventana_ms"], color="#d62728", ls="--", label="fin de la ventana")
        ax.legend(fontsize=8)
    ax.set_ylim(-60, 2)
    ax.set_xlabel("Tiempo desde el pico (ms)")
    ax.set_ylabel("dB")
    ax.set_title("Respuesta al impulso: los picos tras el principal son rebotes\n"
                 "(en «lejos», la ventana debe acabar antes del primero)", fontsize=10)
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    return fig


def guardar(r):
    carpeta = CARPETA / "resultados" / r["nombre"]
    carpeta.mkdir(parents=True, exist_ok=True)
    with open(carpeta / "respuesta.csv", "w", encoding="utf-8") as fh:
        fh.write("frecuencia_Hz,nivel_dB,valida\n")
        for f, n in zip(F_SALIDA, r["nivel"]):
            fh.write(f"{f:.2f},{n:.2f},{int(r['f_min'] <= f <= r['f_max'])}\n")
    if r["dist"]:
        d = r["dist"]
        with open(carpeta / "distorsion.csv", "w", encoding="utf-8") as fh:
            fh.write("frecuencia_Hz,THD_pct,H2_dB,H3_dB,limite_ruido_pct\n")
            for i, f in enumerate(F_SALIDA):
                if f <= 5000:
                    fh.write(f"{f:.2f},{d['thd'][i]:.3f},{d['h2'][i]:.1f},{d['h3'][i]:.1f},"
                             f"{d['limite'][i]:.3f}\n")
    if r["tonos"]:
        with open(carpeta / "tonos.csv", "w", encoding="utf-8") as fh:
            fh.write("frecuencia_Hz,nivel_dB,THD_pct,THD_N_pct,H2_dB,H3_dB\n")
            for t in r["tonos"]:
                fh.write(f"{t['f']:.1f},{t['nivel_db']:.2f},{t['thd']:.2f},{t['thd_n']:.2f},"
                         f"{t['h2_db']:.1f},{t['h3_db']:.1f}\n")
    resumen = {k: r[k] for k in ("nombre", "tipo", "archivo", "canal", "fs", "pico_dbfs",
                                 "deriva_ppm", "snr_db", "f_min", "f_max", "ventana_ms",
                                 "caja", "cortes", "avisos")}
    if r["caja"]:
        resumen["ecualizacion"] = propuesta_eq(r["caja"])
        resumen["joroba"] = r.get("joroba")
    (carpeta / "resumen.json").write_text(json.dumps(resumen, indent=2, ensure_ascii=False),
                                          encoding="utf-8")

    figs = [("respuesta", figura_respuesta(r)), ("distorsion", figura_distorsion(r)),
            ("impulso", figura_impulso(r))]
    with PdfPages(carpeta / "informe.pdf") as pdf:
        portada = plt.figure(figsize=(10, 7.5))
        portada.text(0.06, 0.94, "Radio Mundial Boombox — medida acústica", fontsize=16,
                     weight="bold", va="top")
        portada.text(0.06, 0.88, texto_resumen(r), fontsize=10, va="top", family="monospace")
        pdf.savefig(portada)
        plt.close(portada)
        for nombre, fig in figs:
            fig.savefig(carpeta / f"{nombre}.png", dpi=130)
            pdf.savefig(fig)
            plt.close(fig)
    return carpeta


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("grabacion", type=pathlib.Path)
    ap.add_argument("--nombre", required=True, help="nombre de la medida (carpeta de resultados)")
    ap.add_argument("--tipo", choices=("cerca", "lejos", "sala"), required=True)
    ap.add_argument("--ventana", type=float, default=5.0,
                    help="ms de respuesta al impulso que se usan en «lejos» (5 por defecto)")
    ap.add_argument("--canal", type=int, default=None,
                    help="canal de la grabación (por defecto el de más nivel)")
    ap.add_argument("--fmax-cerca", type=float, default=1000.0,
                    help="límite superior de validez del campo cercano (1000 Hz)")
    a = ap.parse_args()
    r = analizar(a.grabacion, a.nombre, a.tipo, a.ventana, a.canal, a.fmax_cerca)
    carpeta = guardar(r)
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")   # consola de Windows
    print(texto_resumen(r))
    print(f"\nResultados en {carpeta}")


if __name__ == "__main__":
    main()
