/**
 * Táctil capacitivo FT6336 de la FNK0104, por I2C (Wire, dirección 0x38).
 *
 * Registros (hoja de datos del FT6336 en el repositorio de Freenove):
 *   0x02  número de dedos (4 bits bajos)
 *   0x03  X del primer dedo, 4 bits altos (los 2 de arriba son el evento)
 *   0x04  X, 8 bits bajos
 *   0x05  Y, 4 bits altos
 *   0x06  Y, 8 bits bajos
 * Las coordenadas vienen del panel en vertical (240 × 320).
 */
#include "placa.h"

#ifdef PLACA_FNK0104

#include "tactil.h"
#include "hal.h"

#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>

#define DIRECCION_FT6336 0x38

void tactil_iniciar(void)
{
    pinMode(PIN_TOUCH_IRQ, INPUT);
    /* Reinicio del FT6336: a nivel bajo un momento y esperar a que arranque */
    pinMode(PIN_TOUCH_RST, OUTPUT);
    digitalWrite(PIN_TOUCH_RST, LOW);
    delay(10);
    digitalWrite(PIN_TOUCH_RST, HIGH);
    delay(300);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
}

bool tactil_leer(int *x, int *y)
{
    uint8_t d[5];
    Wire.beginTransmission(DIRECCION_FT6336);
    Wire.write(0x02);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(DIRECCION_FT6336, 5) != 5) {
        return false;
    }
    for (uint8_t &b : d) {
        b = Wire.read();
    }
    uint8_t dedos = d[0] & 0x0F;
    if (dedos == 0 || dedos > 2) {
        return false;
    }
    int px = (d[1] & 0x0F) << 8 | d[2];  // 0..239
    int py = (d[3] & 0x0F) << 8 | d[4];  // 0..319
    /* Del panel en vertical a la pantalla horizontal. Medido en la placa
     * tocando las cuatro esquinas con LovyanGFX en el giro 1 (arriba a la
     * izquierda: chip x 21, y 288; abajo a la derecha: x 213, y 24). Ojo:
     * es media vuelta respecto al ejemplo de Freenove con TFT_eSPI. */
#if TFT_ROTACION == 1
    *x = 319 - py;
    *y = px;
#else
    *x = py;
    *y = 239 - px;
#endif
    /* El panel lee los toques «encogidos» hacia el centro: arriba, unos 20
     * píxeles más abajo de donde está el dedo (la flecha de atrás, a y = 15,
     * se leía entre 21 y 48), y abajo, más arriba. Se estiran hacia los
     * bordes, con lo medido en la placa: x de 31..296 a 10..310 e y de
     * 33..217 a 12..228. */
    *x = (*x * 1132 - 25100) / 1000;
    *y = (*y * 1174 - 26700) / 1000;
    *x = constrain(*x, 0, PANTALLA_ANCHO - 1);
    *y = constrain(*y, 0, PANTALLA_ALTO - 1);
    return true;
}

bool tactil_tocando(void)
{
    return digitalRead(PIN_TOUCH_IRQ) == LOW;
}

void tactil_dormir(void)
{
    /* El FT6336 sigue alimentado y avisa por INT al tocar; su reinicio se
     * queda a nivel alto para que no se reinicie mientras dormimos */
    digitalWrite(PIN_TOUCH_RST, HIGH);
    gpio_hold_en((gpio_num_t)PIN_TOUCH_RST);
}

void tactil_soltar(void)
{
    gpio_hold_dis((gpio_num_t)PIN_TOUCH_RST);
}

#endif
