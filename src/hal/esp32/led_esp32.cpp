/**
 * LED RGB de la placa.
 *
 * CYD: LED de ánodo común en tres pines, por PWM (Pulse Width Modulation)
 * de 12 bits para que los tonos tenues y los cambios suaves no den saltos.
 * Canales LEDC 0, 1 y 2: la retroiluminación de la pantalla usa el 7 (ver
 * pantalla_esp32.cpp). En el contador de YouTube, con el LED en el mismo
 * canal que la pantalla, el rojo se quedaba siempre encendido.
 *
 * FNK0104: LED direccionable WS2812B en un solo pin, con neopixelWrite() del
 * núcleo de Arduino (RMT).
 */
#include "hal.h"
#include "placa.h"

#include <Arduino.h>

#if defined(PIN_LED_R)

#define LED_BITS 12
#define LED_TOTAL (1 << LED_BITS)

static const uint8_t PINES[3] = {PIN_LED_R, PIN_LED_G, PIN_LED_B};
static const uint8_t BALANCE[3] = {LED_BALANCE_R, LED_BALANCE_G, LED_BALANCE_B};
static bool iniciado;

void hal_led(uint8_t r, uint8_t g, uint8_t b)
{
    if (!iniciado) {
        for (uint8_t i = 0; i < 3; i++) {
            ledcSetup(i, 5000, LED_BITS);
            ledcAttachPin(PINES[i], i);
        }
        iniciado = true;
    }
    const uint8_t color[3] = {r, g, b};
    for (uint8_t i = 0; i < 3; i++) {
        /* El ojo ve la luz más o menos al cuadrado: así el brillo sube
         * parejo. Ánodo común: apagado es el ciclo entero a nivel alto. */
        float x = color[i] / 255.0f;
        uint32_t luz = (uint32_t)(x * x * BALANCE[i] / 255.0f * LED_TOTAL);
        ledcWrite(i, LED_TOTAL - luz);
    }
}

#elif defined(PIN_LED_WS2812)

void hal_led(uint8_t r, uint8_t g, uint8_t b)
{
    /* Mismo reparto «al cuadrado» que en la CYD, con un tope bajo. Solo se
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
