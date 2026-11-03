/**
 * LED RGB de la placa.
 *
 * FNK0104: LED direccionable WS2812B en un solo pin, con neopixelWrite() del
 * núcleo de Arduino (RMT).
 */
#include "hal.h"
#include "placa.h"

#include <Arduino.h>

#if defined(PIN_LED_WS2812)

void hal_led(uint8_t r, uint8_t g, uint8_t b)
{
    /* El ojo ve la luz más o menos al cuadrado: así el brillo sube parejo. Solo se
     * manda al LED cuando cambia: cada envío ocupa el RMT un momento. */
    static uint8_t antes[3] = {1, 1, 1};
    const uint8_t color[3] = {r, g, b};
    uint8_t salida[3];
    for (uint8_t i = 0; i < 3; i++) {
        float x = color[i] / 255.0f;
        salida[i] = (uint8_t)(x * x * LED_MAXIMO + 0.5f);
    }
    if (memcmp(salida, antes, 3) != 0) {
        neopixelWrite(PIN_LED_WS2812, salida[0], salida[1], salida[2]);
        memcpy(antes, salida, 3);
    }
}

#else

void hal_led(uint8_t r, uint8_t g, uint8_t b)
{
    (void)r;
    (void)g;
    (void)b;
}

#endif
