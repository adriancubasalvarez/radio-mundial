/**
 * Pines de cada placa. Solo se usan pines verificados:
 *
 *   PLACA_CYD  Freenove FNK0114F (CYD 2,8" ILI9341, táctil resistivo
 *              XPT2046, ESP32 sin PSRAM). Pines del repositorio oficial
 *              Freenove_ESP32_Display, probados en el contador de YouTube.
 *
 *   PLACA_FNK0104  Freenove FNK0104 (ESP32-S3 con 8 MB de PSRAM y 16 MB de
 *              flash, 2,8" ILI9341, táctil capacitivo FT6336). Pines del
 *              esquema 2.8inch_ESP32-S3_Display_Schematic.pdf y de los
 *              ejemplos oficiales (repositorio Freenove_ESP32_S3_Display).
 */
#pragma once

#if defined(PLACA_CYD)

/* Pantalla ILI9341 por HSPI */
#define TFT_SPI_HOST SPI2_HOST
#define TFT_INVERTIR false
/* Giro de la pantalla para que se vea derecha con el USB a la izquierda */
#define TFT_ROTACION 1
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
/* Aviso de toque del XPT2046 (PENIRQ, a nivel bajo al tocar; resistencia de
 * 10K a 3,3 V en la placa). Es un pin RTC: puede despertar al ESP32. */
#define PIN_TOUCH_IRQ  36

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

/* LED RGB de ánodo común (luce con el pin a nivel bajo), en la cara de atrás
 * de la placa. Pines del esquema oficial, probados en el contador de YouTube. */
#define PIN_LED_R 22
#define PIN_LED_G 16
#define PIN_LED_B 17
/* Balance de blancos de este LED (el azul luce mucho más que el resto),
 * medido con una webcam en el contador de YouTube */
#define LED_BALANCE_R 192
#define LED_BALANCE_G 255
#define LED_BALANCE_B 82

#elif defined(PLACA_FNK0104)

/* Pantalla ILI9341 por SPI (FSPI) */
#define TFT_SPI_HOST SPI2_HOST
#define PIN_TFT_SCLK 12
#define PIN_TFT_MOSI 11
#define PIN_TFT_MISO 13
#define PIN_TFT_CS   10
#define PIN_TFT_DC   46
#define PIN_TFT_BL   45   /* retroiluminación encendida a nivel alto */
#define TFT_INVERTIR true
/* Giro para que se vea derecha en la carcasa, con el USB a la izquierda
 * (el mismo que usan los ejemplos de Freenove; comprobado en la placa) */
#define TFT_ROTACION 1

/* Táctil capacitivo FT6336 por I2C (dirección 0x38). El códec de audio
 * ES8311 comparte el mismo bus (Fase 2). INT va a nivel bajo mientras se
 * toca y es un pin RTC: puede despertar al ESP32. */
#define PIN_I2C_SDA    16
#define PIN_I2C_SCL    15
#define PIN_TOUCH_RST  18
#define PIN_TOUCH_IRQ  17

#else
#error "Placa no definida: compila con -D PLACA_CYD o -D PLACA_FNK0104 (ver platformio.ini)"
#endif
