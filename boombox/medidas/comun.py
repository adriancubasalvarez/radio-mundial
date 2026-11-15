"""Constantes y utilidades compartidas por generar_senales.py y analizar.py.

Medidas acústicas del boombox de Radio Mundial (Fase M).
Solo necesita numpy (y matplotlib para los gráficos).
"""
import pathlib
import struct

import numpy as np

CARPETA = pathlib.Path(__file__).resolve().parent

FS = 48000                # frecuencia de muestreo de las señales (Hz)

# Barrido exponencial (método de Farina)
F1, F2 = 16.0, 22000.0    # frecuencias inicial y final (Hz)
T_BARRIDO = 8.0           # duración de cada barrido (s)
AMPLITUD = 0.5            # -6 dBFS: deja margen al Bluetooth y al DAC
FUNDIDO_INI = 0.05        # s, para no dar un golpe al cono al empezar
FUNDIDO_FIN = 0.01

# Estructura de la sesión (segundos desde el comienzo del archivo)
SILENCIO_INICIAL = 2.0
N_BARRIDOS = 3
HUECO_BARRIDO = 3.0       # silencio tras cada barrido (cola de la sala)
PERIODO_BARRIDO = T_BARRIDO + HUECO_BARRIDO
SILENCIO_ANTES_TONOS = 1.0
TONOS = [30.0 * 2 ** (k / 6) for k in range(17)]   # 30 .. 190 Hz, 1/6 de octava
T_TONO = 1.5
HUECO_TONO = 0.3
AMPLITUD_TONO = 0.5
SILENCIO_FINAL = 2.0

# Constante de tiempo del barrido exponencial
L_BARRIDO = T_BARRIDO / np.log(F2 / F1)


def inicio_tonos():
    """Segundos desde el comienzo del PRIMER barrido hasta el primer tono."""
    return N_BARRIDOS * PERIODO_BARRIDO + SILENCIO_ANTES_TONOS


def barrido():
    """Barrido exponencial de F1 a F2 con fundidos, amplitud 1."""
    n = int(round(T_BARRIDO * FS))
    t = np.arange(n) / FS
    x = np.sin(2 * np.pi * F1 * L_BARRIDO * (np.exp(t / L_BARRIDO) - 1.0))
    x *= fundidos(n, FUNDIDO_INI, FUNDIDO_FIN)
    return x


def fundidos(n, ini, fin):
    env = np.ones(n)
    ni, nf = int(ini * FS), int(fin * FS)
    if ni:
        env[:ni] = 0.5 - 0.5 * np.cos(np.pi * np.arange(ni) / ni)
    if nf:
        env[n - nf:] = 0.5 + 0.5 * np.cos(np.pi * np.arange(nf) / nf)
    return env


# ---------------------------------------------------------------- WAV

def leer_wav(ruta):
    """Lee un WAV PCM de 16/24/32 bits o de coma flotante de 32/64 bits.

    Devuelve (muestras float en [-1, 1] con forma (n, canales), fs).
    Se hace a mano para no depender de scipy: las grabadoras como el
    DJI Mic pueden guardar en 24 bits o en coma flotante.
    """
    datos = pathlib.Path(ruta).read_bytes()
    if datos[:4] not in (b"RIFF", b"RF64") or datos[8:12] != b"WAVE":
        raise ValueError(f"{ruta} no es un archivo WAV")
    pos, fmt, cuerpo = 12, None, None
    while pos + 8 <= len(datos):
        ident, tam = datos[pos:pos + 4], struct.unpack("<I", datos[pos + 4:pos + 8])[0]
        trozo = datos[pos + 8:pos + 8 + tam]
        if ident == b"fmt ":
            fmt = struct.unpack("<HHIIHH", trozo[:16])
            if fmt[0] == 0xFFFE and len(trozo) >= 26:      # WAVE_FORMAT_EXTENSIBLE
                fmt = (struct.unpack("<H", trozo[24:26])[0],) + fmt[1:]
        elif ident == b"data":
            cuerpo = trozo
        pos += 8 + tam + (tam & 1)
    if fmt is None or cuerpo is None:
        raise ValueError(f"{ruta}: WAV sin bloque fmt o data")
    formato, canales, fs, _, _, bits = fmt
    ancho = bits // 8
    n = len(cuerpo) // (ancho * canales)
    cuerpo = cuerpo[:n * ancho * canales]
    if formato == 3:                                         # coma flotante
        x = np.frombuffer(cuerpo, dtype="<f4" if bits == 32 else "<f8").astype(np.float64)
    elif formato == 1:
        if bits == 16:
            x = np.frombuffer(cuerpo, dtype="<i2") / 32768.0
        elif bits == 24:
            b = np.frombuffer(cuerpo, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
            v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
            v = np.where(v & 0x800000, v - 0x1000000, v)
            x = v / 8388608.0
        elif bits == 32:
            x = np.frombuffer(cuerpo, dtype="<i4") / 2147483648.0
        else:
            raise ValueError(f"{ruta}: {bits} bits no soportados")
    else:
        raise ValueError(f"{ruta}: formato WAV {formato} no soportado")
    return x.reshape(-1, canales), fs


def escribir_wav_flotante(ruta, x, fs=FS):
    """Escribe un WAV de coma flotante de 32 bits (para las grabaciones)."""
    x = np.asarray(x, dtype="<f4")
    if x.ndim == 1:
        x = x[:, None]
    canales = x.shape[1]
    cuerpo = x.tobytes()
    cab = b"RIFF" + struct.pack("<I", 36 + len(cuerpo)) + b"WAVE"
    cab += b"fmt " + struct.pack("<IHHIIHH", 16, 3, canales, fs, fs * canales * 4, canales * 4, 32)
    cab += b"data" + struct.pack("<I", len(cuerpo))
    pathlib.Path(ruta).write_bytes(cab + cuerpo)


def escribir_wav16(ruta, x, fs=FS):
    """Escribe un WAV PCM de 16 bits con ruido de redondeo triangular (TPDF)."""
    x = np.atleast_2d(np.asarray(x, dtype=np.float64))
    if x.shape[0] < x.shape[1]:
        x = x.T
    rng = np.random.default_rng(1234)
    tpdf = (rng.random(x.shape) - rng.random(x.shape))
    v = np.clip(np.round(x * 32767.0 + tpdf), -32768, 32767).astype("<i2")
    canales = x.shape[1]
    cuerpo = v.tobytes()
    cab = b"RIFF" + struct.pack("<I", 36 + len(cuerpo)) + b"WAVE"
    cab += b"fmt " + struct.pack("<IHHIIHH", 16, 1, canales, fs, fs * canales * 2, canales * 2, 16)
    cab += b"data" + struct.pack("<I", len(cuerpo))
    pathlib.Path(ruta).write_bytes(cab + cuerpo)
