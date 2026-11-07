# Script extra de PlatformIO para la placa.
# La librería de audio no manda el encabezado User-Agent y algunos servidores
# (zeno.fm, un 11 % de las emisoras) rechazan esas peticiones con un 401. No se
# puede configurar, así que aquí se activa la línea que la librería trae
# comentada, con nuestro nombre: zeno.fm rechaza también el «Mozilla/5.0» suelto.
Import("env")  # noqa: F821 (lo define PlatformIO)

from pathlib import Path

COMENTADA = '//    strcat(rqh, "User-Agent: Mozilla/5.0\\r\\n"); #363'
ACTIVA = '    strcat(rqh, "User-Agent: RadioMundial/1.0\\r\\n");'

proyecto = Path(env["PROJECT_DIR"])  # noqa: F821
archivo = proyecto / ".pio" / "libdeps" / env["PIOENV"] / "ESP32-audioI2S-master" / "src" / "Audio.cpp"  # noqa: F821

texto = archivo.read_text(encoding="utf-8", errors="surrogateescape")
if COMENTADA in texto:
    archivo.write_text(texto.replace(COMENTADA, ACTIVA), encoding="utf-8", errors="surrogateescape")
    print("Audio.cpp: añadido el encabezado User-Agent")
elif texto.count(ACTIVA) != 2:
    env.Exit("pio_audio.py: la librería de audio ha cambiado; revisar el parche")  # noqa: F821
