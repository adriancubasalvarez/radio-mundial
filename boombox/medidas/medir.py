"""Mide desde el PC: reproduce la sesión por Bluetooth hacia el ESP32 y graba
a la vez con el receptor del micrófono conectado por USB. Al terminar,
analiza la grabación y abre el informe.

Necesita:  pip install sounddevice      (además de numpy y matplotlib)

Uso:
  python medir.py --lista
      Muestra los dispositivos de audio.
  python medir.py --nivel [--canal I] [--atenuacion 12]
      Comprueba el volumen con un barrido corto (unos 10 s). Si satura,
      sube --atenuacion (dB que se baja la señal desde el PC) y usa la
      misma en todas las medidas que vayas a comparar.
  python medir.py --nombre jvc_cerca_I [--tipo cerca] [--canal I]
      Medida completa (unos 72 s): primero un tono de 1 s para comprobar
      que suena (si no, se para y dice qué revisar); luego graba, analiza
      y abre el informe.

Opciones de dispositivos (basta un trozo del nombre):
  --salida "Radio Mundial"        altavoz Bluetooth del ESP32
  --entrada "Wireless Microphone"  receptor del DJI Mic

Antes de medir, el volumen de Windows para la salida Bluetooth al 100 %:
el nivel se ajusta con el mando del amplificador.
"""
import argparse
import os
import sys

import numpy as np

from comun import AMPLITUD, CARPETA, FS, barrido, escribir_wav_flotante
from generar_senales import sesion

try:
    import sounddevice as sd
except ImportError:
    sys.exit("Falta sounddevice. Instálalo con:  python -m pip install sounddevice")

API_PREFERIDA = "Windows DirectSound"   # nombres completos y conversión de frecuencia


def listar():
    apis = sd.query_hostapis()
    for i, d in enumerate(sd.query_devices()):
        api = apis[d["hostapi"]]["name"]
        tipo = ("entrada" if d["max_input_channels"] else "") + \
               (" salida" if d["max_output_channels"] else "")
        print(f"{i:3d}  {d['name']}  [{api}]  ({tipo.strip()})")


def buscar(trozo, entrada):
    """Índice del primer dispositivo cuyo nombre contiene «trozo»,
    prefiriendo DirectSound."""
    apis = sd.query_hostapis()
    candidatos = []
    for i, d in enumerate(sd.query_devices()):
        canales = d["max_input_channels"] if entrada else d["max_output_channels"]
        if canales and trozo.lower() in d["name"].lower():
            candidatos.append((apis[d["hostapi"]]["name"] != API_PREFERIDA, i))
    if not candidatos:
        tipo = "entrada" if entrada else "salida"
        sys.exit(f"No encuentro un dispositivo de {tipo} con «{trozo}». "
                 "Mira la lista con --lista y usa --entrada / --salida.")
    return min(candidatos)[1]


def reproducir_y_grabar(senal, salida, entrada, cola=2.0):
    """Reproduce la señal estéreo por «salida» mientras graba «entrada»."""
    canales_in = min(2, sd.query_devices(entrada)["max_input_channels"])
    trozos = []
    error = []

    def al_grabar(datos, frames, tiempo, estado):
        if estado:
            error.append(str(estado))
        trozos.append(datos.copy())

    with sd.InputStream(device=entrada, samplerate=FS, channels=canales_in,
                        dtype="float32", callback=al_grabar):
        sd.sleep(500)
        sd.play(senal.astype(np.float32), FS, device=salida, blocking=True)
        sd.sleep(int(cola * 1000))
    if error:
        print("Aviso de la tarjeta de sonido:", ", ".join(sorted(set(error))))
    return np.concatenate(trozos)


def estereo(x, canal):
    cero = np.zeros_like(x)
    izq, der = {"I": (x, cero), "D": (cero, x), "ambos": (x, x)}[canal]
    return np.stack([izq, der], axis=1)


def comprobar_nivel(salida, entrada, canal, atenuacion):
    s = np.concatenate([np.zeros(FS // 2), AMPLITUD * barrido(), np.zeros(FS // 2)])
    s *= 10 ** (-atenuacion / 20)
    print("Barrido de prueba (unos 10 s)...")
    g = reproducir_y_grabar(estereo(s, canal), salida, entrada, cola=1.0)
    pico = float(np.max(np.abs(g)))
    pico_db = 20 * np.log10(max(pico, 1e-9))
    ruido = g[: FS // 4]
    ruido_db = 20 * np.log10(max(float(np.sqrt(np.mean(ruido ** 2))), 1e-9))
    print(f"Pico: {pico_db:.1f} dBFS   ruido de fondo: {ruido_db:.1f} dBFS")
    if pico_db > -3:
        print(f"→ DEMASIADO ALTO: prueba --atenuacion {atenuacion + 10:.0f} (o baja el mando).")
    elif pico_db < -30:
        print(f"→ MUY BAJO: prueba --atenuacion {max(atenuacion - 15, 0):.0f} o sube el mando.")
    elif pico_db < -18:
        print("→ Algo bajo pero sirve. Ideal entre -12 y -6 dBFS.")
    else:
        print("→ CORRECTO. No toques el mando entre las medidas que vayas a comparar.")


LISTA = """Revisa antes de repetir:
  1. Amplificador encendido y conectado, mando en su marca.
  2. Cable de la cápsula conectado al amplificador (canal correcto).
  3. ESP32 alimentado y «Radio Mundial BT» en «Conectado – Música» en Windows.
  4. Volumen de Windows para «Radio Mundial BT» en su valor de siempre.
  5. Móvil sin Bluetooth (que no le quite la conexión al PC).
  6. Receptor y transmisor del DJI encendidos; micro a 1 cm del cono."""


def comprobar_sonido(salida, entrada, canal, atenuacion):
    """Tono de 300 Hz de 1 s: comprueba que el micrófono lo oye antes de
    grabar una medida larga. Devuelve False si no llega sonido."""
    n = FS
    t = np.arange(n) / FS
    tono = AMPLITUD * 10 ** (-atenuacion / 20) * np.sin(2 * np.pi * 300 * t)
    tono *= np.minimum(1, np.minimum(t, t[::-1]) / 0.02)
    g = reproducir_y_grabar(estereo(tono, canal), salida, entrada, cola=1.5)
    x = g[:, int(np.argmax(np.sum(g ** 2, axis=0)))]
    b = FS // 10
    bloques = x[: len(x) // b * b].reshape(-1, b)
    espectros = np.abs(np.fft.rfft(bloques * np.hanning(b), axis=1)) ** 2
    f = np.fft.rfftfreq(b, 1 / FS)
    banda = (f > 280) & (f < 320)
    nivel = 10 * np.log10(np.sum(espectros[:, banda], axis=1) + 1e-20)
    snr = float(np.max(nivel) - np.median(nivel))
    pico = 20 * np.log10(max(float(np.max(np.abs(x))), 1e-9))
    if snr < 20:
        print(f"✗ NO LLEGA SONIDO: el tono de prueba apenas se oye ({snr:.0f} dB sobre el ruido).")
        print(LISTA)
        return False
    if pico < -35:
        print(f"✗ Suena demasiado bajo (pico {pico:.1f} dBFS). Revisa el volumen de Windows para "
              "«Radio Mundial BT» (se puede quedar bajo al reconectar el Bluetooth) y el mando "
              "del amplificador, o baja --atenuacion.")
        return False
    if pico > -1:
        print(f"✗ El micrófono satura (pico {pico:.1f} dBFS): sube --atenuacion o baja el mando.")
        return False
    print(f"✓ Suena (tono {snr:.0f} dB sobre el ruido).")
    return True


def medir(nombre, tipo, canal, salida, entrada, ventana, atenuacion, comprobar=True):
    from analizar import analizar, guardar, texto_resumen

    if comprobar and not comprobar_sonido(salida, entrada, canal, atenuacion):
        sys.exit(1)

    carpeta = CARPETA / "grabaciones"
    carpeta.mkdir(exist_ok=True)
    s = sesion() * 10 ** (-atenuacion / 20)
    print(f"Midiendo «{nombre}» ({len(s) / FS:.0f} s, atenuación {atenuacion:.0f} dB). "
          "Silencio, por favor...")
    g = reproducir_y_grabar(estereo(s, canal), salida, entrada)
    ruta = carpeta / f"{nombre}.wav"
    escribir_wav_flotante(ruta, g)
    print(f"Grabación guardada en {ruta}. Analizando...")
    r = analizar(ruta, nombre, tipo, ventana, None, 1000.0)
    destino = guardar(r)
    print()
    print(texto_resumen(r))
    print(f"\nResultados en {destino}")
    if hasattr(os, "startfile"):
        os.startfile(destino / "informe.pdf")


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lista", action="store_true")
    ap.add_argument("--nivel", action="store_true")
    ap.add_argument("--nombre")
    ap.add_argument("--tipo", choices=("cerca", "lejos", "sala"), default="cerca")
    ap.add_argument("--canal", choices=("I", "D", "ambos"), default="I")
    ap.add_argument("--salida", default="Radio Mundial")
    ap.add_argument("--entrada", default="Wireless Microphone")
    ap.add_argument("--ventana", type=float, default=5.0)
    ap.add_argument("--sin-comprobar", action="store_true",
                    help="no hacer la prueba de sonido de 1 s antes de medir")
    ap.add_argument("--atenuacion", type=float, default=0.0,
                    help="dB que se baja la señal desde el PC (para no saturar el micrófono)")
    a = ap.parse_args()
    if a.lista:
        listar()
        return
    salida, entrada = buscar(a.salida, False), buscar(a.entrada, True)
    print(f"Salida:  {sd.query_devices(salida)['name']}")
    print(f"Entrada: {sd.query_devices(entrada)['name']}")
    if a.nivel:
        comprobar_nivel(salida, entrada, a.canal, a.atenuacion)
    elif a.nombre:
        medir(a.nombre, a.tipo, a.canal, salida, entrada, a.ventana, a.atenuacion,
              not a.sin_comprobar)
    else:
        ap.error("usa --lista, --nivel o --nombre")


if __name__ == "__main__":
    main()
