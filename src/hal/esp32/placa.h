/**
 * Pines de cada placa. Solo se usan pines verificados:
 *
 *   PLACA_FNK0104  Freenove FNK0104 (ESP32-S3 con 8 MB de PSRAM y 16 MB de
 *              flash, 2,8" ILI9341, táctil capacitivo FT6336). Pines del
 *              esquema 2.8inch_ESP32-S3_Display_Schematic.pdf y de los
 *              ejemplos oficiales (repositorio Freenove_ESP32_S3_Display).
 */
#pragma once

#if defined(PLACA_FNK0104)

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

/* Audio: I2S hacia el códec ES8311 (I2C 0x18, mismo bus que el táctil). Los
 * datos al códec salen por el GPIO8 (su pin DSDIN en el esquema; el GPIO6
 * es el micrófono). El amplificador se habilita con AUDIO_EN a nivel bajo,
 * como en el ejemplo Sketch_07.1_Music de Freenove. */
#define PIN_I2S_MCK  4
#define PIN_I2S_BCK  5
#define PIN_I2S_WS   7
#define PIN_I2S_DOUT 8
#define PIN_AUDIO_EN 1

/* DAC externo por I2S (PCM5102A o UDA1334A) hacia un amplificador. Va al
 * conector de expansión P3 (1,25 mm, 4 pines: 1=IO2, 2=IO3, 3=IO14, 4=IO21),
 * libre en esta placa. P3 no lleva alimentación: el DAC toma +5 V y GND del
 * conector serie P2 (1=+5 V, 2=GND). Sin MCLK: el DAC saca el reloj de BCK
 * con su PLL. El IO3 queda libre (el ESP32-S3 lo lee al arrancar). */
#define PIN_EXT_BCK  14   /* P3-3 */
#define PIN_EXT_WS   21   /* P3-4: LCK en el PCM5102A, WSEL en el UDA1334A */
#define PIN_EXT_DOUT 2    /* P3-1: DIN del DAC */

/* LED RGB direccionable WS2812B (ejemplo Sketch_02.1_LedPixel de Freenove).
 * Luce muchísimo (Freenove lo usa a 10 de 255): la intensidad se elige en
 * Ajustes y la «alta» lo pone a tope. */
#define PIN_LED_WS2812 42
#define LED_MAXIMO     255  /* tope de cada color (0..255) */

#else
#error "Placa no definida: compila con -D PLACA_FNK0104 (ver platformio.ini)"
#endif
