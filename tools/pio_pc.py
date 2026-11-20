# Script extra de PlatformIO para el entorno "pc" (simulador).
# Ajustes por sistema operativo:
#   - Windows: el lector de títulos ICY usa sockets de Winsock: hay que
#     enlazar ws2_32.
#   - macOS: SDL2 está en Homebrew, fuera de las rutas por defecto del
#     compilador, así que se añaden sus carpetas de include y lib.
# En Linux no hace falta nada (SDL2 suele estar en /usr).
Import("env")  # noqa: F821 (lo define PlatformIO)

import sys

if sys.platform == "win32":
    env.Append(LIBS=["ws2_32"])  # noqa: F821
elif sys.platform == "darwin":
    env.Append(CPPPATH=["/opt/homebrew/include", "/usr/local/include"])
    env.Append(LINKFLAGS=["-L/opt/homebrew/lib", "-L/usr/local/lib"])
