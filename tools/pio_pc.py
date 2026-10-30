# Script extra de PlatformIO para el entorno "pc" (simulador).
# En Windows, el lector de títulos ICY usa sockets de Winsock: hay que
# enlazar ws2_32. En Linux y macOS no hace falta nada.
Import("env")  # noqa: F821 (lo define PlatformIO)

import sys

if sys.platform == "win32":
    env.Append(LIBS=["ws2_32"])  # noqa: F821
