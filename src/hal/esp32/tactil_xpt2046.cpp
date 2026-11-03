/**
 * Táctil resistivo XPT2046 de la CYD, por SPI software (VSPI queda para la
 * SD). Lectura en bruto, mediana de 7 medidas y calibración de placa.h.
 */
#include "placa.h"

#ifdef PLACA_CYD

#include "tactil.h"
#include "hal.h"

#include <Arduino.h>
#include <algorithm>
#include <driver/gpio.h>

static uint8_t transferir(uint8_t salida)
{
    uint8_t entrada = 0;
    for (int bit = 7; bit >= 0; bit--) {  // modo SPI 0, primero el bit más alto
        digitalWrite(PIN_TOUCH_MOSI, (salida >> bit) & 1);
        delayMicroseconds(1);
        digitalWrite(PIN_TOUCH_SCLK, HIGH);
        delayMicroseconds(1);
        entrada = (entrada << 1) | digitalRead(PIN_TOUCH_MISO);
        digitalWrite(PIN_TOUCH_SCLK, LOW);
    }
    return entrada;
}

/* Lectura en bruto (mediana de 7 medidas); false si no se está tocando */
static bool leer_bruto(int &rx, int &ry)
{
    uint8_t d[57];
    memset(d, 0, sizeof(d));
    for (int j = 0; j < 7; j++) {
        d[j * 8 + 0] = 0x91;  // Y
        d[j * 8 + 2] = 0xB1;  // Z1
        d[j * 8 + 4] = 0xD1;  // X
        d[j * 8 + 6] = 0xC1;  // Z2
    }
    d[56] = 0x80;  // al final, a reposo con el aviso de toque activado
    digitalWrite(PIN_TOUCH_CS, LOW);
    for (auto &b : d) {
        b = transferir(b);
    }
    digitalWrite(PIN_TOUCH_CS, HIGH);

    int xt[7], yt[7], zt[7], ix = 0, iy = 0, iz = 0;
    for (int j = 0; j < 7; j++) {
        const uint8_t *v = &d[j * 8];
        int x = (v[5] << 8 | v[6]) >> 3;
        int y = (v[1] << 8 | v[2]) >> 3;
        int z = 0x3200 + y - x + (((v[3] << 8 | v[4]) - (v[7] << 8 | v[8])) >> 1);
        if (x > 128 && x <= 3968) xt[ix++] = x;
        if (y > 128 && y <= 3968) yt[iy++] = y;
        if (z > 0) zt[iz++] = z;
    }
    if (ix < 3 || iy < 3 || iz < 3) {
        return false;
    }
    std::sort(xt, xt + ix);
    std::sort(yt, yt + iy);
    std::sort(zt, zt + iz);
    if ((zt[iz >> 1] >> 8) == 0) {
        return false;
    }
    rx = xt[ix >> 1];
    ry = yt[iy >> 1];
    return true;
}

void tactil_iniciar(void)
{
    pinMode(PIN_TOUCH_SCLK, OUTPUT);
    pinMode(PIN_TOUCH_MOSI, OUTPUT);
    pinMode(PIN_TOUCH_MISO, INPUT);
    pinMode(PIN_TOUCH_CS, OUTPUT);
    pinMode(PIN_TOUCH_IRQ, INPUT);
    digitalWrite(PIN_TOUCH_CS, HIGH);
    digitalWrite(PIN_TOUCH_SCLK, LOW);
}

bool tactil_leer(int *x, int *y)
{
    int rx, ry;
    if (!leer_bruto(rx, ry)) {
        return false;
    }
    /* De la calibración (panel vertical) a la pantalla horizontal con el
     * USB a la izquierda (setRotation(1)) */
    int px = (rx - TOUCH_X_MIN) * 239 / (TOUCH_X_MAX - TOUCH_X_MIN);
    int py = (ry - TOUCH_Y_MIN) * 319 / (TOUCH_Y_MAX - TOUCH_Y_MIN);
    *x = constrain(py, 0, PANTALLA_ANCHO - 1);
    *y = constrain(239 - px, 0, PANTALLA_ALTO - 1);
    return true;
}

bool tactil_tocando(void)
{
    return digitalRead(PIN_TOUCH_IRQ) == LOW;
}

void tactil_dormir(void)
{
    /* Con CS suelto el XPT2046 podría tomar ruido por órdenes y dejar de
     * avisar de los toques: los pines se quedan fijos mientras duerme */
    digitalWrite(PIN_TOUCH_CS, HIGH);
    digitalWrite(PIN_TOUCH_SCLK, LOW);
    digitalWrite(PIN_TOUCH_MOSI, LOW);
    gpio_hold_en((gpio_num_t)PIN_TOUCH_CS);
    gpio_hold_en((gpio_num_t)PIN_TOUCH_SCLK);
    gpio_hold_en((gpio_num_t)PIN_TOUCH_MOSI);
}

void tactil_soltar(void)
{
    gpio_hold_dis((gpio_num_t)PIN_TOUCH_CS);
    gpio_hold_dis((gpio_num_t)PIN_TOUCH_SCLK);
    gpio_hold_dis((gpio_num_t)PIN_TOUCH_MOSI);
}

#endif
