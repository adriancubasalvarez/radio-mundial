"""
build_maps.py — Generador de mapas de Radio Mundial.

A partir de los datos de dominio público de Natural Earth genera:

  data/mapas/mundo.bin            mapamundi con las regiones
  data/mapas/<region>.bin         mapa de cada región con sus países
  data/mapas/*_zonas.bin          rejilla de zonas táctiles de cada mapa
  data/regiones.json              índice de regiones y países
  data/regiones/<region>.json     países de cada región (lo lee el firmware)
  data/paises.txt                 código y nombre de cada país, uno por línea
  tools/preview/*.png             previsualizaciones para revisar en el PC

Formato de las imágenes: binario nativo de LVGL 8 en color indexado de
8 bits (LV_IMG_CF_INDEXED_8BIT). Cada región (en el mapamundi) o cada país
(en el mapa de región) tiene su propio índice de la paleta, así el firmware
puede resaltarlo cambiando un solo color, sin redibujar la imagen.

Uso:
  python tools/build_maps.py            genera todo
  python tools/build_maps.py --listar   solo muestra qué país va a cada región
"""

import argparse
import colorsys
import json
import math
import struct
import sys
import unicodedata
import urllib.request
from pathlib import Path

import numpy as np  # viene con Shapely
import shapely
import shapely.affinity
from shapely.geometry import MultiPolygon, Point, Polygon, shape
from shapely.ops import unary_union
from PIL import Image, ImageDraw

# ---------------------------------------------------------------------------
# Rutas
# ---------------------------------------------------------------------------
RAIZ = Path(__file__).resolve().parent.parent
TOOLS = RAIZ / "tools"
CACHE = TOOLS / "cache"
PREVIEW = TOOLS / "preview"
DATA = RAIZ / "data"
MAPAS = DATA / "mapas"
CONFIG = TOOLS / "config_regiones.json"

URL_NATURAL_EARTH = (
    "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/"
    "geojson/ne_{escala}_admin_0_countries.geojson"
)

# ---------------------------------------------------------------------------
# Índices fijos de la paleta (los demás son regiones o países)
# ---------------------------------------------------------------------------
IDX_OCEANO = 0
IDX_BORDE = 1
IDX_TIERRA_FUERA = 2
IDX_PRIMERO = 3

# Tamaño de la celda de la rejilla táctil, en píxeles
CELDA = 4
# Distancia máxima (píxeles) para asignar un toque en el mar al país más cercano
RADIO_DEDO = 12
# Islas con menos área que esto (píxeles²) no se dibujan; si el país entero
# es así de pequeño, se dibuja como un punto para que se vea
AREA_MINIMA = 6
# Países pequeños (píxeles²) que tienen prioridad en las zonas táctiles
# dentro de RADIO_PEQUENO, para que no los tape el vecino (Andorra, Mónaco...)
AREA_PEQUENO = 40
RADIO_PEQUENO = 6


# ---------------------------------------------------------------------------
# Utilidades
# ---------------------------------------------------------------------------
def hex_a_rgb(color):
    color = color.lstrip("#")
    return tuple(int(color[i:i + 2], 16) for i in (0, 2, 4))


def variar_luz(rgb, delta):
    """Aclara (delta > 0) u oscurece (delta < 0) un color."""
    h, l, s = colorsys.rgb_to_hls(*(c / 255 for c in rgb))
    l = max(0.0, min(1.0, l + delta))
    return tuple(round(c * 255) for c in colorsys.hls_to_rgb(h, l, s))


def sin_tildes(texto):
    """Clave de ordenación alfabética que ignora tildes."""
    return unicodedata.normalize("NFD", texto).encode("ascii", "ignore").decode().lower()


def descargar(escala):
    """Descarga el GeoJSON de Natural Earth si no está en la caché."""
    CACHE.mkdir(parents=True, exist_ok=True)
    archivo = CACHE / f"ne_{escala}_admin_0_countries.geojson"
    if not archivo.exists():
        url = URL_NATURAL_EARTH.format(escala=escala)
        print(f"Descargando {url}")
        urllib.request.urlretrieve(url, archivo)
    return archivo


def partes(geom):
    """Lista de polígonos de una geometría (Polygon o MultiPolygon)."""
    if isinstance(geom, Polygon):
        return [geom]
    if isinstance(geom, MultiPolygon):
        return list(geom.geoms)
    return []


def cruzar_antimeridiano(geom, lon_max):
    """Si el encuadre pasa de 180°, desplaza +360° las partes que quedan
    al otro lado (Chukotka, Fiyi, Samoa...) para que salgan seguidas."""
    if lon_max <= 180:
        return geom
    limite = lon_max - 360
    nuevas = []
    for p in partes(geom):
        if p.bounds[2] <= limite:
            p = shapely.affinity.translate(p, xoff=360)
        nuevas.append(p)
    return MultiPolygon(nuevas) if len(nuevas) > 1 else nuevas[0]


# ---------------------------------------------------------------------------
# Carga de países y asignación a regiones
# ---------------------------------------------------------------------------
def cargar_paises(escala, config):
    """Devuelve {codigo: {"nombre", "region", "geom"}} y la tierra sin región."""
    datos = json.loads(descargar(escala).read_text(encoding="utf-8"))

    por_subregion = {}
    por_codigo = {}
    for r in config["regiones"]:
        for s in r["subregiones"]:
            por_subregion[s] = r["id"]
        for c in r["incluir"]:
            por_codigo[c] = r["id"]

    paises = {}
    sin_region = []
    for f in datos["features"]:
        p = f["properties"]
        geom = shape(f["geometry"])
        codigo = p["ISO_A2_EH"]
        region = por_codigo.get(codigo) or por_subregion.get(p["SUBREGION"])
        if len(codigo) != 2 or region is None:
            sin_region.append(geom)  # se dibuja como tierra, pero no se puede tocar
            continue
        if codigo in paises:  # territorios que comparten código (p. ej. AU)
            paises[codigo]["geom"] = unary_union([paises[codigo]["geom"], geom])
        else:
            nombre = config["nombres"].get(codigo) or (p.get("NAME_ES") or p["NAME"]).strip()
            nombre_en = config.get("nombres_en", {}).get(codigo) or (p.get("NAME_EN") or p["NAME"]).strip()
            paises[codigo] = {"nombre": nombre, "nombre_en": nombre_en, "region": region, "geom": geom}
    return paises, sin_region


# ---------------------------------------------------------------------------
# Proyecciones: de (lon, lat) a píxeles
# ---------------------------------------------------------------------------
def miller(lat):
    """Proyección de Miller (como Mercator, pero sin estirar tanto los polos)."""
    return 1.25 * math.log(math.tan(math.pi / 4 + 0.4 * math.radians(lat)))


def proyeccion_mundo(cfg):
    ancho = cfg["ancho"]
    lon_min = cfg["lon_min"]
    y_max, y_min = miller(cfg["lat_max"]), miller(cfg["lat_min"])
    escala = ancho / (2 * math.pi)
    alto = round((y_max - y_min) * escala)

    def proyectar(coords):
        lon = coords[:, 0]
        lat = coords[:, 1].clip(-89, 89)
        x = (lon - lon_min) / 360 * ancho
        y = (y_max - 1.25 * np.log(np.tan(math.pi / 4 + 0.4 * lat * math.pi / 180))) * escala
        return np.stack([x, y], axis=1)

    return proyectar, ancho, alto


def proyeccion_region(encuadre, ancho, alto):
    """Equirrectangular centrada en la región, corrigiendo el ancho por la
    latitud media. El encuadre se amplía para llenar la pantalla sin deformar."""
    lon_min, lat_min, lon_max, lat_max = encuadre
    lon_c, lat_c = (lon_min + lon_max) / 2, (lat_min + lat_max) / 2
    k = math.cos(math.radians(lat_c))
    escala = min(ancho / ((lon_max - lon_min) * k), alto / (lat_max - lat_min))

    def proyectar(coords):
        x = (coords[:, 0] - lon_c) * k * escala + ancho / 2
        y = alto / 2 - (coords[:, 1] - lat_c) * escala
        return np.stack([x, y], axis=1)

    return proyectar


def a_pixeles(geom, proyectar):
    return shapely.transform(geom, proyectar)


# ---------------------------------------------------------------------------
# Dibujo
# ---------------------------------------------------------------------------
def dibujar(lienzo, geom, indice, borde=True):
    """Dibuja un país o región ya proyectado con su índice de paleta."""
    lista = partes(geom)
    grandes = [p for p in lista if p.area >= AREA_MINIMA]
    for p in grandes:
        lienzo.polygon(list(p.exterior.coords), fill=indice,
                       outline=IDX_BORDE if borde else None)
    if not grandes and lista:
        # País diminuto (Mónaco, Tuvalu...): un punto que se vea y se pueda tocar
        c = max(lista, key=lambda p: p.area).representative_point()
        lienzo.ellipse([c.x - 2, c.y - 2, c.x + 2, c.y + 2], fill=indice,
                       outline=IDX_BORDE if borde else None)


def rejilla_zonas(objetivos, ancho, alto):
    """Rejilla de CELDA×CELDA píxeles: cada celda guarda el índice de lo que
    hay debajo. Si es mar, el más cercano dentro de RADIO_DEDO; si no, 0.
    Los países pequeños mandan dentro de RADIO_PEQUENO."""
    geoms = [g for g, _ in objetivos]
    indices = [i for _, i in objetivos]
    arbol = shapely.STRtree(geoms)

    pequenos = [n for n, g in enumerate(geoms) if g.area < AREA_PEQUENO]
    arbol_pequenos = shapely.STRtree([geoms[n] for n in pequenos]) if pequenos else None

    cols, filas = math.ceil(ancho / CELDA), math.ceil(alto / CELDA)
    celdas = bytearray(cols * filas)
    for f in range(filas):
        for c in range(cols):
            punto = Point(c * CELDA + CELDA / 2, f * CELDA + CELDA / 2)
            if arbol_pequenos is not None:
                cerca = arbol_pequenos.query_nearest(punto, max_distance=RADIO_PEQUENO)
                if len(cerca):
                    celdas[f * cols + c] = indices[pequenos[cerca[0]]]
                    continue
            cerca = arbol.query_nearest(punto, max_distance=RADIO_DEDO)
            if len(cerca):
                celdas[f * cols + c] = indices[cerca[0]]

    # Garantía: todo país visible se queda al menos con un bloque de 3×3
    # celdas alrededor de su punto interior, aunque los vecinos lo tapen
    for g, indice in objetivos:
        if indice in celdas:
            continue
        p = g.representative_point()
        c0, f0 = int(p.x // CELDA), int(p.y // CELDA)
        for f in range(f0 - 1, f0 + 2):
            for c in range(c0 - 1, c0 + 2):
                if 0 <= c < cols and 0 <= f < filas:
                    celdas[f * cols + c] = indice
    return cols, filas, celdas


# ---------------------------------------------------------------------------
# Escritura de archivos
# ---------------------------------------------------------------------------
LV_IMG_CF_INDEXED_8BIT = 10


def guardar_lvgl(imagen, paleta, ruta):
    """Imagen binaria de LVGL 8: cabecera (4 bytes), paleta de 256 colores en
    formato lv_color32_t (azul, verde, rojo, alfa) y un byte por píxel."""
    ancho, alto = imagen.size
    cabecera = LV_IMG_CF_INDEXED_8BIT | (ancho << 10) | (alto << 21)
    datos = bytearray(struct.pack("<I", cabecera))
    for i in range(256):
        r, g, b = paleta[i] if i < len(paleta) else (0, 0, 0)
        datos += bytes((b, g, r, 0xFF))
    datos += imagen.tobytes()
    ruta.write_bytes(datos)
    return len(datos)


def guardar_zonas(cols, filas, celdas, ruta):
    """Zonas táctiles: 'ZN', tamaño de celda, reservado, columnas, filas
    (uint16 little-endian) y un byte por celda con el índice tocado."""
    ruta.write_bytes(b"ZN" + struct.pack("<BBHH", CELDA, 0, cols, filas) + celdas)


def guardar_preview(imagen, paleta, cols, filas, celdas, nombre):
    """PNG ampliado ×2 del mapa y otro con las zonas táctiles superpuestas."""
    PREVIEW.mkdir(parents=True, exist_ok=True)
    plana = []
    for c in paleta:
        plana.extend(c)
    color = imagen.copy()
    color.putpalette(plana + [0] * (768 - len(plana)))
    color = color.convert("RGB")
    color.resize((color.width * 2, color.height * 2), Image.NEAREST).save(PREVIEW / f"{nombre}.png")

    capa = Image.new("RGB", color.size)
    pinta = ImageDraw.Draw(capa)
    for f in range(filas):
        for c in range(cols):
            i = celdas[f * cols + c]
            if i:
                # Un tono distinto por zona (ángulo áureo) para ver bien los límites
                rgb = tuple(round(v * 255) for v in colorsys.hsv_to_rgb((i * 0.618) % 1, 0.7, 1))
                pinta.rectangle([c * CELDA, f * CELDA, (c + 1) * CELDA - 1, (f + 1) * CELDA - 1], fill=rgb)
    zonas = Image.blend(color, capa, 0.55)
    zonas.resize((zonas.width * 2, zonas.height * 2), Image.NEAREST).save(PREVIEW / f"{nombre}_zonas.png")


# ---------------------------------------------------------------------------
# Generación
# ---------------------------------------------------------------------------
def paleta_base(config):
    col = config["colores"]
    return [hex_a_rgb(col["oceano"]), hex_a_rgb(col["borde"]), hex_a_rgb(col["tierra_fuera"])]


def generar_mundo(config, paises, sin_region):
    cfg = config["mundo"]
    proyectar, ancho, alto = proyeccion_mundo(cfg)
    lon_max = cfg["lon_min"] + 360

    imagen = Image.new("P", (ancho, alto), IDX_OCEANO)
    lienzo = ImageDraw.Draw(imagen)
    paleta = paleta_base(config)

    for g in sin_region:
        dibujar(lienzo, a_pixeles(cruzar_antimeridiano(g, lon_max), proyectar), IDX_TIERRA_FUERA)

    objetivos = []
    for n, r in enumerate(config["regiones"]):
        indice = IDX_PRIMERO + n
        r["indice"] = indice
        paleta.append(hex_a_rgb(r["color"]))
        geoms = [cruzar_antimeridiano(p["geom"], lon_max) for p in paises.values() if p["region"] == r["id"]]
        region_px = a_pixeles(unary_union(geoms), proyectar)
        # Se dibuja país a país para que se vean las fronteras
        for g in sorted(geoms, key=lambda g: -g.area):
            dibujar(lienzo, a_pixeles(g, proyectar), indice)
        objetivos.append((region_px, indice))

    cols, filas, celdas = rejilla_zonas(objetivos, ancho, alto)
    MAPAS.mkdir(parents=True, exist_ok=True)
    tam = guardar_lvgl(imagen, paleta, MAPAS / "mundo.bin")
    guardar_zonas(cols, filas, celdas, MAPAS / "mundo_zonas.bin")
    guardar_preview(imagen, paleta, cols, filas, celdas, "mundo")
    print(f"mundo: {ancho}x{alto} px, {tam / 1024:.0f} KB")
    return {"imagen": "mapas/mundo.bin", "zonas": "mapas/mundo_zonas.bin", "ancho": ancho, "alto": alto}


def colorear(geoms):
    """Coloreado voraz: países vecinos reciben tonos distintos."""
    tonos = []
    for i, g in enumerate(geoms):
        vecinos = {tonos[j] for j in range(i) if g.distance(geoms[j]) < 1.5}
        tono = 0
        while tono in vecinos:
            tono += 1
        tonos.append(tono % 4)
    return tonos


def generar_region(config, region, paises, sin_region):
    ancho, alto = config["mapa_region"]["ancho"], config["mapa_region"]["alto"]
    encuadre = region["encuadre"]
    proyectar = proyeccion_region(encuadre, ancho, alto)
    lon_max = encuadre[2]

    imagen = Image.new("P", (ancho, alto), IDX_OCEANO)
    lienzo = ImageDraw.Draw(imagen)
    paleta = paleta_base(config)

    # Tierra de alrededor (otras regiones), solo como contexto
    for g in sin_region + [p["geom"] for p in paises.values() if p["region"] != region["id"]]:
        dibujar(lienzo, a_pixeles(cruzar_antimeridiano(g, lon_max), proyectar), IDX_TIERRA_FUERA)

    propios = [(c, p) for c, p in paises.items() if p["region"] == region["id"]]
    propios.sort(key=lambda cp: sin_tildes(cp[1]["nombre"]))
    geoms_px = [a_pixeles(cruzar_antimeridiano(p["geom"], lon_max), proyectar) for _, p in propios]

    base = hex_a_rgb(region["color"])
    tonos_rgb = [base, variar_luz(base, 0.10), variar_luz(base, -0.10), variar_luz(base, 0.20)]
    tonos = colorear(geoms_px)

    lista, objetivos = [], []
    for n, ((codigo, p), g, tono) in enumerate(zip(propios, geoms_px, tonos)):
        indice = IDX_PRIMERO + n
        paleta.append(tonos_rgb[tono])
        lista.append({"codigo": codigo, "nombre": p["nombre"], "en": p["nombre_en"], "indice": indice})
        objetivos.append((g, indice))

    # Grandes primero, para que los enclaves (Lesoto, San Marino...) queden encima
    for (g, indice) in sorted(objetivos, key=lambda o: -o[0].area):
        dibujar(lienzo, g, indice)

    cols, filas, celdas = rejilla_zonas(objetivos, ancho, alto)
    rid = region["id"]
    tam = guardar_lvgl(imagen, paleta, MAPAS / f"{rid}.bin")
    guardar_zonas(cols, filas, celdas, MAPAS / f"{rid}_zonas.bin")
    guardar_preview(imagen, paleta, cols, filas, celdas, rid)

    # Países que no se pueden tocar en el mapa (quedan fuera del encuadre)
    tocables = set(celdas)
    ocultos = [p["nombre"] for p, o in zip(lista, objetivos) if o[1] not in tocables]
    aviso = f" — sin zona táctil: {', '.join(ocultos)}" if ocultos else ""
    print(f"{rid}: {len(lista)} países, {tam / 1024:.0f} KB{aviso}")

    return {
        "id": rid,
        "nombre": region["nombre"],
        "nombre_en": region["nombre_en"],
        "indice": region["indice"],
        "imagen": f"mapas/{rid}.bin",
        "zonas": f"mapas/{rid}_zonas.bin",
        "paises": lista,
    }


def guardar_indices(indice):
    """Índices que lee el firmware, pequeños para no agotar la RAM de la
    placa: el completo (regiones.json, también lo usan las herramientas), un
    archivo por región con solo sus países (regiones/<id>.json) y la lista
    de nombres de país en texto, una línea por país con el código, el nombre en español y en inglés (paises.txt)."""
    compacto = {"ensure_ascii": False, "separators": (",", ":")}
    (DATA / "regiones.json").write_text(json.dumps(indice, **compacto), encoding="utf-8")
    carpeta = DATA / "regiones"
    carpeta.mkdir(parents=True, exist_ok=True)
    lineas = []
    for r in indice["regiones"]:
        (carpeta / f"{r['id']}.json").write_text(json.dumps(r["paises"], **compacto), encoding="utf-8")
        lineas += [f"{p['codigo']}\t{p['nombre']}\t{p['en']}" for p in r["paises"]]
    (DATA / "paises.txt").write_text("\n".join(lineas) + "\n", encoding="utf-8")
    print(f"Índices guardados: regiones.json, regiones/*.json ({len(indice['regiones'])}), paises.txt")


def main():
    parser = argparse.ArgumentParser(description="Genera los mapas de Radio Mundial")
    parser.add_argument("--listar", action="store_true", help="solo muestra los países de cada región")
    parser.add_argument("--solo-indices", action="store_true",
                        help="regenera solo los índices a partir del regiones.json existente")
    args = parser.parse_args()

    if args.solo_indices:
        guardar_indices(json.loads((DATA / "regiones.json").read_text(encoding="utf-8")))
        return

    config = json.loads(CONFIG.read_text(encoding="utf-8"))
    paises_50, sin_region_50 = cargar_paises("50m", config)

    if args.listar:
        for r in config["regiones"]:
            nombres = sorted((p["nombre"] for p in paises_50.values() if p["region"] == r["id"]), key=sin_tildes)
            print(f"\n{r['nombre']} ({len(nombres)}): {', '.join(nombres)}")
        return

    # El mapamundi usa la escala 1:110m (menos detalle, bordes más limpios)
    paises_110, sin_region_110 = cargar_paises("110m", config)
    mundo = generar_mundo(config, paises_110, sin_region_110)

    regiones = [generar_region(config, r, paises_50, sin_region_50) for r in config["regiones"]]

    guardar_indices({"version": 2, "mundo": mundo, "regiones": regiones})


if __name__ == "__main__":
    sys.exit(main())
