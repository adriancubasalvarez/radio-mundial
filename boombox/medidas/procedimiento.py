"""Genera PROCEDIMIENTO_MEDIDAS.pdf: cómo medir las cápsulas del boombox con
el DJI Mic y el montaje Bluetooth de prueba.

Uso:  python procedimiento.py      (necesita matplotlib)
"""
import pathlib

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.patches import Circle, FancyArrowPatch, Polygon, Rectangle

SALIDA = pathlib.Path(__file__).with_name("PROCEDIMIENTO_MEDIDAS.pdf")
A4 = (8.27, 11.69)
AZUL, ROJO, GRIS = "#1f4e79", "#c00000", "#555555"


def pagina(pdf, titulo, bloques, dibujo=None):
    fig = plt.figure(figsize=A4)
    fig.text(0.07, 0.955, "Radio Mundial Boombox — medidas acústicas", fontsize=9, color=GRIS)
    fig.text(0.07, 0.93, titulo, fontsize=16, weight="bold", color=AZUL, va="top")
    y = 0.885
    if dibujo:
        ax = fig.add_axes([0.07, 0.62, 0.86, 0.25])
        dibujo(ax)
        y = 0.6
    for cab, lineas in bloques:
        fig.text(0.07, y, cab, fontsize=11.5, weight="bold", color=AZUL, va="top")
        y -= 0.027
        for l in lineas:
            mono = l.startswith(("$ ", "M")) or l.startswith(" " * 6)
            if mono:
                fig.text(0.08, y, l.rstrip(), fontsize=8.6, va="top", family="monospace")
            else:
                sangria = 0.1 if l.startswith("   ") else 0.08
                fig.text(sangria, y, l.strip(), fontsize=9.6, va="top")
            y -= 0.021
        y -= 0.012
    pdf.savefig(fig)
    plt.close(fig)


def dibujo_posiciones(ax):
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 40)
    ax.axis("off")

    def capsula(x, y):
        ax.add_patch(Polygon([[x, y - 9], [x + 10, y - 6], [x + 10, y + 6], [x, y + 9]],
                             fc="#ddd", ec="#333"))
        ax.add_patch(Rectangle((x - 1.2, y - 8), 1.2, 16, fc="#888", ec="#333"))
        ax.add_patch(Circle((x - 1.8, y), 1.3, fc="#555", ec="#333"))

    def micro(x, y):
        ax.add_patch(Rectangle((x - 2.2, y - 1.6), 2.2, 3.2, fc=ROJO, ec="#333"))

    # Campo cercano
    capsula(18, 22)
    micro(15.4, 26)
    ax.text(16, 37, "1) CERCA: micrófono a 1 cm del cono", ha="center", fontsize=9, weight="bold")
    ax.text(16, 6, "entre el tweeter central y el borde\ndel cono, mirando al cono",
            ha="center", fontsize=8)
    ax.annotate("1 cm", xy=(15.6, 27.8), xytext=(6, 32), fontsize=8,
                arrowprops=dict(arrowstyle="->", lw=0.8))
    # A distancia
    capsula(80, 22)
    micro(52, 22)
    ax.add_patch(FancyArrowPatch((53, 17), (78, 17), arrowstyle="<->", mutation_scale=10))
    ax.text(65.5, 13.5, "50 cm, en el eje (a la altura del tweeter)", ha="center", fontsize=8)
    ax.text(66, 37, "2) LEJOS: a 50 cm en el eje", ha="center", fontsize=9, weight="bold")
    ax.text(66, 4, "cápsula y micrófono a 1 m o más de suelo, mesa y paredes",
            ha="center", fontsize=8)


def main():
    with PdfPages(SALIDA) as pdf:
        pagina(pdf, "1. Preparación", [
            ("Material", [
                "• Montaje de la prueba Bluetooth: ESP32 con PCM5102A y TPA3116D2.",
                "   El ESP32 alimentado con el POWERBANK (así no hay ruido).",
                "• Una cápsula con su altavoz (la otra puede quedarse conectada).",
                "• Móvil con los archivos de la carpeta senales/ copiados.",
                "• DJI Mic: solo el transmisor (TX), que graba en su memoria.",
                "• Cinta métrica, cinta de carrocero, y algo para sujetar el micrófono",
                "   y para elevar la cápsula (trípode, taburete, cajas...).",
                "• Papel para apuntar cada medida.",
            ]),
            ("Archivos (en el PC)", [
                "$ python generar_senales.py",
                "• Copia al móvil sesion_izquierdo.wav, sesion_derecho.wav,",
                "   sesion_ambos.wav y ruido_rosa.wav.",
                "• Reprodúcelos con un reproductor sencillo (VLC o «Archivos»).",
                "• Desactiva en el móvil ecualizador, Dolby, «sonido adaptativo» y",
                "   similares. Volumen del móvil al MÁXIMO: el nivel se ajusta con el",
                "   mando del amplificador.",
            ]),
            ("DJI Mic", [
                "• Batería cargada y espacio libre en el transmisor.",
                "• Si tu firmware tiene filtro de graves (low cut), reducción de ruido",
                "   o «safety track», DESACTÍVALOS: falsean el grave.",
                "• Activa la grabación interna del transmisor y comprueba que graba",
                "   (haz una grabación corta y mírala en el PC antes de empezar).",
            ]),
            ("Habitación", [
                "• Lo más silenciosa posible: ventiladores, nevera y ordenador apagados",
                "   o lejos. Nadie hablando ni moviéndose mientras graba.",
                "• Cada sesión dura 69 s: 3 barridos de grave a agudo y 17 tonos graves.",
            ]),
        ])

        pagina(pdf, "2. Medidas", [
            ("Antes de nada: el nivel", [
                "1. Micrófono en la posición CERCA (dibujo). Pon ruido_rosa.wav.",
                "2. Sube el mando del amplificador hasta un volumen claro pero",
                "   cómodo, como música alta en una habitación. NO más: a 1 cm el",
                "   micrófono se satura (aguanta unos 114 dB SPL).",
                "3. Marca el mando con cinta y NO lo toques en las medidas que vayas",
                "   a comparar entre sí.",
            ]),
            ("Lista de medidas (graba la sesión entera cada vez)", [
                "M1  cerca_I    CERCA,  cápsula izquierda, sesion_izquierdo.wav",
                "M2  50cm_I     LEJOS,  cápsula izquierda, sesion_izquierdo.wav",
                "M3  cerca_D    CERCA,  cápsula derecha,   sesion_derecho.wav",
                "M4  50cm_D     LEJOS,  cápsula derecha,   sesion_derecho.wav",
                "Variantes (repite M1, y si puedes M2, cambiando solo una cosa):",
                "M5  cerca_relleno_I   con guata o lana suelta dentro (llenando",
                "                      la mitad o 2/3; sin tocar el cono)",
                "M6  cerca_tumbada_I   con la cápsula apoyada en la pata trasera",
                "Opcional:",
                "M7  sala       las dos cápsulas en su sitio, micrófono donde",
                "               te sientas a escuchar, sesion_ambos.wav",
            ]),
            ("Cómo grabar cada medida", [
                "1. Coloca el micrófono. 2. Empieza a grabar en el DJI.",
                "3. Pulsa reproducir la sesión en el móvil y quédate quieto.",
                "4. Cuando termine (69 s), para la grabación.",
                "5. Apunta: número de archivo del DJI → nombre de la medida.",
            ]),
        ], dibujo=dibujo_posiciones)

        pagina(pdf, "3. Análisis en el PC", [
            ("Pasar las grabaciones", [
                "• Conecta el transmisor DJI por USB-C y copia los WAV a",
                "   boombox/medidas/grabaciones/ con el nombre de cada medida.",
                "• Si lo prefieres, mándamelos y los analizo yo.",
            ]),
            ("Analizar", [
                "$ python analizar.py grabaciones/cerca_I.wav --nombre jvc_cerca_I --tipo cerca",
                "$ python analizar.py grabaciones/50cm_I.wav --nombre jvc_50cm_I --tipo lejos",
                "• Abre resultados/<nombre>/informe.pdf. Si avisa de SATURACIÓN, baja",
                "   el volumen y repite. Si la señal/ruido es baja (< 30 dB), súbelo.",
                "• En LEJOS mira impulso.png: el primer pico tras el principal es el",
                "   primer rebote. La ventana (5 ms por defecto) debe acabar antes;",
                "   si el rebote llega antes, repite con --ventana 3 (por ejemplo).",
            ]),
            ("Curva completa y comparaciones", [
                "$ python comparar.py --unir jvc_cerca_I jvc_50cm_I --salida jvc_completa_I",
                "$ python comparar.py jvc_cerca_I jvc_cerca_D --salida izquierda_derecha",
                "$ python comparar.py jvc_cerca_I jvc_cerca_relleno_I --salida efecto_relleno",
            ]),
            ("Qué mirar", [
                "• fc y Qtc (medidas CERCA): la resonancia de la caja cerrada. Qtc cerca",
                "   de 0,7 es un grave plano; por encima de 1, grave «hinchado» y",
                "   retumbante; el relleno suele bajarlo un poco.",
                "• F3: dónde el grave ha caído 3 dB. Es la cifra que se suele publicar.",
                "• Tabla de ecualización: cuánto hay que reforzar el grave (DSP) para",
                "   bajar F3, y cuánta potencia más pide al amplificador y al altavoz.",
                "• Distorsión: un pico aislado en un tono suele ser algo que vibra",
                "   (tornillos, la junta, el cable o el conector RCA).",
                "• Límites: el DJI Mic no está calibrado y pierde nivel por debajo",
                "   de unos 50 Hz. Las comparaciones entre medidas sí son fiables.",
            ]),
        ])
    print(f"Creado {SALIDA}")


if __name__ == "__main__":
    main()
