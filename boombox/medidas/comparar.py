"""Compara medidas ya analizadas y une campo cercano + lejano en una curva.

Uso:
  python comparar.py jvc_cerca_I jvc_cerca_relleno_I --salida relleno
      Superpone las curvas de esas medidas (carpetas de resultados/).

  python comparar.py --unir jvc_cerca_I jvc_50cm_I --salida jvc_completa_I [--corte 400]
      Curva completa de 20 Hz a 20 kHz: el campo cercano por debajo del
      corte y la medida a 50 cm por encima, igualadas en el corte.

Resultados en resultados/<salida>/: comparacion.png (o completa.png) y,
al unir, respuesta.csv con la curva completa.
"""
import argparse
import sys

import numpy as np

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from comun import CARPETA

RESULTADOS = CARPETA / "resultados"
MARCAS = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000]
ETIQUETAS = ["20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k"]


def leer(nombre):
    ruta = RESULTADOS / nombre / "respuesta.csv"
    if not ruta.exists():
        sys.exit(f"No existe {ruta}: analiza antes esa medida con analizar.py")
    d = np.genfromtxt(ruta, delimiter=",", names=True)
    return d["frecuencia_Hz"], d["nivel_dB"], d["valida"].astype(bool)


def ejes(ax, titulo):
    ax.set_xscale("log")
    ax.set_xlim(20, 20000)
    ax.set_ylim(-30, 15)
    ax.set_xticks(MARCAS)
    ax.set_xticklabels(ETIQUETAS)
    ax.axhline(0, color="#444", lw=0.6)
    ax.grid(True, which="both", alpha=0.3)
    ax.set_xlabel("Frecuencia (Hz)")
    ax.set_ylabel("Nivel relativo (dB)")
    ax.set_title(titulo)
    ax.legend(fontsize=8, loc="lower right")


def comparar(nombres, salida):
    fig, ax = plt.subplots(figsize=(10, 5.5))
    for nombre in nombres:
        f, n, v = leer(nombre)
        linea, = ax.plot(f[v], n[v], lw=1.8, label=nombre)
        ax.plot(f[~v], n[~v], lw=0.8, color=linea.get_color(), alpha=0.35)
    ejes(ax, "Comparación de medidas (en tenue: fuera de la zona válida)")
    fig.tight_layout()
    carpeta = RESULTADOS / salida
    carpeta.mkdir(parents=True, exist_ok=True)
    fig.savefig(carpeta / "comparacion.png", dpi=130)
    print(f"Resultado en {carpeta / 'comparacion.png'}")


def unir(cerca, lejos, salida, corte):
    fc, nc, _ = leer(cerca)
    fl, nl, vl = leer(lejos)
    if not np.allclose(fc, fl):
        sys.exit("Las dos medidas no usan la misma rejilla de frecuencias.")
    zona = (fc >= corte / 1.5) & (fc <= corte * 1.5) & vl
    if not np.any(zona):
        sys.exit(f"La medida lejana no es válida alrededor de {corte:.0f} Hz: "
                 "usa un corte más alto (--corte) o una ventana más larga.")
    ajuste = float(np.mean(nl[zona] - nc[zona]))
    # Fundido suave de media octava alrededor del corte
    peso = np.clip((np.log2(fc / corte) + 0.25) / 0.5, 0, 1)
    completa = (1 - peso) * (nc + ajuste) + peso * nl
    # 0 dB = media entre 300 Hz y 3 kHz
    m = (fc >= 300) & (fc <= 3000)
    desplaz = float(np.mean(completa[m]))
    completa -= desplaz

    carpeta = RESULTADOS / salida
    carpeta.mkdir(parents=True, exist_ok=True)
    with open(carpeta / "respuesta.csv", "w", encoding="utf-8") as fh:
        fh.write("frecuencia_Hz,nivel_dB,valida\n")
        for f, n in zip(fc, completa):
            fh.write(f"{f:.2f},{n:.2f},1\n")
    fig, ax = plt.subplots(figsize=(10, 5.5))
    ax.plot(fc, nc + ajuste - desplaz, lw=0.8, color="#1f77b4", alpha=0.4,
            label=f"campo cercano ({cerca})")
    ax.plot(fl, nl - desplaz, lw=0.8, color="#2ca02c", alpha=0.4, label=f"a distancia ({lejos})")
    ax.plot(fc, completa, lw=2.2, color="#d62728", label="curva completa")
    ax.axvline(corte, color="#999", ls="--", lw=0.8)
    ejes(ax, f"Respuesta completa — {salida} (unión en {corte:.0f} Hz)")
    fig.tight_layout()
    fig.savefig(carpeta / "completa.png", dpi=130)
    print(f"Curva completa en {carpeta}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("medidas", nargs="*", help="nombres de medidas a superponer")
    ap.add_argument("--unir", nargs=2, metavar=("CERCA", "LEJOS"))
    ap.add_argument("--corte", type=float, default=400.0, help="frecuencia de unión (400 Hz)")
    ap.add_argument("--salida", required=True)
    a = ap.parse_args()
    if a.unir:
        unir(a.unir[0], a.unir[1], a.salida, a.corte)
    elif a.medidas:
        comparar(a.medidas, a.salida)
    else:
        ap.error("indica medidas para comparar o --unir CERCA LEJOS")


if __name__ == "__main__":
    main()
