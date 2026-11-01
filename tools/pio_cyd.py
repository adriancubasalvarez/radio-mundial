# Script extra de PlatformIO para el entorno "cyd".
# La CYD no tiene RAM para las emisoras https: en vez de data/ sube la base
# reducida de data_cyd/ (se genera con tools/build_cyd.py).
Import("env")  # noqa: F821 (lo define PlatformIO)

import os

env.Replace(PROJECT_DATA_DIR=os.path.join(env.subst("$PROJECT_DIR"), "data_cyd"))  # noqa: F821
