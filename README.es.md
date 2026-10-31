# Radio Mundial

[English](README.md) · **Español**

Radio por Internet de escritorio con **mapamundi táctil**: tocas una región, eliges un país, una emisora… y suena. Pensada para la comunidad maker: placa barata, sin servidores propios ni claves de API, y fácil de instalar. Interfaz en inglés y en español.

> **Estado: en desarrollo.** La versión de PC (simulador) está completa; ahora se está pasando a las placas.

## Instalar

**👉 https://adriancubasalvarez.github.io/radio-mundial/instalar/**

Desde Chrome o Edge en un ordenador: conecta la placa por USB, pulsa «Instalar» y, al terminar, conecta la radio a tu WiFi desde la misma página (o después, desde el móvil, con el código QR que muestra la pantalla). Para publicar una versión nueva: `python tools/build_release.py`.

## Qué hace

- Mapamundi y 9 regiones con zonas táctiles grandes (pensadas para el dedo).
- Unas **3.600 emisoras comprobadas** de 209 países (hasta 8.000 con tarjeta microSD).
- **Buscador** con teclado en pantalla y **13 géneros** (pop, rock, noticias, jazz, clásica…).
- **Favoritos**, **emisora aleatoria del mundo** y título de la canción que suena.
- Recuerda la última emisora y el volumen; pantalla de reposo con reloj.
- Tarjeta microSD opcional: base de emisoras más grande, emisoras propias (`mis_emisoras.csv`) y copia de los favoritos.
- En inglés o en español: se elige al encenderla por primera vez.

## Placas

| Placa | Estado |
|---|---|
| **Freenove FNK0104** (ESP32-S3, 2,8" táctil capacitivo, audio ES8311) | Placa principal — pendiente |
| **CYD Freenove FNK0114F** (ESP32, 2,8" táctil resistivo) | En pruebas |
| **Simulador en el PC** (Windows, LVGL + SDL2) | Completo |

## Estructura

```
src/          firmware (interfaz LVGL común + capa HAL por placa)
include/      configuración (lv_conf.h, HAL, marca)
data/         mapas, emisoras e índice de búsqueda (se suben a la placa)
tools/        scripts de Python que generan los datos y el firmware publicado
docs/         página de instalación (GitHub Pages) y documentación
```

## Simulador en el PC

Requisitos: PlatformIO, MSYS2 con gcc y SDL2, y FFmpeg (`ffplay`) para el sonido.

```
pio run -e pc -t exec
```

## Regenerar los datos

```
python -m pip install -r tools/requirements.txt
python tools/build_maps.py
python tools/build_stations.py --probar
```

## Licencias

- **Código** (firmware, herramientas, página de instalación): **GPL-3.0** — ver [`LICENSE`](LICENSE).
- **Contenido** (manuales, carcasa 3D, logotipo, imágenes): **CC BY-NC-SA 4.0** — ver [`LICENSE-CONTENIDO.md`](LICENSE-CONTENIDO.md).

## Créditos

**Datos**
- Mapas: [Natural Earth](https://www.naturalearthdata.com/) (dominio público).
- Emisoras: [radio-browser.info](https://www.radio-browser.info/), base de datos comunitaria y gratuita.

**Fuentes**
- [Montserrat](https://github.com/JulietaUla/Montserrat) — SIL Open Font License 1.1.
- Símbolos de [Font Awesome Free](https://fontawesome.com/) 5 (vía LVGL) — fuentes SIL OFL 1.1, iconos CC BY 4.0.

**Librerías del firmware**
- [LVGL](https://lvgl.io/) — MIT.
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) — FreeBSD.
- [ArduinoJson](https://arduinojson.org/) — MIT.
- [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) — GPL-3.0.
- [WiFiManager](https://github.com/tzapu/WiFiManager) — MIT.
- [Improv SDK](https://github.com/improv-wifi/sdk-cpp) — Apache-2.0.

**Instalador web**
- [ESP Web Tools](https://esphome.github.io/esp-web-tools/) — Apache-2.0.

**Herramientas**
- [Shapely](https://github.com/shapely/shapely) (BSD), [Pillow](https://python-pillow.org/) (HPND), [NumPy](https://numpy.org/) (BSD), [anyascii](https://github.com/anyascii/anyascii) (ISC), [lv_font_conv](https://github.com/lvgl/lv_font_conv) (MIT), [SDL2](https://www.libsdl.org/) (zlib), [esptool](https://github.com/espressif/esptool) (GPL-2.0).

Inspirado en la idea de explorar la radio del mundo en un mapa; nombre, diseño y código son propios.

---

Un proyecto de **@Adriancubas1980** · [YouTube](https://www.youtube.com/@Adriancubas1980)
