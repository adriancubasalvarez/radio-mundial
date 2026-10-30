#include "imagen.h"
#include "hal.h"

#include <stdlib.h>
#include <string.h>

bool imagen_cargar(Imagen *imagen, const char *ruta)
{
    memset(imagen, 0, sizeof(Imagen));
    size_t tam = 0;
    imagen->datos = hal_leer_archivo(ruta, &tam);
    if (imagen->datos == NULL || tam < sizeof(lv_img_header_t)) {
        imagen_liberar(imagen);
        return false;
    }
    /* Cabecera de 4 bytes (formato, ancho, alto) y después los píxeles */
    memcpy(&imagen->dsc.header, imagen->datos, sizeof(lv_img_header_t));
    imagen->dsc.data = imagen->datos + sizeof(lv_img_header_t);
    imagen->dsc.data_size = tam - sizeof(lv_img_header_t);
    return true;
}

void imagen_liberar(Imagen *imagen)
{
    free(imagen->datos);
    memset(imagen, 0, sizeof(Imagen));
}
