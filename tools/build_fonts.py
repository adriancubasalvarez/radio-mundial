"""
build_fonts.py — Generador de las fuentes de Radio Mundial.

Las fuentes Montserrat que trae LVGL solo tienen caracteres ASCII (sin tildes
ni ñ). Este script genera Montserrat con el rango Latin-1 (español, francés,
alemán, portugués...) más los símbolos de LVGL (play, stop, volumen...).

Las fuentes generadas (src/fuentes/*.c) se guardan en el repositorio, así que
solo hace falta volver a ejecutarlo si se cambian tamaños o caracteres.

Requisito: Node.js (usa "npx lv_font_conv").

Uso:
  python tools/build_fonts.py
"""

import subprocess
import sys
import urllib.request
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
CACHE = RAIZ / "tools" / "cache"
SALIDA = RAIZ / "src" / "fuentes"

# Las mismas fuentes que usa LVGL para sus fuentes integradas
URL_LVGL = "https://raw.githubusercontent.com/lvgl/lvgl/v8.4.0/scripts/built_in_font/"
TEXTO = "Montserrat-Medium.ttf"
SIMBOLOS = "FontAwesome5-Solid+Brands+Regular.woff"

# Tamaños en píxeles que usa la interfaz
TAMANOS = [14, 20, 28]

# ASCII imprimible + Latin-1 (á, é, í, ó, ú, ñ, ü, ç, ß, ¿, ¡...)
RANGO_TEXTO = "0x20-0x7E,0xA0-0xFF"

# Símbolos de LVGL 8 (lista copiada de scripts/built_in_font/built_in_font_gen.py)
RANGO_SIMBOLOS = (
    "61441,61448,61451,61452,61453,61457,61459,61461,61465,61468,61473,61478,"
    "61479,61480,61502,61507,61512,61515,61516,61517,61521,61522,61523,61524,"
    "61543,61544,61550,61552,61553,61556,61559,61560,61561,61563,61587,61589,"
    "61636,61637,61639,61641,61664,61671,61674,61683,61724,61732,61787,61931,"
    "62016,62017,62018,62019,62020,62087,62099,62212,62189,62810,63426,63650"
    # Símbolos propios de Radio Mundial: estrella (favoritos) y aleatorio
    ",0xF005,0xF074"
)

# Fuente grande solo con dígitos y dos puntos para el reloj del reposo
TAM_RELOJ = 48
RANGO_RELOJ = "0x20,0x30-0x3A"

LV_FONT_CONV = "lv_font_conv@1.5.2"


def descargar(nombre):
    CACHE.mkdir(parents=True, exist_ok=True)
    archivo = CACHE / nombre
    if not archivo.exists():
        print(f"Descargando {nombre}")
        urllib.request.urlretrieve(URL_LVGL + urllib.request.quote(nombre), archivo)
    return archivo


def main():
    texto = descargar(TEXTO)
    simbolos = descargar(SIMBOLOS)
    SALIDA.mkdir(parents=True, exist_ok=True)

    trabajos = [(f"radio_{tam}.c", tam, ["--font", str(texto), "-r", RANGO_TEXTO,
                                          "--font", str(simbolos), "-r", RANGO_SIMBOLOS])
                for tam in TAMANOS]
    trabajos.append((f"radio_reloj_{TAM_RELOJ}.c", TAM_RELOJ, ["--font", str(texto), "-r", RANGO_RELOJ]))

    for nombre, tam, fuentes in trabajos:
        destino = SALIDA / nombre
        orden = ["npx", "--yes", LV_FONT_CONV,
                 "--bpp", "4", "--size", str(tam), "--no-compress", *fuentes,
                 "--format", "lvgl", "--force-fast-kern-format", "-o", str(destino)]
        print(f"Generando {destino.name}")
        subprocess.run(orden, check=True, shell=(sys.platform == "win32"))


if __name__ == "__main__":
    main()
