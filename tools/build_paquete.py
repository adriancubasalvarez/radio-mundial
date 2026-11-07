"""
Prepara el paquete de emisoras que la radio descarga desde Ajustes, sin
flashear. Ejecutarlo después de build_stations.py.

Salida (la publica GitHub Pages desde la carpeta docs/):
  docs/emisoras/emisoras.pack   todos los archivos de emisoras en uno
  docs/emisoras/version.txt     suma de control, tamaño y fecha

Formato del paquete (si cambia, cambiar también actualizar_esp32.cpp):
  una línea «RM1»; después, por cada archivo, una línea «ruta<TAB>tamaño» y
  sus bytes.

Uso:
  python tools/build_paquete.py
"""
import zlib
from datetime import date
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
DATA = RAIZ / "data"
SALIDA = RAIZ / "docs" / "emisoras"


def archivos():
    """Lo que necesita la radio: el índice, cada país y la búsqueda."""
    lista = sorted(p for p in (DATA / "emisoras").iterdir() if p.name != "indice.json")
    return lista + [DATA / "buscar.txt"]


def main():
    paquete = bytearray(b"RM1\n")
    for archivo in archivos():
        datos = archivo.read_bytes()
        ruta = archivo.relative_to(DATA).as_posix()
        paquete += f"{ruta}\t{len(datos)}\n".encode("ascii") + datos

    SALIDA.mkdir(exist_ok=True)
    (SALIDA / "emisoras.pack").write_bytes(paquete)
    suma = zlib.crc32(paquete)
    version = f"{suma:08x} {len(paquete)} {date.today().isoformat()}\n"
    (SALIDA / "version.txt").write_text(version, encoding="ascii", newline="\n")
    print(f"{len(archivos())} archivos, {len(paquete) / 1024:.0f} KB: {version.strip()}")


if __name__ == "__main__":
    main()
