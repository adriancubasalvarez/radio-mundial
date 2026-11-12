"""Genera CONEXIONADO_PRUEBA_BT.pdf: esquema y lista de conexiones de la
prueba Bluetooth del boombox (ESP32 DevKit + PCM5102A + TPA3116D2).

Uso:  python conexionado.py      (necesita matplotlib)
"""
import pathlib

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.patches import FancyBboxPatch

SALIDA = pathlib.Path(__file__).with_name("CONEXIONADO_PRUEBA_BT.pdf")

ROJO, NEGRO, NARANJA, VERDE, AZUL, MORADO, GRIS = (
    "#d62728", "#222222", "#ff7f0e", "#2ca02c", "#1f77b4", "#9467bd", "#7f7f7f")


def caja(ax, x, y, w, h, titulo, sub="", color="#e8eef7"):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=3",
                                fc=color, ec="#333", lw=1.2))
    ax.text(x + w / 2, y + h - 6, titulo, ha="center", va="top", fontsize=11, weight="bold")
    if sub:
        ax.text(x + w / 2, y + h - 12, sub, ha="center", va="top", fontsize=7.5, color="#444")


def pin(ax, x, y, nombre, lado):
    ax.plot(x, y, "o", ms=5, mfc="white", mec="#333", zorder=5)
    dx, ha = (-2.5, "right") if lado == "dentro_der" else (2.5, "left")
    ax.text(x + dx, y, nombre, ha=ha, va="center", fontsize=8.5, family="monospace", zorder=6)


def cable(ax, pts, color, lw=2.2):
    xs, ys = zip(*pts)
    ax.plot(xs, ys, color=color, lw=lw, solid_capstyle="round", zorder=3)


def punto(ax, x, y, color):
    ax.plot(x, y, "o", ms=4.5, color=color, zorder=4)


def pagina_esquema(pdf):
    fig = plt.figure(figsize=(11.69, 8.27))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, 297); ax.set_ylim(0, 210); ax.axis("off")
    ax.text(10, 200, "Radio Mundial Boombox — prueba Bluetooth (Fase A): conexionado",
            fontsize=15, weight="bold")
    ax.text(10, 193, "ESP32 DevKit 30 pines (WROOM-32)  ·  DAC PCM5102A (WCMCU-5102)  ·  "
            "amplificador TPA3116D2  ·  2 × JVC CS-J520X", fontsize=9, color="#444")

    # ---- ESP32 -------------------------------------------------------
    caja(ax, 12, 70, 52, 100, "ESP32 DevKit", "30 pines · USB-C · CH340")
    esp = {"VIN (5 V)": 145, "GND": 133, "D26": 115, "D25": 103, "D22": 91}
    for n, y in esp.items():
        pin(ax, 64, y, n, "dentro_der")
    ax.text(38, 76, "USB-C ⇦ cargador 5 V\n o el PC", ha="center", fontsize=8)

    # ---- DAC ---------------------------------------------------------
    caja(ax, 128, 40, 50, 145, "PCM5102A", "módulo WCMCU-5102", color="#efe4f7")
    orden = ["VCC", "3.3V", "GND", "FLT", "DMP", "SCL", "BCK", "DIN", "LCK", "FMT", "XMT"]
    dac = {n: 165 - i * 11 for i, n in enumerate(orden)}
    for n, y in dac.items():
        pin(ax, 128, y, n, "fuera_izq")
    sal = {"L": 80, "G": 70, "R": 60}
    for n, y in sal.items():
        pin(ax, 178, y, n, "dentro_der")
    ax.text(153, 47, "(o el jack LINE OUT)", ha="center", fontsize=7.5, color="#444")

    # ESP32 -> DAC (ruta en escalera para que no se crucen)
    rutas = [("VIN (5 V)", "VCC", ROJO, 78), ("GND", "GND", NEGRO, 84),
             ("D26", "BCK", NARANJA, 90), ("D22", "DIN", VERDE, 96), ("D25", "LCK", AZUL, 102)]
    for e, d, c, xm in rutas:
        cable(ax, [(64, esp[e]), (xm, esp[e]), (xm, dac[d]), (128, dac[d])], c)
        ax.text(xm + 1, (esp[e] + dac[d]) / 2, "", fontsize=7)

    # Puentes del propio DAC: FLT, DMP, SCL, FMT -> GND ; XMT -> 3.3V
    xg = 116
    cable(ax, [(128, dac["GND"]), (xg, dac["GND"]), (xg, dac["FMT"]), (128, dac["FMT"])], NEGRO, 1.6)
    for n in ("FLT", "DMP", "SCL"):
        cable(ax, [(xg, dac[n]), (128, dac[n])], NEGRO, 1.6); punto(ax, xg, dac[n], NEGRO)
    xr = 110
    cable(ax, [(128, dac["3.3V"]), (xr, dac["3.3V"]), (xr, dac["XMT"] - 0.01)], ROJO, 1.6)
    cable(ax, [(xr, dac["XMT"]), (128, dac["XMT"])], ROJO, 1.6)
    ax.text(xr - 1, 36, "puentes en el DAC:\nFLT, DMP, SCL, FMT → GND\nXMT → 3.3V del DAC",
            ha="center", fontsize=7.5, va="top", color="#333")

    # ---- Amplificador ------------------------------------------------
    caja(ax, 215, 95, 70, 75, "TPA3116D2", "2 × 120 W · entrada de línea", color="#e6f2e6")
    amp_in = {"IN L": 80 + 60, "IN G": 70 + 60, "IN R": 60 + 60}
    for n, y in amp_in.items():
        pin(ax, 215, y, n, "fuera_izq")
    for (n, y), c in zip(sal.items(), (AZUL, NEGRO, ROJO)):
        ny = list(amp_in.values())[list(sal).index(n)]
        xm = 192 + list(sal).index(n) * 5
        cable(ax, [(178, y), (xm, y), (xm, ny), (215, ny)], MORADO if n != "G" else NEGRO, 1.8)
    ax.text(196, 92, "cable de audio\ncorto, apantallado", ha="center", fontsize=7.5, va="top")
    pin(ax, 285, 160, "+12 V", "dentro_der"); pin(ax, 285, 150, "GND", "dentro_der")
    for i, n in enumerate(("L+", "L−", "R+", "R−")):
        pin(ax, 225 + i * 15, 95, "", "fuera_izq")
        ax.text(225 + i * 15, 99, n, ha="center", fontsize=8.5, family="monospace")

    # Fuente
    caja(ax, 238, 178, 48, 14, "Fuente 12 V 2,5 A", color="#fff3d6")
    cable(ax, [(270, 178), (270, 172), (288, 172), (288, 160), (285, 160)], ROJO)
    cable(ax, [(278, 178), (278, 175), (292, 175), (292, 150), (285, 150)], NEGRO)

    # Altavoces
    for i, (nom, xs) in enumerate((("JVC izquierdo", 215), ("JVC derecho", 250))):
        caja(ax, xs, 35, 32, 30, nom, "4 Ω", color="#f2f2f2")
        a, b = 225 + i * 30, 240 + i * 30
        cable(ax, [(a, 95), (a, 80), (xs + 9, 80), (xs + 9, 65)], ROJO, 1.8)
        cable(ax, [(b, 95), (b, 75), (xs + 23, 75), (xs + 23, 65)], NEGRO, 1.8)
        ax.text(xs + 9, 61, "+", ha="center", fontsize=10, weight="bold")
        ax.text(xs + 23, 61, "−", ha="center", fontsize=10, weight="bold")
    ax.text(248, 26, "¡Nunca unir L− o R− a GND ni entre sí!", ha="center", fontsize=8.5,
            color=ROJO, weight="bold")

    # Leyenda
    ley = [(ROJO, "5 V / +"), (NEGRO, "GND / −"), (NARANJA, "BCK (GPIO 26)"),
           (VERDE, "DIN (GPIO 22)"), (AZUL, "LCK (GPIO 25)"), (MORADO, "audio L / R")]
    for i, (c, t) in enumerate(ley):
        ax.plot([12, 20], [52 - i * 6.5] * 2, color=c, lw=3)
        ax.text(23, 52 - i * 6.5, t, va="center", fontsize=8.5)
    ax.text(10, 6, "Los nombres de las bornas del TPA3116D2 dependen de tu placa: sigue su serigrafía. "
            "Firmware: boombox/prueba_bt (rama boombox).", fontsize=7.5, color="#555")
    pdf.savefig(fig); plt.close(fig)


def pagina_lista(pdf):
    fig = plt.figure(figsize=(8.27, 11.69))
    ax = fig.add_axes([0, 0, 1, 1]); ax.axis("off")
    y = 0.955
    def linea(t, tam=9.5, peso="normal", color="#111", sangria=0.07, salto=0.0195):
        nonlocal y
        ax.text(sangria, y, t, fontsize=tam, weight=peso, color=color, va="top",
                transform=ax.transAxes, family="DejaVu Sans")
        y -= salto
    linea("Lista de conexiones y pasos", 15, "bold", salto=0.035)

    linea("1. ESP32 → DAC PCM5102A", 11.5, "bold", salto=0.026)
    filas = [("ESP32", "DAC", "Para qué"),
             ("VIN (5 V)", "VCC", "alimentación (el DAC lleva regulador)"),
             ("GND", "GND", "masa común"),
             ("D26 (GPIO 26)", "BCK", "reloj de bit"),
             ("D22 (GPIO 22)", "DIN", "datos de audio"),
             ("D25 (GPIO 25)", "LCK", "canal izquierdo / derecho"),
             ("—", "3.3V", "no va al ESP32: solo alimenta el puente de XMT")]
    for i, (a, b, c) in enumerate(filas):
        p = "bold" if i == 0 else "normal"
        ax.text(0.09, y, a, fontsize=9.5, weight=p, transform=ax.transAxes, va="top")
        ax.text(0.33, y, b, fontsize=9.5, weight=p, transform=ax.transAxes, va="top", family="monospace")
        ax.text(0.47, y, c, fontsize=9.5, weight=p, transform=ax.transAxes, va="top")
        y -= 0.0195
    y -= 0.008
    linea("2. Puentes en el propio DAC (cablecitos entre sus pines)", 11.5, "bold", salto=0.026)
    linea("•  FLT → GND      •  DMP → GND      •  SCL → GND      •  FMT → GND", sangria=0.09)
    linea("•  XMT → 3.3V del DAC   (si XMT queda al aire o a GND, NO SUENA)", sangria=0.09, salto=0.03)

    linea("3. DAC → amplificador TPA3116D2", 11.5, "bold", salto=0.026)
    linea("•  L → entrada izquierda,  G → masa de la entrada,  R → entrada derecha", sangria=0.09)
    linea("•  O bien un cable jack 3,5 mm desde LINE OUT a la entrada del amplificador.", sangria=0.09, salto=0.03)

    linea("4. Amplificador → altavoces y fuente", 11.5, "bold", salto=0.026)
    linea("•  L+ / L− al JVC izquierdo,  R+ / R− al JVC derecho (+ con +, igual en los dos).", sangria=0.09)
    linea("•  Fuente 12 V 2,5 A a la entrada de alimentación del amplificador (respeta + y −).", sangria=0.09)
    linea("•  ESP32 alimentado por su USB-C (cargador de 5 V o el PC).", sangria=0.09, salto=0.03)

    linea("Orden de montaje y prueba", 11.5, "bold", salto=0.026)
    pasos = ["Cablea todo con TODO desconectado.",
             "Revisa con el polímetro que no haya corto entre 5 V y GND en el ESP32.",
             "Conecta solo el USB del ESP32: mide 3,3 V entre los pines 3.3V y GND del DAC.",
             "Graba el firmware:  pio run -t upload   (carpeta boombox/prueba_bt).",
             "Abre el monitor serie (115200): debe decir «Listo: busca \"Radio Mundial BT\"».",
             "Pon el potenciómetro del amplificador al MÍNIMO y enciende su fuente de 12 V.",
             "Empareja el móvil con «Radio Mundial BT» y pon música a volumen bajo.",
             "Sube poco a poco el volumen del amplificador y luego el del móvil.",
             "Comprueba izquierda y derecha con una pista de prueba de canales."]
    for i, p in enumerate(pasos, 1):
        linea(f"{i}.  {p}", sangria=0.09)
    y -= 0.01

    linea("Avisos importantes", 11.5, "bold", color=ROJO, salto=0.026)
    avisos = ["El XL6009 es un ELEVADOR: NO lo uses para sacar 5 V de 12/24 V (quemaría el ESP32).",
              "Nunca unas L− o R− del amplificador a GND ni entre sí (salidas en puente).",
              "Apaga el amplificador antes de conectar o desconectar cables de audio o altavoces.",
              "Con la fuente de 24 V futura, limitar volumen: el JVC aguanta unos 30 W RMS."]
    for a in avisos:
        linea("•  " + a, sangria=0.09)
    y -= 0.01

    linea("Si algo falla", 11.5, "bold", salto=0.026)
    fallos = [("No suena nada", "revisa XMT → 3.3V y SCL → GND; que BCK/DIN/LCK no estén cruzados."),
              ("Suena con pitido o raro", "revisa FMT → GND (formato I2S) y que la masa sea común."),
              ("Solo un canal", "revisa L y R hasta el amplificador y los dos altavoces."),
              ("Zumbido", "cable de audio más corto; masa del DAC (G) unida a la del amplificador."),
              ("No aparece en el móvil", "mira el monitor serie; reinicia el ESP32 con el botón EN.")]
    for t, d in fallos:
        ax.text(0.09, y, t, fontsize=9.5, weight="bold", transform=ax.transAxes, va="top")
        ax.text(0.36, y, d, fontsize=9.5, transform=ax.transAxes, va="top", wrap=True)
        y -= 0.0195
    ax.text(0.07, 0.012, "Radio Mundial Boombox · Fase A (prueba Bluetooth) · nombres de pines "
            "según la serigrafía de cada placa", fontsize=7.5, color="#666", transform=ax.transAxes)
    pdf.savefig(fig); plt.close(fig)


if __name__ == "__main__":
    with PdfPages(SALIDA) as pdf:
        pagina_esquema(pdf)
        pagina_lista(pdf)
    print(f"Creado {SALIDA}")
