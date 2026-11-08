"""Piezas de REFERENCIA para diseñar la carcasa del Radio Mundial Boombox.

No es la carcasa: son sólidos de ayuda para importarlos en SolidWorks (u otro
CAD) y modelar alrededor.
  1. huevo_cavidad       volumen interior de una cápsula (restarlo al exterior)
  2. altavoz_maniqui     altavoz de 4" simplificado (comprobar choques)
  3. amplificador_bloque placa TPA3116D2 como bloque (unidad central)

Sistema de coordenadas común a las piezas 1 y 2 (encajan solas):
  origen = centro de la cara EXTERIOR del frontal (donde apoya el aro)
  eje Z  = eje del altavoz, positivo hacia ATRÁS (hacia dentro de la caja)
  Imprimiendo con el frontal sobre la cama, Z coincide con la altura de impresión.

Uso:  python referencia.py        (necesita: pip install cadquery-ocp)
Salida: carpeta referencia/ con un .step (sólido exacto) y un .stl de cada pieza.

Cotas marcadas (*) son de la ficha de Amazon o estimadas: confírmalas con el
calibre, cambia el valor aquí y vuelve a ejecutar.
"""
import math
import pathlib

from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut, BRepAlgoAPI_Fuse
from OCP.BRepBuilderAPI import (BRepBuilderAPI_MakeEdge, BRepBuilderAPI_MakeFace,
                                BRepBuilderAPI_MakeWire, BRepBuilderAPI_Transform)
from OCP.BRepGProp import BRepGProp
from OCP.BRepMesh import BRepMesh_IncrementalMesh
from OCP.BRepPrimAPI import (BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder,
                             BRepPrimAPI_MakeRevol)
from OCP.GeomAPI import GeomAPI_Interpolate
from OCP.GProp import GProp_GProps
from OCP.gp import gp_Ax1, gp_Ax2, gp_Dir, gp_Pnt, gp_Trsf, gp_Vec
from OCP.IFSelect import IFSelect_RetDone
from OCP.STEPControl import STEPControl_AsIs, STEPControl_Writer
from OCP.StlAPI import StlAPI_Writer
from OCP.collections import HArray1_gp_Pnt

# ---------------------------------------------------------------- parámetros
# Frontal
ESPESOR_FRONTAL = 8.0      # mm, frontal macizo (8–10 recomendado)

# Cavidad en huevo (sólido de revolución alrededor de Z)
HUEVO_RADIO = 90.0         # semieje transversal: Ø180 en la parte más ancha
HUEVO_SEMIEJE = 120.0      # semieje a lo largo de Z del elipsoide base
HUEVO_CENTRO = 45.0        # distancia del centro del elipsoide a la cara INTERIOR del frontal
HUEVO_ANGULO = 45.0        # grados respecto a la vertical a partir de los cuales
                           # la trasera pasa de elipse a cono en punta (imprimible sin soportes)

# Altavoz ePathChina PS1401S (4")
ARO_DIAMETRO = 101.0       # (*) Ø exterior del aro
ARO_ESPESOR = 4.0          # (*) estimado: grosor del aro y de las orejas
AGUJEROS_CIRCULO = 115.0   # (*) distancia entre centros de agujeros opuestos
AGUJERO_DIAMETRO = 5.0     # (*) estimado
OREJA_ANCHO = 14.0         # (*) estimado
OREJAS_ANGULO = 0.0        # giro de las 4 orejas alrededor de Z (0 = arriba/abajo/lados)
MONTAJE_DIAMETRO = 93.0    # (*) PENDIENTE DE MEDIR: Ø del cuerpo bajo el aro (= agujero del frontal - 1 mm)
PROFUNDIDAD = 38.0         # (*) del frente del aro al final del imán
IMAN_DIAMETRO = 54.0       # (*)
IMAN_ALTO = 12.0           # (*) estimado de la foto de Amazon

# Amplificador TPA3116D2 (bloque envolvente)
AMPLI_LARGO = 92.0         # (*) leído del vídeo PXL_20261106_093719198
AMPLI_ANCHO = 69.0         # (*)
AMPLI_ALTO = 13.0          # (*) con condensadores; el disipador puede ser más alto

SALIDA = pathlib.Path(__file__).with_name("referencia")


# ---------------------------------------------------------------- utilidades
def revolucionar(puntos_rz, curva=None):
    """Sólido de revolución alrededor de Z a partir de un perfil cerrado (r, z).

    puntos_rz: lista de vértices del perfil (rectas entre ellos).
    curva: (indice, lista_de_puntos) para sustituir el tramo que empieza en
           'indice' por una curva suave que pasa exactamente por esos puntos.
    """
    alambre = BRepBuilderAPI_MakeWire()
    n = len(puntos_rz)
    for i in range(n):
        a, b = puntos_rz[i], puntos_rz[(i + 1) % n]
        if curva and curva[0] == i:
            pts = curva[1]
            arr = HArray1_gp_Pnt(1, len(pts))
            for k, (r, z) in enumerate(pts, start=1):
                arr.SetValue(k, gp_Pnt(r, 0, z))
            interp = GeomAPI_Interpolate(arr, False, 1e-6)
            interp.Perform()
            borde = BRepBuilderAPI_MakeEdge(interp.Curve()).Edge()
        else:
            borde = BRepBuilderAPI_MakeEdge(gp_Pnt(a[0], 0, a[1]), gp_Pnt(b[0], 0, b[1])).Edge()
        alambre.Add(borde)
    cara = BRepBuilderAPI_MakeFace(alambre.Wire()).Face()
    eje = gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1))
    return BRepPrimAPI_MakeRevol(cara, eje, 2 * math.pi).Shape()


def volumen_litros(forma):
    props = GProp_GProps()
    BRepGProp.VolumeProperties_s(forma, props)
    return props.Mass() / 1e6


def guardar(forma, nombre):
    SALIDA.mkdir(exist_ok=True)
    w = STEPControl_Writer()
    w.Transfer(forma, STEPControl_AsIs)
    if w.Write(str(SALIDA / f"{nombre}.step")) != IFSelect_RetDone:
        raise RuntimeError(f"no se pudo escribir {nombre}.step")
    BRepMesh_IncrementalMesh(forma, 0.1, False, 0.2)
    StlAPI_Writer().Write(forma, str(SALIDA / f"{nombre}.stl"))


# ---------------------------------------------------------------- 1. huevo
def huevo():
    a, c = HUEVO_RADIO, HUEVO_SEMIEJE
    z0 = ESPESOR_FRONTAL + HUEVO_CENTRO          # centro del elipsoide (Z global)
    # punto donde la pared del elipsoide alcanza el ángulo límite:
    # |dr/dz| = (a/c)·u/√(1-u²) = tan(ángulo)  →  u = t/√((a/c)² + t²)
    t = math.tan(math.radians(HUEVO_ANGULO))
    u = t / math.sqrt((a / c) ** 2 + t ** 2)
    zt, rt = c * u, a * math.sqrt(1 - u * u)     # relativos al centro
    punta = zt + rt / t                          # cono tangente hasta el eje
    # elipse desde el frontal (z = -HUEVO_CENTRO) hasta el punto de tangencia
    zi = -HUEVO_CENTRO
    curva = []
    for k in range(41):
        z = zi + (zt - zi) * k / 40
        curva.append((a * math.sqrt(1 - (z / c) ** 2), z0 + z))
    boca_r = curva[0][0]
    perfil = [(0, z0 + zi), curva[0], (rt, z0 + zt), (0, z0 + punta)]
    forma = revolucionar(perfil, curva=(1, curva))
    return forma, {
        "boca_diametro": 2 * boca_r,
        "maximo_diametro": 2 * a,
        "z_maximo": z0,
        "z_punta": z0 + punta,
        "fondo_cavidad": punta - zi,
    }


# ---------------------------------------------------------------- 2. altavoz
def altavoz():
    ra, rm, ri = ARO_DIAMETRO / 2, MONTAJE_DIAMETRO / 2, IMAN_DIAMETRO / 2
    z_iman = PROFUNDIDAD - IMAN_ALTO
    perfil = [
        (0, 0), (ra, 0), (ra, ARO_ESPESOR), (rm, ARO_ESPESOR),
        (ri, z_iman), (ri, PROFUNDIDAD), (0, PROFUNDIDAD),
    ]
    cuerpo = revolucionar(perfil)
    # una oreja en +X, con su agujero, y se copia a 90°
    rh = AGUJEROS_CIRCULO / 2
    largo = rh + AGUJERO_DIAMETRO + 2 - (ra - 6)
    oreja = BRepPrimAPI_MakeBox(gp_Pnt(ra - 6, -OREJA_ANCHO / 2, 0),
                                largo, OREJA_ANCHO, ARO_ESPESOR).Shape()
    taladro = BRepPrimAPI_MakeCylinder(
        gp_Ax2(gp_Pnt(rh, 0, -1), gp_Dir(0, 0, 1)), AGUJERO_DIAMETRO / 2, ARO_ESPESOR + 2).Shape()
    oreja = BRepAlgoAPI_Cut(oreja, taladro).Shape()
    for i in range(4):
        giro = gp_Trsf()
        giro.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)),
                         math.radians(OREJAS_ANGULO + 90 * i))
        cuerpo = BRepAlgoAPI_Fuse(cuerpo, BRepBuilderAPI_Transform(oreja, giro, True).Shape()).Shape()
    return cuerpo


# ---------------------------------------------------------------- 3. amplificador
def amplificador():
    # centrado en X e Y, apoyado en Z = 0 (cara inferior de la placa)
    return BRepPrimAPI_MakeBox(gp_Pnt(-AMPLI_LARGO / 2, -AMPLI_ANCHO / 2, 0),
                               AMPLI_LARGO, AMPLI_ANCHO, AMPLI_ALTO).Shape()


if __name__ == "__main__":
    cav, datos = huevo()
    ref = altavoz()
    amp = amplificador()
    guardar(cav, "huevo_cavidad")
    guardar(ref, "altavoz_maniqui")
    guardar(amp, "amplificador_bloque")

    v_cav, v_alt = volumen_litros(cav), volumen_litros(ref)
    # parte del altavoz que queda dentro de la cavidad (detrás del frontal)
    dentro = BRepAlgoAPI_Cut(ref, BRepPrimAPI_MakeBox(
        gp_Pnt(-200, -200, -1), 400, 400, ESPESOR_FRONTAL + 1).Shape()).Shape()
    v_dentro = volumen_litros(dentro)
    choque = volumen_litros(BRepAlgoAPI_Cut(dentro, cav).Shape())

    print(f"Cavidad: {v_cav:.2f} L brutos")
    print(f"  boca en el frontal  Ø{datos['boca_diametro']:.1f} mm")
    print(f"  Ø máximo            Ø{datos['maximo_diametro']:.0f} mm a Z={datos['z_maximo']:.0f}")
    print(f"  punta trasera       Z={datos['z_punta']:.1f} mm "
          f"(fondo interior {datos['fondo_cavidad']:.1f} mm)")
    print(f"Altavoz dentro de la caja: {v_dentro:.2f} L")
    print(f"Volumen neto aprox.: {v_cav - v_dentro:.2f} L (sin nervios ni guata)")
    print(f"Choque altavoz/cavidad: {choque * 1e6:.1f} mm³ (0 = cabe)")
    print(f"Archivos en {SALIDA}")
