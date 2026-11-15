"""Diseña la corrección DSP de una cápsula a partir de una medida en campo
cercano y predice cómo quedará la respuesta.

Uso:
  python disenar_eq.py jvc_cerca_I [--f3 90] [--pasoalto 65] [--max-refuerzo 9]

Cadena de procesado (la misma que hace el firmware, con las mismas
fórmulas de filtros biquad de R. Bristow-Johnson):
  ganancia previa -> paso alto -> corte de la joroba -> refuerzo del grave

Curva de partida: los tonos fijos por debajo de 120 Hz (respuesta estable,
la que importa con música) y el barrido por encima, unidos de 120 a 190 Hz.
El barrido rápido se queda corto en el grave de esta cápsula: con un barrido
lento o con tonos sale varios dB más alto.

Resultados en resultados/<medida>_eq/:
  prediccion.png    curva medida, corrección y curva prevista
  parametros.json   los valores de los filtros
  comandos.txt      los comandos para el puerto serie del ESP32
"""
import argparse
import json
import sys

import numpy as np

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from comun import CARPETA

FS_BT = 44100.0                      # frecuencia habitual del Bluetooth (A2DP)
F = 20.0 * 2 ** (np.arange(0, 330) / 48)   # 20 Hz .. ~2 kHz
RESULTADOS = CARPETA / "resultados"


# ------------------------------------------- filtros biquad (RBJ Audio EQ Cookbook)

def biquad_paso_alto(f0, q=0.7071, fs=FS_BT):
    w = 2 * np.pi * f0 / fs
    alfa = np.sin(w) / (2 * q)
    c = np.cos(w)
    b = [(1 + c) / 2, -(1 + c), (1 + c) / 2]
    a = [1 + alfa, -2 * c, 1 - alfa]
    return np.array(b) / a[0], np.array(a) / a[0]


def biquad_pico(f0, ganancia_db, q, fs=FS_BT):
    A = 10 ** (ganancia_db / 40)
    w = 2 * np.pi * f0 / fs
    alfa = np.sin(w) / (2 * q)
    c = np.cos(w)
    b = [1 + alfa * A, -2 * c, 1 - alfa * A]
    a = [1 + alfa / A, -2 * c, 1 - alfa / A]
    return np.array(b) / a[0], np.array(a) / a[0]


def respuesta_db(b, a, f=F, fs=FS_BT):
    z = np.exp(-1j * 2 * np.pi * f / fs)
    h = (b[0] + b[1] * z + b[2] * z ** 2) / (a[0] + a[1] * z + a[2] * z ** 2)
    return 20 * np.log10(np.abs(h))


def cadena_db(p):
    """Respuesta de toda la corrección (sin la ganancia previa)."""
    total = np.zeros_like(F)
    if p["pasoalto"] > 0:
        total += respuesta_db(*biquad_paso_alto(p["pasoalto"]))
    total += respuesta_db(*biquad_pico(p["joroba_f"], p["joroba_db"], p["joroba_q"]))
    total += respuesta_db(*biquad_pico(p["grave_f"], p["grave_db"], p["grave_q"]))
    return total


# ------------------------------------------------------------- curva medida

def curva_combinada(nombre):
    carpeta = RESULTADOS / nombre
    if not (carpeta / "tonos.csv").exists():
        sys.exit(f"No encuentro {carpeta}: analiza antes la medida (tipo cerca).")
    r = np.genfromtxt(carpeta / "respuesta.csv", delimiter=",", names=True)
    t = np.genfromtxt(carpeta / "tonos.csv", delimiter=",", names=True)
    barrido = np.interp(np.log(F), np.log(r["frecuencia_Hz"]), r["nivel_dB"])
    tonos = np.interp(np.log(F), np.log(t["frecuencia_Hz"]), t["nivel_dB"])
    # Por debajo del primer tono, la caída sigue a 24 dB por octava
    f0 = t["frecuencia_Hz"][0]
    bajo = F < f0
    tonos[bajo] = t["nivel_dB"][0] - 24 * np.log2(f0 / F[bajo])
    peso = np.clip(np.log2(F / 120) / np.log2(190 / 120), 0, 1)
    return (1 - peso) * tonos + peso * barrido


def objetivo(f3):
    """0 dB plano por encima de f3 y caída de 2º orden (Butterworth) por debajo."""
    x = F / f3
    return 20 * np.log10(x ** 2 / np.sqrt(1 + x ** 4))


# ------------------------------------------------------------------- ajuste

RANGOS = {
    "joroba_f": (130, 230), "joroba_db": (-10, 0), "joroba_q": (0.7, 5),
    "grave_f": (60, 130), "grave_db": (0, None), "grave_q": (0.5, 3),
}


def error(p, medida, meta, banda):
    pred = medida + cadena_db(p)
    return float(np.sqrt(np.mean((pred[banda] - meta[banda]) ** 2)))


def ajustar(medida, f3, pasoalto, max_refuerzo):
    p = dict(pasoalto=pasoalto, joroba_f=170.0, joroba_db=-5.0, joroba_q=2.0,
             grave_f=95.0, grave_db=6.0, grave_q=1.0)
    rangos = dict(RANGOS, grave_db=(0, max_refuerzo))
    meta = objetivo(f3)
    banda = (F >= max(pasoalto, 50)) & (F <= 600)
    pasos = dict(joroba_f=10, joroba_db=1, joroba_q=0.5, grave_f=10, grave_db=1, grave_q=0.3)
    mejor = error(p, medida, meta, banda)
    for _ in range(6):                       # descenso por coordenadas, pasos cada vez menores
        mejora = True
        while mejora:
            mejora = False
            for k, paso in pasos.items():
                for signo in (+1, -1):
                    q = dict(p)
                    lo, hi = rangos[k]
                    q[k] = float(np.clip(p[k] + signo * paso, lo, hi))
                    e = error(q, medida, meta, banda)
                    if e < mejor - 1e-4:
                        p, mejor, mejora = q, e, True
        pasos = {k: v / 2 for k, v in pasos.items()}
    p = {k: round(v, 1) if k.endswith(("_f", "pasoalto")) else round(v, 2) for k, v in p.items()}
    return p, mejor


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("medida")
    ap.add_argument("--f3", type=float, default=90.0, help="F3 objetivo (90 Hz)")
    ap.add_argument("--pasoalto", type=float, default=65.0, help="paso alto de protección (65 Hz)")
    ap.add_argument("--max-refuerzo", type=float, default=9.0, help="refuerzo máximo del grave (9 dB)")
    a = ap.parse_args()

    medida = curva_combinada(a.medida)
    p, err = ajustar(medida, a.f3, a.pasoalto, a.max_refuerzo)
    eq = cadena_db(p)
    prevista = medida + eq
    # Ganancia previa: lo que más sube la corrección, para que la señal
    # digital nunca pase del máximo (redondeado hacia arriba).
    p["ganancia"] = -float(np.ceil(max(np.max(eq), 0)))

    def nivel(curva, f):
        return float(np.interp(np.log(f), np.log(F), curva))

    def cruce(curva, umbral):
        idx = np.where((F <= 600))[0][::-1]
        for i, j in zip(idx[:-1], idx[1:]):
            if curva[i] >= umbral > curva[j]:
                return float(F[j])
        return None

    carpeta = RESULTADOS / f"{a.medida}_eq"
    carpeta.mkdir(parents=True, exist_ok=True)
    resumen = dict(medida=a.medida, objetivo_f3=a.f3, error_db=round(err, 2), parametros=p,
                   f3_antes=cruce(medida, -3), f3_despues=cruce(prevista, -3),
                   joroba_antes_db=round(float(np.max(medida[(F > 120) & (F < 300)])), 1),
                   joroba_despues_db=round(float(np.max(prevista[(F > 120) & (F < 300)])), 1),
                   refuerzo_max_db=round(float(np.max(eq)), 1),
                   ganancia_60hz_db=round(nivel(eq, 60), 1))
    (carpeta / "parametros.json").write_text(json.dumps(resumen, indent=2, ensure_ascii=False),
                                              encoding="utf-8")
    comandos = [f"ganancia {p['ganancia']:.0f}",
                f"pasoalto {p['pasoalto']:.0f}",
                f"joroba {p['joroba_f']:.0f} {p['joroba_db']:.1f} {p['joroba_q']:.2f}",
                f"grave {p['grave_f']:.0f} {p['grave_db']:.1f} {p['grave_q']:.2f}",
                "dsp on"]
    (carpeta / "comandos.txt").write_text("\n".join(comandos) + "\n", encoding="utf-8")

    fig, ax = plt.subplots(figsize=(10, 5.5))
    ax.semilogx(F, medida, color="#1f77b4", lw=2, label="medida (tonos + barrido)")
    ax.semilogx(F, eq, color="#2ca02c", lw=1.2, ls="--", label="corrección del DSP")
    ax.semilogx(F, prevista, color="#d62728", lw=2.4, label="prevista con el DSP")
    ax.semilogx(F, objetivo(a.f3), color="#999", lw=1, ls=":", label=f"objetivo (F3 = {a.f3:.0f} Hz)")
    ax.axhline(0, color="#444", lw=0.6)
    ax.set_xlim(20, 2000)
    ax.set_ylim(-35, 12)
    ax.set_xticks([20, 30, 50, 70, 100, 150, 200, 300, 500, 1000, 2000])
    ax.set_xticklabels(["20", "30", "50", "70", "100", "150", "200", "300", "500", "1k", "2k"])
    ax.grid(True, which="both", alpha=0.3)
    ax.set_xlabel("Frecuencia (Hz)")
    ax.set_ylabel("Nivel relativo (dB)")
    ax.set_title(f"Corrección DSP prevista — {a.medida}")
    texto = (f"paso alto {p['pasoalto']:.0f} Hz\n"
             f"joroba {p['joroba_db']:+.1f} dB a {p['joroba_f']:.0f} Hz (Q {p['joroba_q']:.1f})\n"
             f"grave {p['grave_db']:+.1f} dB a {p['grave_f']:.0f} Hz (Q {p['grave_q']:.1f})\n"
             f"ganancia previa {p['ganancia']:.0f} dB")
    ax.text(0.02, 0.97, texto, transform=ax.transAxes, va="top", fontsize=9, family="monospace",
            bbox=dict(fc="white", ec="#ccc"))
    ax.legend(loc="lower right", fontsize=8)
    fig.tight_layout()
    fig.savefig(carpeta / "prediccion.png", dpi=130)

    print(json.dumps(resumen, indent=2, ensure_ascii=False))
    print("\nComandos para el ESP32:\n  " + "\n  ".join(comandos))
    print(f"\nResultados en {carpeta}")


if __name__ == "__main__":
    main()
