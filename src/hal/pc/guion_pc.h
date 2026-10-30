/**
 * Modo guion del simulador (solo para pruebas en el PC).
 *
 * Si la variable de entorno RADIO_GUION apunta a un archivo de texto, el
 * simulador ejecuta sus órdenes, una por línea, sin tocar el ratón real:
 *
 *   espera 1000          esperar 1000 ms
 *   toca 160 120         tocar en (160, 120) de la pantalla de 320x240
 *   captura mundo.bmp    guardar la pantalla en un BMP
 *   salir                cerrar el simulador
 *
 * Las líneas vacías y las que empiezan por # se ignoran.
 */
#pragma once

#include <stdint.h>

/* Abre el guion si RADIO_GUION está definida */
void guion_iniciar(void);

/* Avanza el guion. Si el guion está tocando la pantalla, rellena x, y y
 * pulsado y devuelve true. Pone *salir a true al llegar a «salir». */
bool guion_atender(uint32_t ahora, int *x, int *y, bool *pulsado, bool *salir);

/* Pide guardar la próxima pantalla completa (lo usa pantalla_pc.cpp) */
const char *guion_captura_pendiente(void);
void guion_captura_hecha(void);
