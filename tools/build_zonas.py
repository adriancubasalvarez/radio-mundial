"""
Genera src/zonas.h: la zona horaria de cada país, con sus reglas de verano,
para enseñar la hora local de la emisora que suena.

Cada emisora solo sabe de qué país es, así que los países con varias zonas
usan la de su capital o la de su ciudad más poblada (ver ZONA_PRINCIPAL).

La regla POSIX solo describe el patrón de siempre. Si un país tiene una
excepción ya decidida (Cuba sigue en horario de verano hasta noviembre de
2027), la tabla lleva además una regla fija y la fecha en que termina. Conviene
regenerar el archivo de vez en cuando, con tzdata al día.

Uso:
  python tools/build_zonas.py
"""
import importlib.resources
import struct
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from zoneinfo import ZoneInfo

RAIZ = Path(__file__).resolve().parent.parent
PAISES = RAIZ / "data" / "paises.txt"
DESTINO = RAIZ / "src" / "zonas.h"

# Países cuya primera zona en la base de datos no es la de la capital
ZONA_PRINCIPAL = {
    "AU": "Australia/Sydney",
    "BR": "America/Sao_Paulo",
    "CA": "America/Toronto",
    "ID": "Asia/Jakarta",
    "KZ": "Asia/Almaty",
    "MX": "America/Mexico_City",
    "RU": "Europe/Moscow",
    "US": "America/New_York",
    "XK": "Europe/Belgrade",  # Kosovo no tiene zona propia en la base de datos
}


MAXIMO_EXCEPCION_S = 5 * 365 * 24 * 3600


def carpeta_zonas():
    return importlib.resources.files("tzdata") / "zoneinfo"


def zonas_por_pais():
    """Primera zona de cada país en zone.tab, la tabla oficial de la base."""
    zonas = {}
    for linea in (carpeta_zonas() / "zone.tab").read_text(encoding="utf-8").splitlines():
        if linea and not linea.startswith("#"):
            codigo, _, zona = linea.split("\t")[:3]
            zonas.setdefault(codigo, zona)
    return zonas


def regla_posix(zona):
    """Regla de la zona en formato POSIX: es la última línea de su archivo."""
    archivo = carpeta_zonas().joinpath(*zona.split("/"))
    return archivo.read_bytes().rstrip(b"\n").rsplit(b"\n", 1)[-1].decode("ascii")


def ultimo_cambio(zona):
    """Fecha (segundos Unix) del último cambio escrito en el archivo de la zona."""
    datos = carpeta_zonas().joinpath(*zona.split("/")).read_bytes()
    cabecera = ">4sc15x6I"
    isut, isstd, leap, tiempos, tipos, letras = struct.unpack(cabecera, datos[:44])[2:]
    # El primer bloque es de 32 bits; el de 64 bits viene detrás
    inicio = 44 + tiempos * 5 + tipos * 6 + letras + leap * 8 + isstd + isut
    tiempos = struct.unpack(cabecera, datos[inicio:inicio + 44])[5]
    if tiempos == 0:
        return 0
    return struct.unpack_from(">q", datos, inicio + 44 + (tiempos - 1) * 8)[0]


def regla_fija(zona):
    """Regla POSIX sin cambios de hora con la hora que rige ahora en la zona."""
    ahora = datetime.now(timezone.utc).astimezone(ZoneInfo(zona))
    minutos = -int(ahora.utcoffset().total_seconds()) // 60
    nombre = ahora.tzname()
    nombre = nombre if nombre.isalpha() else f"<{nombre}>"
    signo = "-" if minutos < 0 else ""
    horas, resto = divmod(abs(minutos), 60)
    return f"{nombre}{signo}{horas}" + (f":{resto:02d}" if resto else "")


def fila_c(codigo, zona):
    """Una fila de la tabla: {código, regla} o, con excepción, {código, regla
    fija, fecha de fin, regla de después}."""
    regla = regla_posix(zona)
    fin = ultimo_cambio(zona)
    # Palestina tiene cambios escritos hasta 2086 (dependen del Ramadán): eso no
    # es una excepción pasajera, y una regla fija la dejaría sin cambios de hora
    if time.time() < fin < time.time() + MAXIMO_EXCEPCION_S:
        print(f"  {codigo}: regla temporal hasta {datetime.fromtimestamp(fin, timezone.utc):%Y-%m-%d}")
        return f'    {{"{codigo}", "{regla_fija(zona)}", {fin}, "{regla}"}},'
    return f'    {{"{codigo}", "{regla}"}},'


def main():
    zonas = zonas_por_pais() | ZONA_PRINCIPAL
    codigos = sorted(l.split("\t")[0] for l in PAISES.read_text(encoding="utf-8").splitlines() if l.strip())
    filas = []
    faltan = []
    for codigo in codigos:
        if codigo in zonas:
            filas.append(fila_c(codigo, zonas[codigo]))
        else:
            faltan.append(codigo)
    if faltan:
        sys.exit(f"Países sin zona: {' '.join(faltan)}")

    lineas = [
        "/* Generado por tools/build_zonas.py: no editar a mano.",
        " * Zona horaria de cada país (regla POSIX), ordenada por código. */",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "typedef struct {",
        "    char codigo[3];",
        "    const char *regla;",
        "    uint32_t hasta;       /* si hay excepción, cuándo acaba (segundos Unix) */",
        "    const char *despues;  /* y la regla que sigue */",
        "} ZonaPais;",
        "",
        "static const ZonaPais ZONAS[] = {",
    ]
    lineas += filas
    lineas += ["};", ""]
    DESTINO.write_text("\n".join(lineas), encoding="utf-8", newline="\n")
    print(f"{len(filas)} países en {DESTINO.relative_to(RAIZ)}")


if __name__ == "__main__":
    main()
