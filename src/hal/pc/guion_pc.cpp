#include "guion_pc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Duración de un toque: lo bastante para que LVGL lo lea como pulsación */
#define DURACION_TOQUE 120

static FILE *archivo;
static uint32_t espera_hasta;   /* no leer más órdenes hasta este instante */
static uint32_t toque_hasta;    /* el dedo está pulsado hasta este instante */
static int toque_x, toque_y;
static char captura[256];

void guion_iniciar(void)
{
    const char *ruta = getenv("RADIO_GUION");
    if (ruta == NULL) {
        return;
    }
    archivo = fopen(ruta, "r");
    if (archivo == NULL) {
        fprintf(stderr, "No se puede abrir el guion %s\n", ruta);
    }
}

bool guion_atender(uint32_t ahora, int *x, int *y, bool *pulsado, bool *salir)
{
    if (archivo == NULL) {
        return false;
    }

    /* Mientras dura un toque, el dedo sigue abajo */
    if (ahora < toque_hasta) {
        *x = toque_x;
        *y = toque_y;
        *pulsado = true;
        return true;
    }

    if (ahora >= espera_hasta && captura[0] == '\0') {
        char linea[300];
        if (fgets(linea, sizeof(linea), archivo) == NULL) {
            fclose(archivo);
            archivo = NULL;
            return false;
        }
        int a, b, c;
        char nombre[256];
        if (sscanf(linea, "mantener %d %d %d", &a, &b, &c) == 3) {
            toque_x = a;
            toque_y = b;
            toque_hasta = ahora + c;
            espera_hasta = toque_hasta + 50;
            printf("[guion] mantener %d %d %d ms\n", a, b, c);
        } else if (sscanf(linea, "espera %d", &a) == 1) {
            espera_hasta = ahora + a;
        } else if (sscanf(linea, "toca %d %d", &a, &b) == 2) {
            toque_x = a;
            toque_y = b;
            toque_hasta = ahora + DURACION_TOQUE;
            /* Tras soltar, un respiro para que LVGL procese el clic */
            espera_hasta = toque_hasta + 50;
            printf("[guion] toca %d %d\n", a, b);
        } else if (sscanf(linea, "captura %255s", nombre) == 1) {
            strcpy(captura, nombre);
        } else if (strncmp(linea, "salir", 5) == 0) {
            *salir = true;
        }
    }

    /* Fuera de un toque, el dedo está levantado donde se tocó por última vez */
    *x = toque_x;
    *y = toque_y;
    *pulsado = false;
    return true;
}

const char *guion_captura_pendiente(void)
{
    return captura[0] ? captura : NULL;
}

void guion_captura_hecha(void)
{
    printf("[guion] captura %s\n", captura);
    captura[0] = '\0';
}
