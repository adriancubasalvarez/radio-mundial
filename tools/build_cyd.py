"""
build_cyd.py — Base reducida para la CYD Freenove FNK0114F (sin PSRAM).

La CYD no tiene RAM para el cifrado de las emisoras https: con el audio y la
WiFi en marcha, el bloque libre más grande es de unos 43 KB y el SSL pide
más. Este script prepara data_cyd/ con:

  - solo los países elegidos y solo emisoras http (las más populares);
  - los mapas de data/, con las regiones sin emisoras pintadas como tierra
    de fondo en el mapamundi y quitadas de regiones.json (no se pueden tocar).

El entorno «cyd» de platformio.ini sube data_cyd/ en vez de data/.
Antes hay que haber generado data/ (build_maps.py y build_stations.py).

Uso:
  python tools/build_cyd.py                       EE. UU., Cuba, España y Francia
  python tools/build_cyd.py --paises US,MX,ES     otros países
  python tools/build_cyd.py --probar              prueba cada emisora con ffprobe
"""

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
DATA = RAIZ / "data"
DESTINO = RAIZ / "data_cyd"
PAISES = "US,CU,ES,FR"

# Paleta del .bin de LVGL: tras la cabecera (4 bytes), 256 colores de 4 bytes
# (ver guardar_lvgl en build_maps.py)
INICIO_PALETA = 4
IDX_TIERRA_FUERA = 2


def copiar_mapas():
    """Copia de data/ todo lo que no son emisoras: mapas, regiones y logos."""
    if DESTINO.exists():
        shutil.rmtree(DESTINO)
    shutil.copytree(DATA, DESTINO, ignore=shutil.ignore_patterns("emisoras", "buscar.txt"))


def regiones_con_emisoras():
    """Ids de las regiones que tienen algún país con emisoras."""
    indice = DESTINO / "emisoras" / "indice.txt"
    con_emisoras = {l.split("\t")[0] for l in indice.read_text(encoding="utf-8").splitlines() if l}
    ids = set()
    for archivo in (DESTINO / "regiones").glob("*.json"):
        paises = json.loads(archivo.read_text(encoding="utf-8"))
        if any(p["codigo"] in con_emisoras for p in paises):
            ids.add(archivo.stem)
    return ids


def recortar_regiones(ids):
    """Quita de regiones.json las regiones vacías y pinta su color del
    mapamundi como tierra de fondo. Devuelve los nombres de las que quedan."""
    ruta = DESTINO / "regiones.json"
    indice = json.loads(ruta.read_text(encoding="utf-8"))
    vacias = [r for r in indice["regiones"] if r["id"] not in ids]
    indice["regiones"] = [r for r in indice["regiones"] if r["id"] in ids]
    ruta.write_text(json.dumps(indice, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")

    mundo = DESTINO / indice["mundo"]["imagen"]
    datos = bytearray(mundo.read_bytes())
    fondo = INICIO_PALETA + IDX_TIERRA_FUERA * 4
    for r in vacias:
        pos = INICIO_PALETA + r["indice"] * 4
        datos[pos:pos + 4] = datos[fondo:fondo + 4]
    mundo.write_bytes(datos)

    for r in vacias:
        (DESTINO / "regiones" / f"{r['id']}.json").unlink(missing_ok=True)
        for clave in ("imagen", "zonas"):
            (DESTINO / r[clave]).unlink(missing_ok=True)
    return [r["nombre"] for r in indice["regiones"]]


def main():
    parser = argparse.ArgumentParser(description="Genera la base reducida de la CYD (data_cyd/)")
    parser.add_argument("--paises", default=PAISES, help=f"países separados por comas (por defecto {PAISES})")
    parser.add_argument("--max", type=int, default=30, help="máximo de emisoras por país")
    parser.add_argument("--probar", action="store_true", help="probar cada emisora con ffprobe")
    args = parser.parse_args()

    if not (DATA / "regiones.json").exists():
        sys.exit("Falta data/regiones.json: ejecuta antes tools/build_maps.py")
    copiar_mapas()
    subprocess.run([sys.executable, str(RAIZ / "tools" / "build_stations.py"), "--destino", "cyd",
                    "--paises", args.paises, "--solo-http", "--max", str(args.max)]
                   + (["--probar"] if args.probar else []), check=True)
    quedan = recortar_regiones(regiones_con_emisoras())
    tam = sum(f.stat().st_size for f in DESTINO.rglob("*") if f.is_file())
    print(f"\nRegiones en el mapa: {', '.join(quedan)}")
    print(f"data_cyd/: {tam / 1024:.0f} KB")


if __name__ == "__main__":
    main()
