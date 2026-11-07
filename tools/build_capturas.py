"""
Genera las capturas de pantalla del manual con el simulador del PC, en inglés
y en español. Primero hay que compilarlo: pio run -e pc

Salida: docs/manual/img/<idioma>/<nombre>.png, al doble de tamaño (640x480).

Uso:
  python tools/build_capturas.py
"""
import json
import os
import subprocess
import sys
import tempfile
from datetime import datetime, timedelta
from pathlib import Path

from PIL import Image

RAIZ = Path(__file__).resolve().parent.parent
PROGRAMA = RAIZ / ".pio" / "build" / "pc" / "program.exe"
SALIDA = RAIZ / "docs" / "manual" / "img"
AMPLIACION = 2

ATRAS = "toca 20 15\nespera 1300"
AJUSTES = "mantener 60 15 2500\nespera 800"

# El recorrido principal: toques en la pantalla de 320x240 (ver guion_pc.h)
RECORRIDO = f"""
espera 1200
captura 02_logo
espera 3000
captura 03_mapa
toca 155 70
espera 1500
captura 04_region
toca 298 15
espera 1500
captura 05_paises
{ATRAS}
toca 110 160
espera 1500
captura 06_emisoras
toca 160 70
espera 9000
captura 07_sonando
toca 90 145
espera 500
captura 08_temporizador
toca 90 145
espera 300
toca 90 145
espera 300
toca 90 145
espera 300
toca 90 145
espera 300
{ATRAS}
{ATRAS}
{ATRAS}
captura 09_mapa_sonando
toca 254 15
espera 1200
captura 10_favoritos
toca 298 15
espera 1200
captura 11_recientes
{ATRAS}
{ATRAS}
toca 210 15
espera 1200
captura 12_buscar
{ATRAS}
{AJUSTES}
captura 13_ajustes
toca 252 62
espera 1300
captura 14_alarmas
toca 160 70
espera 1300
captura 15_alarma
toca 136 212
espera 1300
captura 16_sonido
{ATRAS}
{ATRAS}
{ATRAS}
{AJUSTES}
toca 265 22
espera 1500
captura 17_actualizar
toca 80 202
espera 1500
{AJUSTES}
toca 90 110
espera 2000
captura 18_wifi
salir
"""

PRIMER_ARRANQUE = """
espera 4500
captura 01_idioma
salir
"""

REPOSO = """
espera 9000
captura 19_reposo
espera 10500
captura 20_tiempo
espera 10500
captura 21_aire
salir
"""

ALARMA = """
espera 4500
espera 66000
captura 22_alarma_sonando
salir
"""


def emisora(pais, codigo, nombre_pais, posicion=0):
    datos = json.loads((RAIZ / "data" / "emisoras" / f"{codigo}.json").read_text(encoding="utf-8"))
    e = datos[posicion]
    return {"n": e["n"], "u": e["u"], "c": e["c"], "b": e["b"], "p": codigo, "np": nombre_pais}


def ajustes(idioma):
    """Una radio de ejemplo, sin datos de nadie."""
    espanol = idioma == "es"
    favoritos = [emisora("", "FR", "Francia" if espanol else "France"),
                 emisora("", "ES", "España" if espanol else "Spain", 2),
                 emisora("", "JP", "Japón" if espanol else "Japan"),
                 emisora("", "CU", "Cuba")]
    return {
        "volumen": 70, "reposo": 0, "reanudar": False, "luz": 2, "luz_brillo": 1, "idioma": idioma,
        "ultima": favoritos[0], "favoritos": favoritos, "recientes": favoritos[::-1],
        "alarmas": [
            {"h": 7, "m": 30, "d": 62, "a": True, "v": 85, "s": True},
            {"h": 9, "m": 0, "d": 65, "a": True, "v": 70, "s": False},
        ],
    }


def primer_arranque(datos):
    del datos["idioma"]


def con_reposo(datos):
    datos["reposo"] = 5


def con_alarma(datos):
    """Una alarma que salta durante el próximo minuto entero."""
    momento = datetime.now() + timedelta(seconds=30)
    momento += timedelta(minutes=1)
    datos["alarmas"] = [{"h": momento.hour, "m": momento.minute, "d": 0, "a": True, "v": 85}]


def generar(idioma, guion, carpeta, cambio=None):
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        ruta_ajustes = tmp / "ajustes.json"
        datos = ajustes(idioma)
        if cambio is not None:
            cambio(datos)
        ruta_ajustes.write_text(json.dumps(datos, ensure_ascii=False), encoding="utf-8")
        lineas = []
        for orden in guion.strip().splitlines():
            orden = orden.strip()
            if orden.startswith("captura "):
                orden = "captura " + (tmp / (orden.split()[1] + ".bmp")).as_posix()
            lineas.append(orden)
        ruta_guion = tmp / "guion.txt"
        ruta_guion.write_text("\n".join(lineas) + "\n", encoding="utf-8")
        entorno = dict(os.environ, RADIO_GUION=str(ruta_guion), RADIO_AJUSTES=str(ruta_ajustes),
                       RADIO_SD=str(tmp / "sin_tarjeta"),
                       SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
        subprocess.run([str(PROGRAMA)], cwd=RAIZ, env=entorno, timeout=240, stdout=subprocess.DEVNULL)
        carpeta.mkdir(parents=True, exist_ok=True)
        for bmp in sorted(tmp.glob("*.bmp")):
            imagen = Image.open(bmp).convert("RGB")
            imagen = imagen.resize((imagen.width * AMPLIACION, imagen.height * AMPLIACION), Image.NEAREST)
            imagen.save(carpeta / (bmp.stem + ".png"), optimize=True)
            print(carpeta.name, bmp.stem)


def main():
    for idioma in sys.argv[1:] or ["en", "es"]:
        carpeta = SALIDA / idioma
        generar(idioma, RECORRIDO, carpeta)
        generar(idioma, PRIMER_ARRANQUE, carpeta, primer_arranque)
        generar(idioma, REPOSO, carpeta, con_reposo)
        generar(idioma, ALARMA, carpeta, con_alarma)


if __name__ == "__main__":
    main()
