"""
Radio Mundial con DAC y batería: tapa trasera hueca («mochila») para la
carcasa pequeña de Adrián (carcasa/pequena), que sustituye a la tapa plana.

El frontal NO cambia: la mochila se atornilla en sus mismos 4 agujeros.
Dentro lleva, de izquierda a derecha mirando por detrás desde el USB-C:
  - el DAC PCM5102A (WCMCU-5102, 32,5 x 24 mm) contra la pared del lado del
    USB-C, con la boca de su jack a ras y un agujero para el cable (así todos
    los cables salen por el mismo lado);
  - un tubo de luz ovalado detrás del LED RGB de la placa (46 mm del borde
    del USB-C), con la misma rejilla redonda de la tapa original en el fondo:
    el LED se sigue viendo por detrás;
  - la batería (2 x 18650 en paralelo, 68 x 38 x 22 mm) tumbada a lo largo,
    entre el tubo y la pared del lado del altavoz, en una cuna de esquinas;
  - ranuras de ventilación sobre la batería y dos patas traseras para que la
    radio apoye nivelada sobre la mesa.

Ejes (los del STEP del frontal): X = fondo (0 en la pantalla, 18 en la cara
trasera del frontal), Y = ancho (-65..65; USB-C en -Y, altavoz en +Y),
Z = alto (-32,5..32,5; patas hacia -Z).

Uso (con CadQuery instalado):   python mochila_dac.py
Salida en esta carpeta: mochila_dac.step / .stl / .3mf (ya orientados para
imprimir: el fondo sobre la cama) y vistas en img/.
"""
import pathlib

import cadquery as cq

AQUI = pathlib.Path(__file__).resolve().parent

# ---------------------------------------------------------------- medidas
# Contorno (el de la tapa plana original)
ANCHO, ALTO, RADIO_ESQ = 130.0, 65.0, 4.0
X0 = 18.0                  # cara trasera del frontal: aquí apoya la mochila
FONDO = 26.0               # fondo total de la mochila
PARED = 2.0                # paredes laterales
SUELO = 2.0                # pared del fondo (la que va sobre la cama)

# Tornillos al frontal (mismos agujeros que la tapa plana)
TORNILLOS = [(61.5, 29.0), (-61.5, 29.0), (61.5, -29.0), (-61.5, -29.0)]
D_TORRE = 9.0              # columna que lleva el tornillo (1,25 mm de pared alrededor del cajeado)
D_PASO = 3.4               # paso del M3
D_CAJEADO = 6.5            # cajeado para la cabeza del tornillo
CARNE = 5.0                # material bajo la cabeza: con M3 x 10 entran 5 mm en el frontal

# LED RGB de la placa (por detrás): 46 mm del borde del USB-C (la placa va de
# Y = -61 a +25) y 4 mm descentrado en vertical (no sabemos si hacia arriba o
# hacia abajo: el tubo es ovalado y cubre los dos casos)
LED_Y, LED_Z = -61.0 + 46.0, 0.0
# Tubo de luz: aprovecha todo el hueco entre el marco del DAC y la batería para
# que la luz llene casi toda la rejilla (el LED queda dentro, algo descentrado)
TUBO_PARED = 1.2
TUBO_IZQ, TUBO_DCHA = -33.5, -9.0    # bordes interiores en Y
TUBO_ALTO = 34.0                     # alto interior (Z)
TUBO_ANCHO = TUBO_DCHA - TUBO_IZQ
TUBO_Y = (TUBO_IZQ + TUBO_DCHA) / 2  # centro del tubo y de la rejilla
assert TUBO_IZQ + 2 < LED_Y < TUBO_DCHA - 2, "el LED tiene que quedar dentro del tubo"
# Rejilla de la tapa original: 55 agujeros de 3 mm en un círculo de 32 mm
REJILLA_D = 3.0

# Batería: 68 x 38 x 22 + holgura, entre el tubo de luz y la pared derecha
BAT_L, BAT_A, BAT_H = 68.0, 38.0, 22.0
HOLGURA = 1.0
BAT_Y = (TUBO_DCHA + TUBO_PARED + (ANCHO / 2 - PARED)) / 2   # centro del hueco
BAT_Z = 0.0
ALTO_CUNA = 8.0            # esquinas de la cuna
PATA_CUNA = 10.0           # largo de cada brazo de las esquinas

# DAC: 32,5 (a lo largo de Z) x 24 (a lo largo de Y), jack hacia +Y
DAC_L, DAC_A = 32.5, 24.0
DAC_HOLGURA = 0.5
DAC_Z = 0.0                # centro del DAC en Z
JACK_DESDE_BORDE = 5.0     # centro del jack desde el borde «derecho» de la placa
JACK_ALTURA = 4.0          # centro del jack sobre la cara de abajo del PCB (ajustable)
JACK_D = 8.0               # agujero para el cable
ALTO_MARCO_DAC = 2.0       # marco bajo alrededor del DAC

# Ventilación sobre la batería
RANURAS = 6
RANURA_L, RANURA_A = 44.0, 2.5

# Patas traseras: llegan a la misma altura que las del frontal (Z = -36,3)
PATA_Z = -36.3
PATA_Y = 35.0              # mismas posiciones que las patas del frontal
PATA_ANCHO, PATA_LARGO = 8.0, 10.0

X1 = X0 + FONDO            # cara exterior del fondo
X_SUELO = X1 - SUELO       # cara interior del fondo
Y_PARED_IZDA = -(ANCHO / 2 - PARED)   # cara interior de la pared del lado del USB-C


# Centros de los 55 agujeros de la tapa original (leídos de back_cover.step),
# respecto al centro de la rejilla. Es simétrica, así que vale en los dos sentidos.
_REJILLA = [
    (4.0, 0.0), (4.65, -4.51), (4.65, 4.51), (6.54, -8.65), (6.54, 8.65), (8.0, 0.0),
    (8.91, -4.59), (8.91, 4.59), (9.52, -12.09), (9.52, 12.09), (11.51, -8.49), (11.51, 8.49),
    (12.0, 0.0), (13.35, -14.55), (13.35, 14.55), (13.53, -4.7), (13.53, 4.7), (15.41, -11.09),
    (15.41, 11.09), (16.0, 0.0), (17.53, -7.61), (17.53, 7.61), (17.72, -15.84), (17.72, 15.84),
    (18.0, -3.46), (18.0, 3.46), (20.0, -12.0), (20.0, 0.0), (20.0, 12.0), (22.0, -3.46),
    (22.0, 3.46), (22.28, -15.84), (22.28, 15.84), (22.47, -7.61), (22.47, 7.61), (24.0, 0.0),
    (24.59, -11.09), (24.59, 11.09), (26.47, -4.7), (26.47, 4.7), (26.65, -14.55), (26.65, 14.55),
    (28.0, 0.0), (28.49, -8.49), (28.49, 8.49), (30.48, -12.09), (30.48, 12.09), (31.09, -4.59),
    (31.09, 4.59), (32.0, 0.0), (33.46, -8.65), (33.46, 8.65), (35.35, -4.51), (35.35, 4.51),
    (36.0, 0.0),
]


def rejilla_original():
    """Posiciones (dy, dz) de los agujeros respecto al centro de la rejilla."""
    return [(y - 20.0, z) for y, z in _REJILLA]


def caja():
    exterior = (cq.Workplane("YZ", origin=(X0, 0, 0)).rect(ANCHO, ALTO).extrude(FONDO)
                .edges("|X").fillet(RADIO_ESQ))
    hueco = (cq.Workplane("YZ", origin=(X0, 0, 0))
             .rect(ANCHO - 2 * PARED, ALTO - 2 * PARED).extrude(FONDO - SUELO)
             .edges("|X").fillet(RADIO_ESQ - PARED))
    return exterior.cut(hueco)


def torres(pieza):
    """Columnas de los tornillos, recortadas al contorno exterior (están en las
    esquinas y si no sobresaldrían), con paso del M3 y cajeado desde el fondo."""
    envolvente = (cq.Workplane("YZ", origin=(X0, 0, 0)).rect(ANCHO, ALTO).extrude(FONDO)
                  .edges("|X").fillet(RADIO_ESQ))
    for y, z in TORNILLOS:
        t = cq.Workplane("YZ", origin=(X0, y, z)).circle(D_TORRE / 2).extrude(FONDO)
        pieza = pieza.union(t.intersect(envolvente))
    for y, z in TORNILLOS:
        paso = cq.Workplane("YZ", origin=(X0, y, z)).circle(D_PASO / 2).extrude(FONDO)
        cajeado = cq.Workplane("YZ", origin=(X0 + CARNE, y, z)).circle(D_CAJEADO / 2).extrude(FONDO)
        pieza = pieza.cut(paso).cut(cajeado)
    return pieza


def cuna_bateria(pieza):
    """Cuatro esquinas en L sobre el fondo, alrededor de la batería."""
    lx = BAT_L + 2 * HOLGURA
    lz = BAT_A + 2 * HOLGURA
    for sy in (-1, 1):
        for sz in (-1, 1):
            ey = BAT_Y + sy * (lx / 2 + PARED / 2)
            ez = BAT_Z + sz * (lz / 2 + PARED / 2)
            # brazo a lo largo de Y y brazo a lo largo de Z
            for (dy, dz) in ((PATA_CUNA, PARED), (PARED, PATA_CUNA)):
                cy = ey - sy * (dy / 2 - PARED / 2)
                cz = ez - sz * (dz / 2 - PARED / 2)
                b = (cq.Workplane("YZ", origin=(X_SUELO - ALTO_CUNA, cy, cz))
                     .rect(dy, dz).extrude(ALTO_CUNA))
                pieza = pieza.union(b)
    return pieza


def ventilacion(pieza):
    paso = (BAT_A - 8) / (RANURAS - 1)
    for i in range(RANURAS):
        z = BAT_Z - (BAT_A - 8) / 2 + i * paso
        r = (cq.Workplane("YZ", origin=(X_SUELO - 1, BAT_Y, z))
             .slot2D(RANURA_L, RANURA_A).extrude(SUELO + 2))
        pieza = pieza.cut(r)
    return pieza


def dac_centro_y():
    return Y_PARED_IZDA + (DAC_A + 2 * DAC_HOLGURA) / 2


def dac(pieza):
    """Marco bajo en tres lados (el cuarto es la pared del lado del USB-C, donde
    asoma el jack) y agujero para la clavija en esa pared."""
    ly = DAC_A + 2 * DAC_HOLGURA
    lz = DAC_L + 2 * DAC_HOLGURA
    cy = dac_centro_y()
    marco = (cq.Workplane("YZ", origin=(X_SUELO - ALTO_MARCO_DAC, cy + PARED / 2, DAC_Z))
             .rect(ly + PARED, lz + 2 * PARED).extrude(ALTO_MARCO_DAC))
    hueco = (cq.Workplane("YZ", origin=(X_SUELO - ALTO_MARCO_DAC - 1, cy, DAC_Z))
             .rect(ly, lz).extrude(ALTO_MARCO_DAC + 1))
    pieza = pieza.union(marco).cut(hueco)
    # jack: el PCB apoya en el fondo con los componentes hacia el frontal
    x_jack = X_SUELO - JACK_ALTURA
    z_jack = DAC_Z - (DAC_L / 2 - JACK_DESDE_BORDE)
    agujero = (cq.Workplane("XZ", origin=(0, Y_PARED_IZDA + 1, 0)).center(x_jack, z_jack)
               .slot2D(JACK_D + 2, JACK_D, angle=0).extrude(PARED + 3))
    return pieza.cut(agujero)


def tubo_luz(pieza):
    """Tubo ovalado del fondo al frontal, detrás del LED, y la rejilla original
    en el fondo, centrada en el LED."""
    exterior = (cq.Workplane("YZ", origin=(X0 + 0.5, TUBO_Y, LED_Z))
                .rect(TUBO_ANCHO + 2 * TUBO_PARED, TUBO_ALTO + 2 * TUBO_PARED)
                .extrude(X_SUELO - X0 - 0.5).edges("|X").fillet(TUBO_PARED + 2))
    # el hueco se para en el fondo: la luz sale solo por los agujeros de la rejilla
    interior = (cq.Workplane("YZ", origin=(X0, TUBO_Y, LED_Z)).rect(TUBO_ANCHO, TUBO_ALTO)
                .extrude(X_SUELO - X0).edges("|X").fillet(2))
    pieza = pieza.union(exterior).cut(interior)
    agujeros = (cq.Workplane("YZ", origin=(X_SUELO - 1, 0, 0))
                .pushPoints([(TUBO_Y + dy, LED_Z + dz) for dy, dz in rejilla_original()])
                .circle(REJILLA_D / 2).extrude(SUELO + 2))
    return pieza.cut(agujeros)


def patas(pieza):
    for y in (-PATA_Y, PATA_Y):
        p = (cq.Workplane("XY", origin=(X1 - PATA_LARGO / 2, y, PATA_Z))
             .rect(PATA_LARGO, PATA_ANCHO).extrude(-PATA_Z - ALTO / 2 + 1)
             .edges("|Z").fillet(1.5))
        pieza = pieza.union(p)
    return pieza


def mochila():
    p = caja()
    p = torres(p)
    p = cuna_bateria(p)
    p = ventilacion(p)
    p = dac(p)
    p = tubo_luz(p)
    p = patas(p)
    return p


def para_imprimir(p):
    """Fondo sobre la cama (Z = 0), abierto hacia arriba."""
    return p.rotate((0, 0, 0), (0, 1, 0), -90).translate((0, 0, 0))


if __name__ == "__main__":
    pieza = mochila()
    cq.exporters.export(pieza, str(AQUI / "mochila_dac_montaje.step"))  # en ejes del frontal
    imp = para_imprimir(pieza)
    bb = imp.val().BoundingBox()
    imp = imp.translate((0, 0, -bb.zmin))
    cq.exporters.export(imp, str(AQUI / "mochila_dac.step"))
    cq.exporters.export(imp, str(AQUI / "mochila_dac.stl"), tolerance=0.05, angularTolerance=0.1)
    cq.exporters.export(imp, str(AQUI / "mochila_dac.3mf"), tolerance=0.05, angularTolerance=0.1)
    bb = pieza.val().BoundingBox()
    print(f"Mochila: {bb.xlen:.1f} x {bb.ylen:.1f} x {bb.zlen:.1f} mm, "
          f"volumen {pieza.val().Volume() / 1000:.1f} cm3")
