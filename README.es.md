# Radio Mundial

[English](README.md) · **Español**

Radio por Internet de escritorio con **mapamundi táctil**: tocas una región, eliges un país, una emisora… y suena. Pensada para la comunidad maker: placa barata, sin servidores propios ni claves de API, y fácil de instalar. Interfaz en inglés y en español.

> **Estado: funciona en la Freenove FNK0104.** Faltan el manual y la guía de la carcasa 3D.

## Instalar

**👉 https://adriancubasalvarez.github.io/radio-mundial/instalar/**

**📖 Manual:** https://adriancubasalvarez.github.io/radio-mundial/manual/es/

**🖨️ Carcasa 3D:** [MakerWorld](https://makerworld.com/es/models/3568249) · [Printables](https://www.printables.com/model/1905906) (los archivos también están en [`carcasa/pequena`](carcasa/pequena))

**🛠️ Guía de montaje paso a paso (en inglés):** [Instructables](https://www.instructables.com/Radio-Mundial-a-Tiny-Cute-Internet-Radio-With-a-To/) · **🎬 Vídeo:** [YouTube](https://youtu.be/h7qwVlGDZos)

Desde Chrome o Edge en un ordenador: conecta la placa por USB, pulsa «Instalar» y, al terminar, conecta la radio a tu WiFi desde la misma página (o después, desde el móvil, con el código QR que muestra la pantalla). Para publicar una versión nueva: `python tools/build_release.py`.

## Dónde comprar la placa

**Freenove FNK0104, versión «2.8 Inch IPS Touch»** (la radio no funciona en la versión sin táctil):
[Amazon España](https://link.amazon/B0c9qnA3x) · [Amazon EE. UU.](https://link.amazon/A0fZ7EvDX)

<sub>Enlaces de afiliado: en calidad de Afiliado de Amazon, obtengo ingresos por las compras adscritas que cumplen los requisitos aplicables, sin coste extra para ti.</sub>

## Qué hace

- Mapamundi y 9 regiones con zonas táctiles grandes (pensadas para el dedo).
- Unas **3.700 emisoras comprobadas** de 207 países, probadas como las pide la placa.
- **Buscador** con teclado en pantalla y **13 géneros** (pop, rock, noticias, jazz, clásica…).
- **Favoritos**, **recientes**, **emisora aleatoria del mundo** y título de la canción que suena.
- **Hora local** del país de la emisora (la de la capital o ciudad principal en países con varias zonas).
- **Alarmas** que despiertan a la radio: emisora o tono propio, posponer 10 minutos y amanecer con el LED. También **temporizador para dormir**.
- Las emisoras que fallan seguido bajan al final de la lista y vuelven cuando suenan.
- **Actualiza las emisoras desde Ajustes**, sin flashear.
- Recuerda la última emisora y el volumen; pantalla de reposo con reloj, tiempo y calidad del aire.
- En inglés o en español: se elige al encenderla por primera vez.

## Placas

| Placa | Estado |
|---|---|
| **Freenove FNK0104** (ESP32-S3, 2,8" táctil capacitivo, audio ES8311) | Funciona: pantalla, táctil, sonido, LED, reposo, alarmas e instalador web |
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
python tools/build_zonas.py
python tools/build_stations.py --descargar --probar
python tools/build_paquete.py
```

`build_paquete.py` escribe `docs/emisoras/`, el paquete que las radios descargan desde Ajustes. Al publicarlo en `main` llega a todas las radios que lo pidan.

## Licencias

- **Código** (firmware, herramientas, página de instalación): **GPL-3.0** — ver [`LICENSE`](LICENSE).
- **Contenido** (manuales, carcasa 3D, logotipo, imágenes): **CC BY-NC-SA 4.0** — ver [`LICENSE-CONTENIDO.md`](LICENSE-CONTENIDO.md).

## Créditos

**Datos**
- Mapas: [Natural Earth](https://www.naturalearthdata.com/) (dominio público).
- Emisoras: [radio-browser.info](https://www.radio-browser.info/), base de datos comunitaria y gratuita.
- Tiempo y calidad del aire: [Open-Meteo.com](https://open-meteo.com/) (CC BY 4.0). Ubicación por la conexión a Internet: [ip-api.com](https://ip-api.com/). Los dos, gratis para uso no comercial y sin claves.

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
