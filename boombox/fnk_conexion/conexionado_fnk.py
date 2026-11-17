"""Genera CONEXIONADO_FNK0104_DAC.pdf: cómo conectar la Freenove FNK0104
(2,8") al DAC PCM5102A (WCMCU-5102) y al amplificador, y cómo comprobar
los cables con los firmwares del kit.

Uso:  python conexionado_fnk.py      (necesita matplotlib)
"""
import pathlib

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.patches import FancyBboxPatch, Rectangle

SALIDA = pathlib.Path(__file__).with_name("CONEXIONADO_FNK0104_DAC.pdf")
A4_H = (11.69, 8.27)
A4_V = (8.27, 11.69)
ROJO, NEGRO, NARANJA, VERDE, AZUL, GRIS = ("#d62728", "#222222", "#ff7f0e", "#2ca02c",
                                           "#1f77b4", "#7f7f7f")
TITULO = "#1f4e79"


def caja(ax, x, y, w, h, titulo, sub="", color="#e8eef7"):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=2",
                                fc=color, ec="#333", lw=1.2))
    ax.text(x + w / 2, y + h - 3, titulo, ha="center", va="top", fontsize=11, weight="bold")
    if sub:
        ax.text(x + w / 2, y + h - 8, sub, ha="center", va="top", fontsize=7.5, color="#444")


def conector(ax, x, y, nombre, pines, arriba=True):
    """Conector de 4 pines de 1,25 mm dibujado en horizontal; devuelve las x de los pines."""
    ax.add_patch(Rectangle((x - 1, y - 2.2), 4 * 5 + 2, 4.4, fc="white", ec="#333", lw=1))
    xs = []
    for i, p in enumerate(pines):
        px = x + 2.5 + i * 5
        ax.plot(px, y, "s", ms=6, mfc="#ddd", mec="#333", zorder=5)
        # Las etiquetas van en el lado contrario al de los cables, para que no las tapen
        ax.text(px, y + (-3.5 if arriba else 3.5), p, ha="center", va="top" if arriba else "bottom",
                fontsize=6.5, family="monospace")
        xs.append(px)
    ax.text(x + 10, y + (-11 if arriba else 11), nombre, ha="center", va="center", fontsize=8,
            weight="bold")
    return xs


def cable(ax, pts, color, lw=2.2):
    xs, ys = zip(*pts)
    ax.plot(xs, ys, color=color, lw=lw, solid_capstyle="round", zorder=3)


def pagina_esquema(pdf):
    fig = plt.figure(figsize=A4_H)
    ax = fig.add_axes([0.02, 0.02, 0.96, 0.9])
    ax.set_xlim(0, 200)
    ax.set_ylim(0, 125)
    ax.axis("off")
    fig.text(0.03, 0.955, "Radio Mundial Boombox — FNK0104 → DAC PCM5102A → amplificador",
             fontsize=15, weight="bold", color=TITULO)
    fig.text(0.03, 0.925, "Vista de los conectores tal como los rotula la serigrafía de la placa. "
             "El orden real de los colores del cable se comprueba con el firmware «identificar».",
             fontsize=9, color="#444")

    # Placa FNK0104
    caja(ax, 5, 25, 70, 70, "")
    ax.text(40, 62, "Freenove FNK0104", ha="center", fontsize=12, weight="bold")
    ax.text(40, 56, "ESP32-S3, pantalla 2,8\" (vista por detrás)", ha="center", fontsize=8,
            color="#444")
    xs_io = conector(ax, 30, 95, "«I/O pin port» (P3)", ["IO2", "IO3", "IO14", "IO21"])
    xs_ua = conector(ax, 30, 25, "«Serial port» UART (P2)", ["5V", "GND", "TXD", "RXD"],
                     arriba=False)
    ax.text(8, 45, "USB-C: alimentación (powerbank en las pruebas)", fontsize=7.5, ha="left",
            va="center", color="#444")

    # DAC
    caja(ax, 100, 20, 42, 80, "DAC PCM5102A", "WCMCU-5102 (puentes ya soldados)")
    pines_dac = ["LCK", "DIN", "BCK", "GND", "VCC"]
    ys_dac = {}
    for i, p in enumerate(pines_dac):
        y = 82 - i * 11
        ax.plot(100, y, "o", ms=6, mfc="white", mec="#333", zorder=5)
        ax.text(103, y, p, va="center", fontsize=9, family="monospace")
        ys_dac[p] = y
    ax.text(121, 26, "FLT, DMP, SCL, FMT → GND\nXMT → 3.3V del DAC", fontsize=7, ha="center",
            color="#444")

    # Cables de señal
    io2, io3, io14, io21 = xs_io
    cable(ax, [(io2, 97), (io2, 118), (90, 118), (90, ys_dac["DIN"]), (100, ys_dac["DIN"])], NARANJA)
    cable(ax, [(io14, 97), (io14, 115), (93, 115), (93, ys_dac["BCK"]), (100, ys_dac["BCK"])], AZUL)
    cable(ax, [(io21, 97), (io21, 112), (96, 112), (96, ys_dac["LCK"]), (100, ys_dac["LCK"])], VERDE)
    # Alimentación
    v5, gnd, tx, rx = xs_ua
    cable(ax, [(v5, 23), (v5, 8), (84, 8), (84, ys_dac["VCC"]), (100, ys_dac["VCC"])], ROJO)
    cable(ax, [(gnd, 23), (gnd, 5), (87, 5), (87, ys_dac["GND"]), (100, ys_dac["GND"])], NEGRO)

    # Amplificador
    caja(ax, 160, 45, 36, 30, "Amplificador", "TPA3116D2 · 12 V", color="#f3eee2")
    cable(ax, [(142, 60), (160, 60)], GRIS, lw=3)
    ax.text(151, 63, "jack o L/G/R\n(como ahora)", ha="center", fontsize=7)
    ax.text(178, 40, "→ cápsulas", ha="center", fontsize=8)

    # Leyenda
    ley = [(NARANJA, "IO2 → DIN (datos)"), (AZUL, "IO14 → BCK (reloj de bit)"),
           (VERDE, "IO21 → LCK (izquierda/derecha)"), (ROJO, "5V → VCC"), (NEGRO, "GND → GND"),
           ("white", "IO3, TXD y RXD: sin conectar")]
    for i, (c, t) in enumerate(ley):
        ax.plot([150, 158], [108 - i * 5] * 2, color=c, lw=3)
        ax.text(160, 108 - i * 5, t, va="center", fontsize=8)
    pdf.savefig(fig)
    plt.close(fig)


def pagina_texto(pdf, titulo, bloques):
    fig = plt.figure(figsize=A4_V)
    fig.text(0.07, 0.955, "Radio Mundial Boombox — conexión FNK0104 → DAC", fontsize=9, color=GRIS)
    fig.text(0.07, 0.93, titulo, fontsize=16, weight="bold", color=TITULO, va="top")
    y = 0.885
    for cab, lineas in bloques:
        fig.text(0.07, y, cab, fontsize=11.5, weight="bold", color=TITULO, va="top")
        y -= 0.026
        for l in lineas:
            if l.startswith("$ "):
                fig.text(0.09, y, l, fontsize=8.6, va="top", family="monospace")
            else:
                fig.text(0.1 if l.startswith("   ") else 0.08, y, l.strip(), fontsize=9.4, va="top")
            y -= 0.0205
        y -= 0.011
    pdf.savefig(fig)
    plt.close(fig)


def main():
    with PdfPages(SALIDA) as pdf:
        pagina_esquema(pdf)
        pagina_texto(pdf, "Paso a paso", [
            ("Material", [
                "• FNK0104 de 2,8\" con táctil, el DAC PCM5102A (WCMCU-5102), el amplificador.",
                "• DOS cables de 1,25 mm de 4 pines a Dupont: el kit trae uno; hace falta otro.",
                "• Polímetro. Powerbank para alimentar la FNK0104 en las pruebas.",
            ]),
            ("1. Identificar el cable del «Serial port» (UART)", [
                "1. Enchufa un cable en el «Serial port» (abajo, serigrafía UART).",
                "2. Con la placa APAGADA, polímetro en continuidad: el cable que pita con la",
                "   carcasa metálica del USB-C es GND. Márcalo con cinta negra.",
                "3. Enciende la placa (USB-C). En tensión continua, negro en GND: el cable que",
                "   da ~5 V es 5V. Márcalo con cinta roja. TXD y RXD no se usan: aíslalos.",
            ]),
            ("2. Identificar los cables del «I/O pin port»", [
                "1. Enchufa el otro cable en el «I/O pin port» (arriba, serigrafía IO2 IO3 IO14 IO21).",
                "2. Flashea el firmware «identificar» (desde boombox/fnk_conexion):",
                "$ pio run -e identificar -t upload",
                "3. Polímetro en tensión continua, negro en el GND del paso 1. Prueba cada cable:",
                "   LED ROJO  → el que da 3,3 V es IO2  → irá a DIN",
                "   LED VERDE → el que da 3,3 V es IO14 → irá a BCK",
                "   LED AZUL  → el que da 3,3 V es IO21 → irá a LCK",
                "   El cuarto cable (IO3) no da nunca 3,3 V: aíslalo, no se usa.",
                "4. Marca cada cable (cinta o rotulador).",
            ]),
            ("3. Conectar el DAC (con todo APAGADO)", [
                "IO2 → DIN   IO14 → BCK   IO21 → LCK   5V → VCC   GND → GND",
                "La salida del DAC (jack o L/G/R) al amplificador, como en la prueba Bluetooth.",
                "Antes de encender: mando del amplificador BAJO.",
            ]),
            ("4. Tono de prueba", [
                "$ pio run -e tono -t upload",
                "Se repite: 440 Hz solo IZQUIERDO (LED rojo), 660 Hz solo DERECHO (verde),",
                "550 Hz por los DOS (azul) y 2 s de silencio. Sube el mando poco a poco.",
                "Si los tres suenan donde deben, los cables están bien.",
            ]),
            ("5. La radio", [
                "Flashea el firmware del boombox (rama boombox, entorno fnk0104) y en",
                "Ajustes pulsa el botón «DAC» (Fase 7): la radio sonará por el amplificador.",
            ]),
        ])
        pagina_texto(pdf, "Si algo no va", [
            ("No suena nada con el tono", [
                "• ¿GND conectado? Sin GND común el DAC no recibe nada. Es lo primero.",
                "• ¿5V llega al DAC? Mide entre VCC y GND del DAC: ~5 V; y entre 3.3V y GND: ~3,3 V.",
                "• ¿Amplificador encendido, cápsula conectada, mando subido?",
                "• Cambia IO2 (DIN) y IO21 (LCK) de sitio si dudas: es el error más típico.",
            ]),
            ("Suena distorsionado, como «robótico» o a medias", [
                "• BCK y LCK intercambiados: revisa IO14 → BCK e IO21 → LCK.",
                "• Algún cable Dupont flojo: aprieta o cambia el cable.",
            ]),
            ("Izquierdo y derecho cambiados", [
                "• Cambia los cables de las cápsulas en el amplificador (no los del DAC).",
            ]),
            ("Zumbido o ruido de fondo agudo", [
                "• Es el ruido de masa que ya conocemos: alimenta la FNK0104 con el powerbank.",
                "   Para el montaje final: convertidor CC-CC aislado de 12 V a 5 V.",
            ]),
            ("La placa no arranca tras conectar el DAC", [
                "• Revisa que el cable de IO3 esté libre (no conectado a nada).",
                "• Desconecta el DAC, arranca, y vuelve a conectarlo con la placa apagada.",
            ]),
        ])
    print(f"Creado {SALIDA}")


if __name__ == "__main__":
    main()
