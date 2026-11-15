"""Manda comandos al DSP del ESP32 (firmware boombox/prueba_bt) por el
puerto serie USB, sin reiniciar la placa.

Necesita:  pip install pyserial

Uso:
  python dsp.py ver
  python dsp.py dsp off
  python dsp.py joroba 167 -9.8 2.1
  python dsp.py --archivo resultados/jvc_cerca_I_eq/comandos.txt
  python dsp.py                       (modo interactivo; «salir» para terminar)

  --puerto COM3   puerto serie del ESP32 (por defecto, el primero que
                  parezca un ESP32: CH340, CP210x...)
"""
import argparse
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("Falta pyserial. Instálalo con:  python -m pip install pyserial")


def buscar_puerto():
    from serial.tools import list_ports
    for p in list_ports.comports():
        texto = f"{p.description} {p.manufacturer or ''}".upper()
        if any(c in texto for c in ("CH340", "CH910", "CP210", "USB-SERIAL", "USB SERIAL", "UART")):
            return p.device
    sys.exit("No encuentro el ESP32 en ningún puerto serie. Usa --puerto COMx.")


def abrir(puerto):
    s = serial.Serial()
    s.port = puerto
    s.baudrate = 115200
    s.timeout = 0.1
    # Sin tocar DTR/RTS: si no, el ESP32 se reinicia y se pierde el Bluetooth
    s.dtr = False
    s.rts = False
    s.open()
    time.sleep(0.1)
    s.reset_input_buffer()
    return s


def enviar(s, orden, espera=0.6):
    s.write((orden.strip() + "\n").encode())
    fin = time.time() + espera
    respuesta = b""
    while time.time() < fin:
        respuesta += s.read(256)
    texto = respuesta.decode("utf-8", errors="replace").strip()
    print(f"> {orden.strip()}")
    if texto:
        print(texto)


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("orden", nargs="*")
    ap.add_argument("--puerto", default=None)
    ap.add_argument("--archivo", help="archivo con un comando por línea")
    a = ap.parse_args()
    a.puerto = a.puerto or buscar_puerto()
    try:
        s = abrir(a.puerto)
    except serial.SerialException as e:
        sys.exit(f"No puedo abrir {a.puerto}: {e}\n(¿está abierto el monitor serie en otro programa?)")
    with s:
        if a.archivo:
            for linea in open(a.archivo, encoding="utf-8"):
                if linea.strip() and not linea.startswith("#"):
                    enviar(s, linea)
        elif a.orden:
            enviar(s, " ".join(a.orden))
        else:
            print("Modo interactivo. Escribe «ayuda» o «salir».")
            while True:
                try:
                    linea = input("dsp> ")
                except EOFError:
                    break
                if linea.strip() in ("salir", "exit", "quit"):
                    break
                enviar(s, linea)


if __name__ == "__main__":
    main()
