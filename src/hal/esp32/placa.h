/**
 * Pines de cada placa. Solo se usan pines verificados:
 *
 *   PLACA_CYD  Freenove FNK0114F (CYD 2,8" ILI9341, táctil resistivo
 *              XPT2046, ESP32 sin PSRAM). Pines del repositorio oficial
 *              Freenove_ESP32_Display, probados en el contador de YouTube.
 */
#pragma once

#if defined(PLACA_CYD)

/* Pantalla ILI9341 por HSPI */
#define PIN_TFT_SCLK 14
#define PIN_TFT_MOSI 13
#define PIN_TFT_MISO 12
#define PIN_TFT_CS   15
#define PIN_TFT_DC   2
#define PIN_TFT_BL   21

/* Táctil resistivo XPT2046, por SPI software (VSPI queda para la SD) */
#define PIN_TOUCH_SCLK 25
#define PIN_TOUCH_MOSI 32
#define PIN_TOUCH_MISO 39
#define PIN_TOUCH_CS   33

/* Calibración del táctil de una FNK0114F: valores en bruto en los bordes
 * del panel en vertical (240x320); el eje Y va invertido */
#define TOUCH_X_MIN 186
#define TOUCH_X_MAX 3898
#define TOUCH_Y_MIN 3796
#define TOUCH_Y_MAX 136

/* Tarjeta microSD por VSPI */
#define PIN_SD_CS   5
#define PIN_SD_SCK  18
#define PIN_SD_MISO 19
#define PIN_SD_MOSI 23

/* Audio: DAC interno del ESP32 hacia el amplificador, que se habilita
 * poniendo AUDIO_EN a nivel bajo */
#define PIN_AUDIO_DAC 26
#define PIN_AUDIO_EN  4

#else
#error "Placa no definida: compila con -D PLACA_CYD (ver platformio.ini)"
#endif
