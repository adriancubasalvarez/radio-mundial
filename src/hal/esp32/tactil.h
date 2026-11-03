/**
 * Táctil de cada placa (lo usa pantalla_esp32.cpp):
 *   CYD      XPT2046 resistivo por SPI software (tactil_xpt2046.cpp)
 *   FNK0104  FT6336 capacitivo por I2C (tactil_ft6336.cpp)
 */
#pragma once

void tactil_iniciar(void);

/* Punto tocado en la pantalla horizontal (320 × 240), ya girado como la
 * pantalla. false si no se está tocando. */
bool tactil_leer(int *x, int *y);

/* true mientras hay un dedo encima (pin de aviso a nivel bajo) */
bool tactil_tocando(void);

/* Antes de dormir: deja el táctil avisando de los toques y fija sus pines.
 * tactil_soltar() suelta esos pines al arrancar otra vez. */
void tactil_dormir(void);
void tactil_soltar(void);
