"""
build_release.py — Prepara el firmware para el instalador web.

Para cada placa:
  1. compila el firmware y la imagen de datos (mapas y emisoras) con PlatformIO;
  2. junta en UN solo archivo el arranque, la tabla de particiones, el
     firmware y los datos (esptool merge-bin), para instalarlo de una vez;
  3. escribe el manifest que lee ESP Web Tools.

Salida (la publica GitHub Pages desde la carpeta docs/):
  docs/instalar/firmware/radio-mundial-<placa>.bin
  docs/instalar/manifest-<placa>.json

Requisitos: PlatformIO (pio) y esptool (python -m pip install esptool).

Uso:
  python tools/build_release.py            todas las placas
  python tools/build_release.py cyd        solo una
"""

import csv
import json
import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
SALIDA = RAIZ / "docs" / "instalar"
BOOT_APP0 = Path.home() / ".platformio" / "packages" / "framework-arduinoespressif32" / \
    "tools" / "partitions" / "boot_app0.bin"

# Placas que se publican. «familia» es el nombre de chip que usa ESP Web Tools.
PLACAS = {
    "cyd": {
        "nombre": 'CYD 2,8" Freenove FNK0114F',
        "chip": "esp32",
        "familia": "ESP32",
        "particiones": "particiones_4mb.csv",
        "tam_flash": "4MB",
        "modo_flash": "dio",
        "frec_flash": "40m",
        "arranque": 0x1000,   # en el ESP32 clásico el arranque va en 0x1000
    },
}


def version():
    """Versión del firmware, de include/marca.h (p. ej. «v0.1» -> «0.1»)."""
    texto = (RAIZ / "include" / "marca.h").read_text(encoding="utf-8")
    return re.search(r'MARCA_VERSION\s+"v?([^"]+)"', texto).group(1)


def ejecutar(orden):
    print("  $", " ".join(str(o) for o in orden))
    subprocess.run(orden, check=True, cwd=RAIZ)


def particiones(archivo):
    """{nombre: (posición, tamaño)} de una tabla de particiones .csv"""
    resultado = {}
    with open(RAIZ / archivo, encoding="utf-8") as f:
        for fila in csv.reader(l for l in f if l.strip() and not l.startswith("#")):
            nombre, _, _, posicion, tamano = [c.strip() for c in fila[:5]]
            resultado[nombre] = (int(posicion, 0), int(tamano, 0))
    return resultado


def preparar(placa, cfg, ver):
    print(f"\n== {cfg['nombre']} ({placa}) ==")
    ejecutar(["pio", "run", "-e", placa])
    ejecutar(["pio", "run", "-e", placa, "-t", "buildfs"])

    build = RAIZ / ".pio" / "build" / placa
    tabla = particiones(cfg["particiones"])
    pos_app = tabla["app0"][0]
    pos_datos = tabla["spiffs"][0]
    pos_otadata = tabla["otadata"][0]

    SALIDA.mkdir(parents=True, exist_ok=True)
    (SALIDA / "firmware").mkdir(exist_ok=True)
    binario = SALIDA / "firmware" / f"radio-mundial-{placa}.bin"
    ejecutar([sys.executable, "-m", "esptool", "--chip", cfg["chip"], "merge-bin",
              "-o", binario,
              "--flash-mode", cfg["modo_flash"], "--flash-freq", cfg["frec_flash"],
              "--flash-size", cfg["tam_flash"],
              hex(cfg["arranque"]), build / "bootloader.bin",
              "0x8000", build / "partitions.bin",
              hex(pos_otadata), BOOT_APP0,
              hex(pos_app), build / "firmware.bin",
              hex(pos_datos), build / "littlefs.bin"])

    # ESP Web Tools detecta Improv solo: tras instalar, si la radio responde,
    # ofrece configurar la WiFi. La radio tarda unos segundos en arrancar.
    manifest = {
        "name": "Radio Mundial",
        "version": ver,
        "new_install_prompt_erase": True,
        "new_install_improv_wait_time": 15,
        "builds": [{
            "chipFamily": cfg["familia"],
            "parts": [{"path": f"firmware/{binario.name}", "offset": 0}],
        }],
    }
    (SALIDA / f"manifest-{placa}.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False),
                                                    encoding="utf-8")
    print(f"  -> {binario.relative_to(RAIZ)} ({binario.stat().st_size / 1024 / 1024:.1f} MB)")


def main():
    elegidas = sys.argv[1:] or list(PLACAS)
    ver = version()
    print(f"Radio Mundial {ver}")
    for placa in elegidas:
        if placa not in PLACAS:
            sys.exit(f"Placa desconocida: {placa} (hay: {', '.join(PLACAS)})")
        preparar(placa, PLACAS[placa], ver)


if __name__ == "__main__":
    main()
