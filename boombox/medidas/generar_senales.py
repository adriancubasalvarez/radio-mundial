"""Genera las señales de prueba para medir los altavoces del boombox.

Uso:  python generar_senales.py        (necesita numpy)

Crea en la carpeta senales/:
  sesion_izquierdo.wav  sesion_derecho.wav  sesion_ambos.wav
      Una sesión completa (unos 70 s) que se reproduce de una vez mientras
      el micrófono graba:
        - 2 s de silencio
        - 3 barridos exponenciales de 16 Hz a 22 kHz (8 s cada uno, con 3 s
          de silencio detrás): respuesta en frecuencia y distorsión
        - 17 tonos fijos de 30 a 190 Hz (1/6 de octava, 1,5 s cada uno):
          distorsión en graves y para oír si algo vibra o roza
      «izquierdo» y «derecho» suenan solo por un canal, para medir cada
      cápsula por separado; «ambos» suena igual por los dos.
  ruido_rosa.wav
      30 s de ruido rosa, para ajustar el volumen antes de grabar.

Todo a 48 kHz y 16 bits, a -6 dBFS de pico.
"""
import numpy as np

from comun import (AMPLITUD, AMPLITUD_TONO, CARPETA, FS, HUECO_BARRIDO, HUECO_TONO,
                   N_BARRIDOS, SILENCIO_ANTES_TONOS, SILENCIO_FINAL, SILENCIO_INICIAL,
                   T_TONO, TONOS, barrido, escribir_wav16, fundidos)


def silencio(s):
    return np.zeros(int(round(s * FS)))


def tono(f):
    n = int(round(T_TONO * FS))
    t = np.arange(n) / FS
    return AMPLITUD_TONO * np.sin(2 * np.pi * f * t) * fundidos(n, 0.02, 0.02)


def sesion():
    trozos = [silencio(SILENCIO_INICIAL)]
    for _ in range(N_BARRIDOS):
        trozos += [AMPLITUD * barrido(), silencio(HUECO_BARRIDO)]
    trozos.append(silencio(SILENCIO_ANTES_TONOS))
    for f in TONOS:
        trozos += [tono(f), silencio(HUECO_TONO)]
    trozos.append(silencio(SILENCIO_FINAL))
    return np.concatenate(trozos)


def ruido_rosa(segundos=30.0):
    """Ruido rosa (-3 dB por octava) entre 20 Hz y 20 kHz, -6 dBFS de pico."""
    n = int(segundos * FS)
    rng = np.random.default_rng(7)
    espectro = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / FS)
    filtro = np.zeros_like(f)
    banda = (f >= 20) & (f <= 20000)
    filtro[banda] = 1 / np.sqrt(f[banda])
    x = np.fft.irfft(espectro * filtro, n)
    x *= AMPLITUD / np.max(np.abs(x))
    return x * fundidos(n, 0.5, 0.5)


def main():
    carpeta = CARPETA / "senales"
    carpeta.mkdir(exist_ok=True)
    s = sesion()
    cero = np.zeros_like(s)
    for nombre, (izq, der) in {"izquierdo": (s, cero), "derecho": (cero, s),
                               "ambos": (s, s)}.items():
        ruta = carpeta / f"sesion_{nombre}.wav"
        escribir_wav16(ruta, np.stack([izq, der], axis=1))
        print(f"{ruta.name}: {len(s) / FS:.1f} s")
    r = ruido_rosa()
    escribir_wav16(carpeta / "ruido_rosa.wav", np.stack([r, r], axis=1))
    print(f"ruido_rosa.wav: {len(r) / FS:.1f} s")


if __name__ == "__main__":
    main()
