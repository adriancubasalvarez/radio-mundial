/**
 * Archivos del simulador: se leen de la carpeta data/ del proyecto.
 * Se puede cambiar con la variable de entorno RADIO_DATA.
 */
#include "hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>

uint8_t *hal_leer_archivo(const char *ruta, size_t *tam)
{
    const char *base = getenv("RADIO_DATA");
    std::string completa = std::string(base ? base : "data") + "/" + ruta;

    FILE *f = fopen(completa.c_str(), "rb");
    if (f == NULL) {
        fprintf(stderr, "No se encuentra %s\n", completa.c_str());
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long largo = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *datos = (uint8_t *)malloc(largo + 1);
    if (datos != NULL) {
        fread(datos, 1, largo, f);
        datos[largo] = 0; /* por comodidad, si es texto queda terminado en cero */
        *tam = (size_t)largo;
    }
    fclose(f);
    return datos;
}
