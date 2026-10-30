"""
build_stations.py — Generador de la base de emisoras de Radio Mundial.

Descarga las emisoras de la API pública de radio-browser.info (sin clave),
las filtra para que las pueda reproducir el ESP32 y genera:

  data/emisoras/XX.json      emisoras de cada país (XX = código ISO del país)
  data/emisoras/indice.json  número de emisoras de cada país

Filtros: códec MP3, AAC o AAC+; bitrate <= 192 kbps (o desconocido);
última comprobación de radio-browser correcta; sin HLS ni listas de
reproducción; sin duplicados. Se ordenan por popularidad (clics y votos)
y se guardan como máximo --max por país.

Los nombres se pasan a caracteres que tienen nuestras fuentes (Latin-1):
los de otros alfabetos se transliteran («Радио Рекорд» -> «Radio Rekord»).

Uso:
  python tools/build_stations.py              genera la base
  python tools/build_stations.py --probar     además prueba cada emisora con
                                              ffprobe y descarta las que no
                                              responden (tarda 10-20 minutos)
  python tools/build_stations.py --max 20     otro límite por país
"""

import argparse
import json
import random
import re
import shutil
import subprocess
import sys
import time
import unicodedata
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from anyascii import anyascii

# ---------------------------------------------------------------------------
# Rutas y ajustes
# ---------------------------------------------------------------------------
RAIZ = Path(__file__).resolve().parent.parent
CACHE = RAIZ / "tools" / "cache"
DATA = RAIZ / "data"
SALIDA = DATA / "emisoras"

ARCHIVO_BASE = CACHE / "radio-browser.json"
ARCHIVO_PRUEBAS = CACHE / "pruebas_emisoras.json"

AGENTE = "RadioMundial/0.1 (proyecto maker; github)"
URL_SERVIDORES = "https://all.api.radio-browser.info/json/servers"

HORAS_CACHE_BASE = 24       # no volver a descargar la base antes de esto
DIAS_CACHE_PRUEBAS = 7      # validez de una prueba con ffprobe
CODECS = {"MP3", "AAC", "AAC+"}
BITRATE_MAX = 192
MAX_POR_PAIS = 30
MAX_BYTES_NOMBRE = 63       # el firmware guarda el nombre en char[64]
MAX_BYTES_URL = 255         # y la URL en char[256]
HILOS_PRUEBA = 24
SEGUNDOS_PRUEBA = 15

# Extensiones de listas de reproducción (no son audio directo)
LISTAS = re.compile(r"\.(m3u8?|pls|asx|xspf)(\?|$)", re.IGNORECASE)


# ---------------------------------------------------------------------------
# Descarga
# ---------------------------------------------------------------------------
def pedir_json(url):
    peticion = urllib.request.Request(url, headers={"User-Agent": AGENTE})
    with urllib.request.urlopen(peticion, timeout=120) as respuesta:
        return json.loads(respuesta.read().decode("utf-8"))


def descargar_base(forzar=False):
    """Descarga todas las emisoras activas (una sola petición) y las guarda
    en la caché. Si la caché es reciente, la reutiliza."""
    CACHE.mkdir(parents=True, exist_ok=True)
    if not forzar and ARCHIVO_BASE.exists():
        horas = (time.time() - ARCHIVO_BASE.stat().st_mtime) / 3600
        if horas < HORAS_CACHE_BASE:
            print(f"Usando la base en caché ({horas:.1f} h)")
            return json.loads(ARCHIVO_BASE.read_text(encoding="utf-8"))

    # radio-browser pide elegir uno de sus servidores espejo al azar
    servidores = list({s["name"] for s in pedir_json(URL_SERVIDORES)})
    random.shuffle(servidores)
    for servidor in servidores:
        # Sin "limit" la API devuelve solo 1000 emisoras
        url = f"https://{servidor}/json/stations?hidebroken=true&limit=200000"
        try:
            print(f"Descargando emisoras de {servidor}...")
            emisoras = pedir_json(url)
            ARCHIVO_BASE.write_text(json.dumps(emisoras, ensure_ascii=False), encoding="utf-8")
            return emisoras
        except Exception as error:  # probar el siguiente servidor
            print(f"  falló ({error})")
    sys.exit("No se pudo descargar la base de ningún servidor")


# ---------------------------------------------------------------------------
# Limpieza de nombres
# ---------------------------------------------------------------------------
def es_abyad(c):
    """True si el carácter es árabe o hebreo (alfabetos sin vocales escritas)."""
    nombre = unicodedata.name(c, "")
    return nombre.startswith(("ARABIC", "HEBREW"))


def limpiar_nombre(nombre):
    """Deja solo caracteres de Latin-1; translitera los demás y quita emojis."""
    salida = []
    for c in nombre:
        categoria = unicodedata.category(c)
        if categoria.startswith("C") or categoria in ("So", "Sk"):
            salida.append(" ")  # controles, emojis y símbolos decorativos
        elif ord(c) < 0x20 or 0x7F <= ord(c) < 0xA0:
            salida.append(" ")
        elif ord(c) <= 0xFF:
            salida.append(c)
        elif es_abyad(c):
            # Árabe y hebreo no escriben vocales: la transliteración da solo
            # consonantes con signos ` y ' que se quitan para que se lea mejor
            salida.append(anyascii(c).replace("`", "").replace("'", ""))
        else:
            salida.append(anyascii(c))
    texto = re.sub(r"\s+", " ", "".join(salida))
    texto = texto.strip(" -|_.,:;*~·•")
    # Recortar sin partir un carácter de varios bytes en UTF-8
    while len(texto.encode("utf-8")) > MAX_BYTES_NOMBRE:
        texto = texto[:-1]
    return texto.strip()


def clave_nombre(nombre):
    """Clave para detectar duplicados: sin tildes, mayúsculas ni signos."""
    base = unicodedata.normalize("NFD", nombre).encode("ascii", "ignore").decode().lower()
    return re.sub(r"[^a-z0-9]", "", base)


# ---------------------------------------------------------------------------
# Filtrado y orden
# ---------------------------------------------------------------------------
def candidata(e):
    """Convierte una emisora de radio-browser al formato compacto, o None si
    no sirve para el ESP32."""
    codec = (e.get("codec") or "").upper()
    bitrate = int(e.get("bitrate") or 0)
    url = (e.get("url_resolved") or e.get("url") or "").strip()
    original = e.get("name") or ""
    nombre = limpiar_nombre(original)
    codigo = (e.get("countrycode") or "").upper()

    if codec not in CODECS or bitrate > BITRATE_MAX:
        return None
    if e.get("hls") or not e.get("lastcheckok"):
        return None
    if not url.startswith(("http://", "https://")) or LISTAS.search(url):
        return None
    if len(url.encode("utf-8")) > MAX_BYTES_URL or not nombre or len(codigo) != 2:
        return None

    # Popularidad: clics recientes y votos; si no se sabe el bitrate, puntúa menos
    puntos = int(e.get("clickcount") or 0) + int(e.get("votes") or 0) / 10
    if bitrate == 0:
        puntos *= 0.5
    # Nombres en árabe o hebreo transliterados se leen mal: mejor otras primero
    if any(es_abyad(c) for c in original):
        puntos *= 0.3
    return {"pais": codigo, "puntos": puntos, "n": nombre, "u": url, "c": codec, "b": bitrate}


def agrupar_por_pais(emisoras):
    """Filtra, ordena por popularidad y quita duplicados (misma URL o nombre)."""
    por_pais = {}
    for e in emisoras:
        c = candidata(e)
        if c is not None:
            por_pais.setdefault(c["pais"], []).append(c)

    for codigo, lista in por_pais.items():
        lista.sort(key=lambda c: -c["puntos"])
        vistas_url, vistos_nombre, unicas = set(), set(), []
        for c in lista:
            k = clave_nombre(c["n"])
            if c["u"] in vistas_url or k in vistos_nombre:
                continue
            vistas_url.add(c["u"])
            vistos_nombre.add(k)
            unicas.append(c)
        por_pais[codigo] = unicas
    return por_pais


# ---------------------------------------------------------------------------
# Prueba real con ffprobe
# ---------------------------------------------------------------------------
def cargar_pruebas():
    if ARCHIVO_PRUEBAS.exists():
        return json.loads(ARCHIVO_PRUEBAS.read_text(encoding="utf-8"))
    return {}


def probar_url(url):
    """True si ffprobe consigue leer audio MP3 o AAC de la URL."""
    orden = ["ffprobe", "-v", "error", "-rw_timeout", str((SEGUNDOS_PRUEBA - 3) * 1_000_000),
             "-user_agent", AGENTE, "-show_entries", "stream=codec_name", "-of", "csv=p=0", url]
    try:
        r = subprocess.run(orden, capture_output=True, text=True, timeout=SEGUNDOS_PRUEBA)
        return r.stdout.strip().split("\n")[0].strip() in ("mp3", "aac")
    except (subprocess.TimeoutExpired, OSError):
        return False


def filtrar_probando(por_pais, maximo):
    """Prueba las emisoras en orden de popularidad hasta tener `maximo` buenas
    por país. Los resultados se guardan en caché unos días."""
    if shutil.which("ffprobe") is None:
        sys.exit("--probar necesita ffprobe (FFmpeg) en el PATH")

    pruebas = cargar_pruebas()
    caduca = time.time() - DIAS_CACHE_PRUEBAS * 86400
    pruebas = {u: p for u, p in pruebas.items() if p["fecha"] > caduca}

    # Se prueban algunas más del máximo para cubrir las que fallen
    pendientes = []
    for lista in por_pais.values():
        for c in lista[: int(maximo * 1.5)]:
            if c["u"] not in pruebas:
                pendientes.append(c["u"])

    print(f"Probando {len(pendientes)} emisoras con ffprobe ({HILOS_PRUEBA} a la vez)...")
    hechas = 0
    with ThreadPoolExecutor(HILOS_PRUEBA) as grupo:
        for url, ok in zip(pendientes, grupo.map(probar_url, pendientes)):
            pruebas[url] = {"ok": ok, "fecha": time.time()}
            hechas += 1
            if hechas % 200 == 0:
                print(f"  {hechas}/{len(pendientes)}")
                ARCHIVO_PRUEBAS.write_text(json.dumps(pruebas), encoding="utf-8")
    ARCHIVO_PRUEBAS.write_text(json.dumps(pruebas), encoding="utf-8")

    caidas = 0
    for codigo, lista in por_pais.items():
        buenas = []
        for c in lista[: int(maximo * 1.5)]:
            if pruebas.get(c["u"], {}).get("ok"):
                buenas.append(c)
            else:
                caidas += 1
        por_pais[codigo] = buenas
    print(f"Descartadas por no responder: {caidas}")
    return por_pais


# ---------------------------------------------------------------------------
# Escritura
# ---------------------------------------------------------------------------
def paises_del_mapa():
    """Códigos de los países que aparecen en los mapas (data/regiones.json)."""
    indice = json.loads((DATA / "regiones.json").read_text(encoding="utf-8"))
    return {p["codigo"]: p["nombre"] for r in indice["regiones"] for p in r["paises"]}


def guardar(por_pais, maximo):
    SALIDA.mkdir(parents=True, exist_ok=True)
    for viejo in SALIDA.glob("*.json"):
        viejo.unlink()

    mapa = paises_del_mapa()
    conteo = {}
    total_bytes = 0
    for codigo in sorted(por_pais):
        if codigo not in mapa:
            continue  # país que no sale en nuestros mapas
        lista = [{k: c[k] for k in ("n", "u", "c", "b")} for c in por_pais[codigo][:maximo]]
        if not lista:
            continue
        # Una emisora por línea: compacto y fácil de revisar a mano
        texto = "[" + ",\n ".join(json.dumps(e, ensure_ascii=False, separators=(",", ":")) for e in lista) + "]"
        (SALIDA / f"{codigo}.json").write_text(texto, encoding="utf-8")
        conteo[codigo] = len(lista)
        total_bytes += len(texto.encode("utf-8"))

    (SALIDA / "indice.json").write_text(json.dumps(conteo, separators=(",", ":")), encoding="utf-8")

    sin = sorted(nombre for codigo, nombre in mapa.items() if codigo not in conteo)
    print(f"\nPaíses con emisoras: {len(conteo)} de {len(mapa)}")
    print(f"Emisoras guardadas: {sum(conteo.values())}  ({total_bytes / 1024:.0f} KB)")
    print(f"Países sin emisoras ({len(sin)}): {', '.join(sin)}")


def main():
    parser = argparse.ArgumentParser(description="Genera la base de emisoras de Radio Mundial")
    parser.add_argument("--probar", action="store_true", help="probar cada emisora con ffprobe")
    parser.add_argument("--max", type=int, default=MAX_POR_PAIS, help="máximo de emisoras por país")
    parser.add_argument("--descargar", action="store_true", help="descargar la base aunque haya caché")
    args = parser.parse_args()

    emisoras = descargar_base(args.descargar)
    print(f"Emisoras en radio-browser: {len(emisoras)}")

    por_pais = agrupar_por_pais(emisoras)
    print(f"Pasan los filtros: {sum(len(l) for l in por_pais.values())} en {len(por_pais)} países")

    if args.probar:
        por_pais = filtrar_probando(por_pais, args.max)

    guardar(por_pais, args.max)


if __name__ == "__main__":
    main()
