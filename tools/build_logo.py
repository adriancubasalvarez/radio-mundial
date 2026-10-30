"""
build_logo.py — Convierte el logo del creador al formato de LVGL.

Entrada:  assets/logo.png  (cuadrado, mejor con fondo transparente; si es
          una foto de perfil cuadrada, se recorta en círculo)
Salida:   data/logo.bin       logo grande para la pantalla de arranque
          data/logo_mini.bin  logo pequeño para la cabecera
          tools/preview/logo.png  previsualización

Si no existe assets/logo.png se dibuja un logo provisional (un globo con
ondas de radio) para que todo funcione mientras tanto.

Formato: LVGL 8, color verdadero con transparencia (LV_IMG_CF_TRUE_COLOR_ALPHA)
a 16 bits: por cada píxel, RGB565 (2 bytes, little-endian) y alfa (1 byte).

Uso:
  python tools/build_logo.py
  python tools/build_logo.py --cuadrado   no recortar en círculo
"""

import argparse
import struct
from pathlib import Path

from PIL import Image, ImageDraw

RAIZ = Path(__file__).resolve().parent.parent
ENTRADA = RAIZ / "assets" / "logo.png"
DATA = RAIZ / "data"
PREVIEW = RAIZ / "tools" / "preview"

TAM_GRANDE = 96   # píxeles, pantalla de arranque
TAM_MINI = 22     # píxeles, cabecera de 30 px
LV_IMG_CF_TRUE_COLOR_ALPHA = 5

# Colores de la interfaz (los mismos que src/ui/comun.h)
FONDO = (11, 29, 51)
ACENTO = (255, 200, 69)
AZUL = (47, 111, 168)


def logo_provisional(tam=512):
    """Globo terráqueo con ondas de radio, en los colores de la interfaz."""
    img = Image.new("RGBA", (tam, tam), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c, r = tam / 2, tam * 0.30
    grosor = max(2, tam // 40)
    # Ondas de radio a ambos lados
    for k in (1, 2):
        rr = r + k * tam * 0.08
        caja = [c - rr, c - rr, c + rr, c + rr]
        d.arc(caja, 145, 215, fill=ACENTO, width=grosor)
        d.arc(caja, -35, 35, fill=ACENTO, width=grosor)
    # Globo: círculo, meridianos y paralelos
    d.ellipse([c - r, c - r, c + r, c + r], fill=AZUL, outline=ACENTO, width=grosor)
    for f in (0.45, 0.85):
        d.ellipse([c - r * f, c - r, c + r * f, c + r], outline=ACENTO, width=grosor // 2 + 1)
    d.line([c, c - r, c, c + r], fill=ACENTO, width=grosor // 2 + 1)
    for f in (-0.5, 0, 0.5):
        y = c + r * f
        ancho = (r * r - (r * f) ** 2) ** 0.5
        d.line([c - ancho, y, c + ancho, y], fill=ACENTO, width=grosor // 2 + 1)
    return img


def recortar_circulo(img):
    """Recorta en círculo con borde suave (se dibuja la máscara a 4x)."""
    tam = img.size[0]
    mascara = Image.new("L", (tam * 4, tam * 4), 0)
    ImageDraw.Draw(mascara).ellipse([0, 0, tam * 4 - 1, tam * 4 - 1], fill=255)
    mascara = mascara.resize((tam, tam), Image.LANCZOS)
    alfa = Image.composite(img.getchannel("A"), Image.new("L", (tam, tam), 0), mascara)
    img.putalpha(alfa)
    return img


def cuadrar(img):
    """Recorta al cuadrado central (por si la imagen no es cuadrada)."""
    ancho, alto = img.size
    lado = min(ancho, alto)
    x, y = (ancho - lado) // 2, (alto - lado) // 2
    return img.crop((x, y, x + lado, y + lado))


def guardar_lvgl(img, ruta):
    ancho, alto = img.size
    datos = bytearray(struct.pack("<I", LV_IMG_CF_TRUE_COLOR_ALPHA | (ancho << 10) | (alto << 21)))
    pixeles = img.tobytes()  # RGBA, 4 bytes por píxel
    for i in range(0, len(pixeles), 4):
        r, g, b, a = pixeles[i:i + 4]
        rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        datos += struct.pack("<HB", rgb565, a)
    ruta.write_bytes(datos)
    return len(datos)


def main():
    parser = argparse.ArgumentParser(description="Convierte el logo al formato de LVGL")
    parser.add_argument("--cuadrado", action="store_true", help="no recortar en círculo")
    args = parser.parse_args()

    if ENTRADA.exists():
        print(f"Usando {ENTRADA}")
        base = cuadrar(Image.open(ENTRADA).convert("RGBA"))
        circulo = not args.cuadrado
    else:
        print(f"No hay {ENTRADA}: se usa un logo provisional")
        base = logo_provisional()
        circulo = False  # ya tiene fondo transparente

    DATA.mkdir(parents=True, exist_ok=True)
    PREVIEW.mkdir(parents=True, exist_ok=True)
    for tam, nombre in ((TAM_GRANDE, "logo.bin"), (TAM_MINI, "logo_mini.bin")):
        img = base.resize((tam, tam), Image.LANCZOS)
        if circulo:
            img = recortar_circulo(img)
        print(f"{nombre}: {tam}x{tam} px, {guardar_lvgl(img, DATA / nombre) / 1024:.1f} KB")
        if tam == TAM_GRANDE:
            fondo = Image.new("RGBA", img.size, FONDO + (255,))
            Image.alpha_composite(fondo, img).resize((tam * 2, tam * 2), Image.NEAREST).save(PREVIEW / "logo.png")


if __name__ == "__main__":
    main()
