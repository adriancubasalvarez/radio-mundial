// Pines de la Freenove FNK0104 (2,8") usados por el kit de conexión.
// Mismos valores que src/hal/esp32/placa.h del firmware de la radio.
#pragma once
#include <Arduino.h>

// Conector «I/O pin port» (P3): serigrafía IO2, IO3, IO14, IO21
constexpr int PIN_DIN = 2;    // -> DIN del DAC
constexpr int PIN_BCK = 14;   // -> BCK del DAC
constexpr int PIN_LCK = 21;   // -> LCK del DAC (WSEL en el UDA1334A)
// IO3 no se toca: el ESP32-S3 lo lee al arrancar.

// Conector «Serial port» / UART (P2): serigrafía 5V, GND, TXD, RXD
//   5V -> VCC del DAC     GND -> GND del DAC     TXD, RXD sin conectar

constexpr int PIN_AMP_INTEGRADO = 1;   // amplificador del altavoz integrado, activo a nivel bajo
constexpr int PIN_LED = 42;            // LED RGB WS2812B de la placa
constexpr int PIN_PANTALLA_LUZ = 45;   // retroiluminación

// Deja la placa en silencio: altavoz integrado apagado, pantalla apagada
inline void prepararPlaca() {
  pinMode(PIN_AMP_INTEGRADO, OUTPUT);
  digitalWrite(PIN_AMP_INTEGRADO, HIGH);
  pinMode(PIN_PANTALLA_LUZ, OUTPUT);
  digitalWrite(PIN_PANTALLA_LUZ, LOW);
}

inline void led(uint8_t r, uint8_t g, uint8_t b) { neopixelWrite(PIN_LED, r, g, b); }
