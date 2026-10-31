/**
 * Ajustes que se recuerdan entre sesiones: volumen, última emisora,
 * favoritos y tiempo de reposo. Se guardan como JSON a través de la HAL
 * (ajustes.json en el PC, NVS en la placa).
 */
#pragma once

#include "datos.h"

#include <stdint.h>

#define MAX_FAVORITOS 20

/* Emisora con el país al que pertenece */
typedef struct {
    Emisora emisora;
    char pais[3];
    char nombre_pais[48];
} EmisoraGuardada;

typedef struct {
    uint8_t volumen;          /* 0..100 */
    uint16_t reposo_s;        /* segundos sin tocar hasta el reposo (0 = nunca) */
    bool reanudar;            /* al arrancar, volver a poner la última emisora */
    bool idioma_elegido;      /* false: el primer arranque pregunta el idioma */
    bool espanol;             /* idioma: true español, false inglés */
    bool hay_ultima;
    EmisoraGuardada ultima;
    EmisoraGuardada favoritos[MAX_FAVORITOS];
    uint8_t num_favoritos;
} Ajustes;

extern Ajustes ajustes;

/* Lee los ajustes guardados (o pone los de fábrica si no hay) */
void ajustes_cargar(void);

/* Guarda los ajustes */
void ajustes_guardar(void);

/* Cambia el idioma de la interfaz y guarda */
void ajustes_poner_idioma(bool espanol);

/* Recuerda la emisora que está sonando y guarda */
void ajustes_recordar_ultima(const Emisora *emisora, const Pais *pais);

/* true si la URL está en favoritos */
bool ajustes_es_favorito(const char *url);

/* Añade o quita una emisora de favoritos y guarda.
 * Devuelve true si ahora es favorita. Si la lista está llena no la añade. */
bool ajustes_alternar_favorito(const Emisora *emisora, const Pais *pais);
