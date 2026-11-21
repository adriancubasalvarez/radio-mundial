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
  - el mismo borde de centrado de la tapa original, que entra en el frontal,
    y dos patas traseras en el plano de apoyo de las delanteras (5°).

Ejes (los del STEP del frontal): X = fondo (17 en la cara de la pantalla,
0 en el borde abierto donde apoya la tapa; la mochila va de 0 a -27,5),
Y = ancho (-65..65; USB-C en -Y, altavoz en +Y), Z = alto (-32,5..32,5;
patas hacia -Z, abajo).

Construcción: el interior se dibuja en unos ejes «locales» con la boca en
X = X0 y el fondo en X = X0 + FONDO, y al final se refleja (x -> X0 - x)
para ponerlo detrás del frontal. Después se añaden el borde y las patas, ya
en los ejes del frontal.

Uso (con CadQuery instalado):   python mochila_dac.py
Salida en esta carpeta: mochila_dac.step / .stl / .3mf (ya orientados para
imprimir: el fondo sobre la cama) y vistas en img/.
"""
import math
import pathlib

import cadquery as cq

AQUI = pathlib.Path(__file__).resolve().parent

# ---------------------------------------------------------------- medidas
# Contorno (el de la tapa plana original)
ANCHO, ALTO, RADIO_ESQ = 130.0, 65.0, 4.0
X0 = 18.0                  # boca de la mochila en los ejes locales (ver arriba)
FONDO = 27.5               # fondo total: batería (22) + brida por encima + holgura al frontal
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
# hacia abajo: el tubo es lo bastante grande para los dos casos)
LED_Y, LED_Z = -61.0 + 46.0, 0.0

# Tubo de luz REDONDO, del tamaño de la rejilla, entre el marco del DAC y la
# batería. Hace también de tope izquierdo de la batería.
TUBO_PARED = 1.2
_MARCO_DAC_DCHA = -(130.0 / 2 - 2.0) + 24.0 + 2 * 0.5 + 2.0   # borde del marco del DAC
_BAT_IZQ = 130.0 / 2 - 2.0 - (68.0 + 1.0)                      # donde empieza la batería
TUBO_D_EXT = _BAT_IZQ - _MARCO_DAC_DCHA                         # diámetro exterior del tubo
TUBO_R = TUBO_D_EXT / 2 - TUBO_PARED                            # radio interior
TUBO_Y = (_MARCO_DAC_DCHA + _BAT_IZQ) / 2                       # centro del tubo y de la rejilla
assert (LED_Y - TUBO_Y) ** 2 + LED_Z ** 2 < (TUBO_R - 3) ** 2, "el LED tiene que quedar dentro del tubo"

# Rejilla de salida de luz: pocos agujeros grandes en anillos, como la tapa
# original pero menos densa: 1 en el centro, 6 en el primer anillo y 12 en el
# segundo, todos dentro del tubo
REJILLA_D = 4.0
REJILLA_PASO = 5.5         # radio del primer anillo (el segundo, el doble)

# Batería: 68 x 38 x 22, entre el tubo de luz y la pared del lado del altavoz
BAT_L, BAT_A, BAT_H = 68.0, 38.0, 22.0
HOLGURA = 0.5              # por cada lado
BAT_Y = (_BAT_IZQ + (ANCHO / 2 - PARED)) / 2
BAT_Z = 0.0
ALTO_CUNA = 8.0            # topes de la cuna
PATA_CUNA = 10.0           # largo de cada brazo de los topes

# Anclajes de las bridas: un puente a cada lado de la batería por cada brida.
# La brida rodea la batería por encima y pasa por debajo de los dos puentes.
BRIDAS_Y = (-18.0, 18.0)   # posición de cada brida respecto al centro de la batería
TUNEL_ANCHO = 5.5          # a lo largo de Y: bridas de hasta 4,8 mm
TUNEL_ALTO = 2.0           # hueco bajo el techo del puente
TECHO = 1.6                # grosor del techo del puente
PUENTE_GRUESO = 3.0        # a lo largo de Z
PUENTE_LARGO = TUNEL_ANCHO + 2 * 1.6

# DAC: 32,5 (a lo largo de Z) x 24 (a lo largo de Y), con el jack en el
# extremo de arriba (+Z) y su boca contra la pared del lado del USB-C.
# Medido en la primera impresión (21-nov): el centro del jack queda a unos
# 20 mm de la cara interior de arriba y a 4 mm de la cara de abajo del PCB.
DAC_L, DAC_A = 32.5, 24.0
DAC_HOLGURA = 0.5
JACK_DESDE_BORDE = 5.0     # centro del jack desde el extremo de la placa
JACK_Z = (ALTO / 2 - PARED) - 20.0    # = 10,5: centro del jack en Z
DAC_Z = JACK_Z - (DAC_L / 2 - JACK_DESDE_BORDE)   # centro del DAC en Z
JACK_ALTURA = 4.0          # centro del jack sobre la cara de abajo del PCB
JACK_D = 6.0               # agujero redondo para la clavija
ALTO_MARCO_DAC = 2.0       # marco bajo alrededor del DAC

# Ranuras de ventilación sobre la batería: quitadas (0). La batería apenas se
# calienta (la placa carga a ~300 mA y la radio gasta ~200 mA) y la rejilla del
# LED ya deja respirar el interior. Para volver a ponerlas: RANURAS = 6.
RANURAS = 0
RANURA_L, RANURA_A = 44.0, 2.5

# Borde de centrado: el mismo marco de la tapa original (back_cover.step),
# 2 mm de alto, que entra en el frontal por dentro de las torres de los tornillos
TAPA_ORIGINAL = AQUI.parent / "pequena" / "back_cover.step"

# Patas traseras: siguen el plano de apoyo de las patas del frontal, que están
# inclinadas 5° (Z = -35,1 en X = 0,9 y Z = -36,3 en X = 14,8, medido en
# front.step; la parte baja, junto a la pantalla). Así la radio se echa 5°
# hacia atrás, como la carcasa original, y las cuatro patas apoyan con toda
# la cara. Detrás, el plano queda solo entre 0,1 y 1 mm por debajo del fondo
# de la mochila: las patas son tacos bajos en cuña.
PATA_ANGULO = 5.0                    # grados
PATA_REF_X, PATA_REF_Z = 14.8, -36.3  # un punto del plano de apoyo
PATA_Y = 40.0              # centradas a lo ancho con las patas del frontal
PATA_ANCHO, PATA_LARGO = 8.0, 10.0


def pata_z(x):
    """Altura del plano de apoyo en la coordenada X."""
    return PATA_REF_Z - math.tan(math.radians(PATA_ANGULO)) * (x - PATA_REF_X)

X1 = X0 + FONDO            # cara exterior del fondo
X_SUELO = X1 - SUELO       # cara interior del fondo
Y_PARED_IZDA = -(ANCHO / 2 - PARED)   # cara interior de la pared del lado del USB-C


def rejilla_original():
    """Posiciones (dy, dz) de los agujeros respecto al centro de la rejilla:
    1 en el centro, 6 en un anillo de radio REJILLA_PASO y 12 en otro del doble."""
    import math
    pos = [(0.0, 0.0)]
    for n, r in ((6, REJILLA_PASO), (12, 2 * REJILLA_PASO)):
        for i in range(n):
            a = 2 * math.pi * i / n + math.pi / 2   # un agujero arriba
            pos.append((r * math.cos(a), r * math.sin(a)))
    return pos


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
        # el cajeado queda a ~0,25 mm del contorno: esa lámina el laminador la
        # ignora, así que se abre el cajeado hacia la esquina (muesca limpia)
        sy, sz = (1 if y > 0 else -1), (1 if z > 0 else -1)
        muesca = (cq.Workplane("YZ", origin=(X0 + CARNE, y + sy * 5, z + sz * 5))
                  .rect(10, 10).extrude(FONDO))
        pieza = pieza.cut(paso).cut(cajeado).cut(muesca)
    return pieza


def cuna_bateria(pieza):
    """Topes de la batería sobre el fondo. En el lado del altavoz, esquinas en L
    (la pared lateral hace de tope). En el lado del tubo de luz, el propio tubo
    hace de tope y solo hay dos topes, arriba y abajo, fuera de la rejilla."""
    lx = BAT_L + 2 * HOLGURA
    lz = BAT_A + 2 * HOLGURA

    def tope(cy, cz, dy, dz):
        return (cq.Workplane("YZ", origin=(X_SUELO - ALTO_CUNA, cy, cz))
                .rect(dy, dz).extrude(ALTO_CUNA))

    for sz in (-1, 1):
        ez = BAT_Z + sz * (lz / 2 + PARED / 2)
        # lado del altavoz: esquina en L (brazo en Y y brazo en Z)
        ey = BAT_Y + (lx / 2 + PARED / 2)
        pieza = pieza.union(tope(ey - (PATA_CUNA / 2 - PARED / 2), ez, PATA_CUNA, PARED))
        pieza = pieza.union(tope(ey, ez - sz * (PATA_CUNA / 2 - PARED / 2), PARED, PATA_CUNA))
        # lado del tubo: solo el brazo en Y (arriba y abajo de la batería)
        pieza = pieza.union(tope(BAT_Y - lx / 2 + PATA_CUNA / 2, ez, PATA_CUNA, PARED))
    return pieza


def anclajes_bridas(pieza):
    """Puentes sobre el fondo a ambos lados de la batería: la brida pasa por
    debajo del techo de cada puente (túnel a lo largo de Z) y rodea la batería
    por encima. Así no se ve nada por fuera."""
    lz = BAT_A + 2 * HOLGURA
    alto = TUNEL_ALTO + TECHO
    for dy in BRIDAS_Y:
        for sz in (-1, 1):
            cz = BAT_Z + sz * (lz / 2 + 0.5 + PUENTE_GRUESO / 2)
            bloque = (cq.Workplane("YZ", origin=(X_SUELO - alto, BAT_Y + dy, cz))
                      .rect(PUENTE_LARGO, PUENTE_GRUESO).extrude(alto))
            tunel = (cq.Workplane("YZ", origin=(X_SUELO - TUNEL_ALTO, BAT_Y + dy, cz))
                     .rect(TUNEL_ANCHO, PUENTE_GRUESO + 2).extrude(TUNEL_ALTO + 0.01))
            pieza = pieza.union(bloque).cut(tunel)
    return pieza


def ventilacion(pieza):
    if RANURAS < 2:
        return pieza
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
    agujero = (cq.Workplane("XZ", origin=(0, Y_PARED_IZDA + 1, 0))
               .center(X_SUELO - JACK_ALTURA, JACK_Z)
               .circle(JACK_D / 2).extrude(PARED + 3))
    return pieza.cut(agujero)


def tubo_luz(pieza):
    """Tubo redondo del fondo al frontal, alrededor de la rejilla, y la rejilla
    en el fondo: la luz del LED sale solo por sus agujeros, todos dentro del tubo."""
    exterior = (cq.Workplane("YZ", origin=(X0 + 0.5, TUBO_Y, LED_Z))
                .circle(TUBO_R + TUBO_PARED).extrude(X_SUELO - X0 - 0.5))
    # el hueco se para en el fondo: la luz sale solo por los agujeros de la rejilla
    interior = (cq.Workplane("YZ", origin=(X0, TUBO_Y, LED_Z)).circle(TUBO_R)
                .extrude(X_SUELO - X0))
    pieza = pieza.union(exterior).cut(interior)
    agujeros = (cq.Workplane("YZ", origin=(X_SUELO - 1, 0, 0))
                .pushPoints([(TUBO_Y + dy, LED_Z + dz) for dy, dz in rejilla_original()])
                .circle(REJILLA_D / 2).extrude(SUELO + 2))
    return pieza.cut(agujeros)


def a_montaje(wp):
    """De los ejes locales a los del frontal: refleja x -> X0 - x."""
    return wp.mirror("YZ", basePointVector=(X0 / 2, 0, 0))


def borde_centrado(pieza):
    """El marco de la tapa original: la parte de back_cover.step que queda por
    debajo de su placa (x < 0 en su STEP), girada para que entre en el frontal
    (x de 0 a 2)."""
    tapa = cq.importers.importStep(str(TAPA_ORIGINAL))
    corte = cq.Workplane("YZ", origin=(-5, 0, 0)).rect(200, 200).extrude(5)
    marco = tapa.intersect(corte).mirror("YZ")
    # En la tapa original el marco se apoyaba en su placa; aquí la boca está
    # abierta y quedaría flotando a 1,7 mm de las paredes. Repisa que lo une a
    # las paredes: plana en la boca (x = 0) desde la pared hasta el borde
    # interior del marco, y en chaflán de 45° por debajo (sin soportes al
    # imprimir con la boca hacia arriba).
    hueco = marco.faces("<X").val().innerWires()[0].BoundingBox()
    iy, iz = hueco.ylen, hueco.zlen                       # 119,6 x 54,6
    py, pz = ANCHO - 2 * PARED, ALTO - 2 * PARED          # 126 x 61
    vuelo = (py - iy) / 2
    repisa = (cq.Workplane("YZ", origin=(-vuelo, 0, 0)).rect(py, pz)
              .extrude(vuelo))
    chaflan = (cq.Workplane("YZ", origin=(0, 0, 0)).rect(iy, iz)
               .workplane(offset=-vuelo - 0.01).rect(py, pz).loft())
    repisa = repisa.cut(chaflan)
    pieza = pieza.union(repisa).union(marco)
    # vuelve a abrir el paso de los tornillos a través de la repisa
    for y, z in TORNILLOS:
        paso = cq.Workplane("YZ", origin=(-vuelo - 1, y, z)).circle(D_PASO / 2).extrude(vuelo + 4)
        pieza = pieza.cut(paso)
    return pieza


def patas(pieza):
    # perfil lateral (X, Z), ya en los ejes del frontal: arriba se mete 1 mm
    # en la pared de abajo y la cara de apoyo sigue el plano inclinado
    xa, xb = -FONDO, -FONDO + PATA_LARGO
    arriba = -ALTO / 2 + 1
    perfil = [(xa, arriba), (xb, arriba), (xb, pata_z(xb)), (xa, pata_z(xa))]
    for y in (-PATA_Y, PATA_Y):
        p = (cq.Workplane("XZ", origin=(0, y, 0)).polyline(perfil).close()
             .extrude(PATA_ANCHO / 2, both=True)
             .edges("|Z").fillet(1.5))
        pieza = pieza.union(p)
    return pieza


def mochila():
    p = caja()
    p = torres(p)
    p = cuna_bateria(p)
    p = anclajes_bridas(p)
    p = ventilacion(p)
    p = dac(p)
    p = tubo_luz(p)
    p = a_montaje(p)
    p = borde_centrado(p)
    p = patas(p)
    return p


def para_imprimir(p):
    """Fondo sobre la cama (Z = 0), abierto hacia arriba con el borde arriba:
    el fondo (x = -27,5) pasa a ser la Z más baja."""
    return p.rotate((0, 0, 0), (0, 1, 0), -90)


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
