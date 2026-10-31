# Radio Mundial

**English** · [Español](README.es.md)

Desktop internet radio with a **touch world map**: tap a region, pick a country, a station… and it plays. Built for the maker community: a cheap board, no servers of its own, no API keys, and easy to install. Interface in English and Spanish.

> **Status: in development.** The PC version (simulator) is complete; it is now being brought to the boards.

## Install

**👉 https://adriancubasalvarez.github.io/radio-mundial/install/**

From Chrome or Edge on a computer: plug the board in over USB, click "Install" and, when it finishes, connect the radio to your Wi-Fi from the same page (or later, from your phone, with the QR code shown on the screen). To publish a new version: `python tools/build_release.py`.

## Features

- World map and 9 regions with large touch zones (made for fingers).
- About **3,600 tested stations** from 209 countries (up to 8,000 with a microSD card).
- **Search** with an on-screen keyboard and **13 genres** (pop, rock, news, jazz, classical…).
- **Favorites**, **random station from anywhere in the world** and the title of the song playing.
- Remembers the last station and the volume; sleep screen with a clock.
- Optional microSD card: bigger station database, your own stations (`mis_emisoras.csv`) and a backup of your favorites.
- English or Spanish: chosen the first time it starts.

## Boards

| Board | Status |
|---|---|
| **Freenove FNK0104** (ESP32-S3, 2.8" capacitive touch, ES8311 audio) | Main board — pending |
| **CYD Freenove FNK0114F** (ESP32, 2.8" resistive touch) | Testing |
| **PC simulator** (Windows, LVGL + SDL2) | Complete |

## Layout

```
src/          firmware (shared LVGL interface + one HAL layer per board)
include/      configuration (lv_conf.h, HAL, branding)
data/         maps, stations and search index (uploaded to the board)
tools/        Python scripts that build the data and the published firmware
docs/         install page (GitHub Pages) and documentation
```

The source code comments are in Spanish.

## PC simulator

Requirements: PlatformIO, MSYS2 with gcc and SDL2, and FFmpeg (`ffplay`) for sound.

```
pio run -e pc -t exec
```

## Rebuilding the data

```
python -m pip install -r tools/requirements.txt
python tools/build_maps.py
python tools/build_stations.py --probar
```

## Licenses

- **Code** (firmware, tools, install page): **GPL-3.0** — see [`LICENSE`](LICENSE).
- **Content** (manuals, 3D case, logo, images): **CC BY-NC-SA 4.0** — see [`LICENSE-CONTENIDO.md`](LICENSE-CONTENIDO.md).

## Credits

**Data**
- Maps: [Natural Earth](https://www.naturalearthdata.com/) (public domain).
- Stations: [radio-browser.info](https://www.radio-browser.info/), a free community database.

**Fonts**
- [Montserrat](https://github.com/JulietaUla/Montserrat) — SIL Open Font License 1.1.
- [Font Awesome Free](https://fontawesome.com/) 5 symbols (via LVGL) — fonts SIL OFL 1.1, icons CC BY 4.0.

**Firmware libraries**
- [LVGL](https://lvgl.io/) — MIT.
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) — FreeBSD.
- [ArduinoJson](https://arduinojson.org/) — MIT.
- [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) — GPL-3.0.
- [WiFiManager](https://github.com/tzapu/WiFiManager) — MIT.
- [Improv SDK](https://github.com/improv-wifi/sdk-cpp) — Apache-2.0.

**Web installer**
- [ESP Web Tools](https://esphome.github.io/esp-web-tools/) — Apache-2.0.

**Tools**
- [Shapely](https://github.com/shapely/shapely) (BSD), [Pillow](https://python-pillow.org/) (HPND), [NumPy](https://numpy.org/) (BSD), [anyascii](https://github.com/anyascii/anyascii) (ISC), [lv_font_conv](https://github.com/lvgl/lv_font_conv) (MIT), [SDL2](https://www.libsdl.org/) (zlib), [esptool](https://github.com/espressif/esptool) (GPL-2.0).

Inspired by the idea of exploring the world's radio on a map; name, design and code are original.

---

A project by **@Adriancubas1980** · [YouTube](https://www.youtube.com/@Adriancubas1980)
