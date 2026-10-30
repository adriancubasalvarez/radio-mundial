/**
 * Navegación entre pantallas.
 *
 * Cada pantalla se construye al entrar y se destruye al salir (así solo hay
 * en memoria un mapa a la vez, importante en la placa). Una pila recuerda el
 * camino para que «Atrás» funcione siempre igual:
 *
 *   mundo -> región -> (lista de países) -> emisoras -> reproduciendo
 */
#pragma once

#include "datos.h"

#include <lvgl.h>

typedef enum {
    PANT_MUNDO,
    PANT_REGION,
    PANT_PAISES,
    PANT_EMISORAS,
    PANT_SONANDO,
    PANT_FAVORITOS,
} Pantalla;

/* Lo elegido hasta ahora en el recorrido */
typedef struct {
    IndiceMundo indice;   /* regiones.json */
    Region region;        /* región elegida */
    Pais pais;            /* país elegido */
    Emisora emisora;      /* emisora elegida en la lista */
    Emisora sonando;      /* emisora que está sonando (puede ser otra) */
    bool hay_sonando;     /* true si deberíamos estar oyendo "sonando" */
    bool aleatoria;       /* «Reproduciendo» en modo emisora aleatoria */
} EstadoNav;

extern EstadoNav nav;

/* Abre una pantalla nueva encima de la actual */
void nav_ir(Pantalla pantalla);

/* Vuelve a la pantalla anterior */
void nav_atras(void);

/* Constructores de cada pantalla (devuelven un objeto pantalla de LVGL) */
lv_obj_t *pantalla_mundo(void);
lv_obj_t *pantalla_region(void);
lv_obj_t *pantalla_paises(void);
lv_obj_t *pantalla_emisoras(void);
lv_obj_t *pantalla_sonando(void);
lv_obj_t *pantalla_favoritos(void);

/* Pone a sonar nav.emisora (de nav.pais) y la recuerda como última */
void nav_reproducir(void);
