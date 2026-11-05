"""
build_tono.py — Genera el tono de emergencia de las alarmas (data/alarma.mp3).

Suena si a la hora de la alarma no hay WiFi o falla la emisora. Es original:
tres notas de campana sintetizadas aquí mismo, sin grabaciones de nadie.

Requisitos: ffmpeg en el PATH.

Uso:
  python tools/build_tono.py
"""

import math
import struct
import subprocess
import tempfile
import wave
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
SALIDA = RAIZ / "data" / "alarma.mp3"
MUESTREO = 44100

# (frecuencia en Hz, instante en segundos): tres notas ascendentes
NOTAS = [(659.25, 0.00), (880.00, 0.28), (1318.51, 0.56)]
DURACION = 3.0   # con silencio al final, antes de repetirse
DECAIMIENTO = 4.0


def campana(frecuencia, t):
    """Una nota de campana: la nota y dos armónicos que se apagan antes."""
    envolvente = math.exp(-DECAIMIENTO * t)
    ataque = min(t / 0.004, 1.0)  # sin chasquido al empezar
    return ataque * envolvente * (math.sin(2 * math.pi * frecuencia * t)
                                  + 0.4 * math.exp(-3 * t) * math.sin(2 * math.pi * 2 * frecuencia * t)
                                  + 0.2 * math.exp(-6 * t) * math.sin(2 * math.pi * 3 * frecuencia * t))


def main():
    muestras = []
    for i in range(int(DURACION * MUESTREO)):
        t = i / MUESTREO
        muestras.append(sum(campana(f, t - inicio) for f, inicio in NOTAS if t >= inicio))
    pico = max(abs(m) for m in muestras)
    datos = b"".join(struct.pack("<h", int(m / pico * 30000)) for m in muestras)

    with tempfile.TemporaryDirectory() as carpeta:
        wav = Path(carpeta) / "tono.wav"
        with wave.open(str(wav), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(MUESTREO)
            w.writeframes(datos)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(wav), "-c:a", "libmp3lame",
                        "-b:a", "96k", "-write_xing", "0", "-id3v2_version", "0", str(SALIDA)], check=True)
    print(f"{SALIDA}: {SALIDA.stat().st_size / 1024:.0f} KB")


if __name__ == "__main__":
    main()
