/**
 * Ajustes que se recuerdan entre sesiones: volumen, última emisora,
 * favoritos y tiempo de reposo. Se guardan como JSON a través de la HAL
 * (ajustes.json en el PC, NVS en la placa).
 */
#pragma once

#include "datos.h"

#include <stdint.h>

#define MAX_FAVORITOS 20
#define MAX_RECIENTES 10
#define MAX_FALLOS 40
#define MAX_ALARMAS 5

/* Emisora con el país al que pertenece */
typedef struct {
    Emisora emisora;
    char pais[3];
    char nombre_pais[48];
} EmisoraGuardada;

typedef struct {
    uint8_t hora, minuto;
    uint8_t dias;             /* bit 0 domingo .. bit 6 sábado; 0 = una vez */
    bool activa;
    uint8_t volumen;          /* 0..100, al que llega al sonar */
    bool con_emisora;         /* false: suena la última emisora escuchada */
    bool amanecer;            /* el LED sube poco a poco antes de sonar */
    EmisoraGuardada emisora;
} Alarma;

/* Emisora que no llegó a sonar: basta un resumen de su URL */
typedef struct {
    uint32_t resumen;
    uint8_t veces;
} FalloEmisora;

typedef struct {
    uint8_t volumen;          /* 0..100 */
    uint16_t reposo_s;        /* segundos sin tocar hasta el reposo (0 = nunca) */
    bool reanudar;            /* al arrancar, volver a poner la última emisora */
    bool idioma_elegido;      /* false: el primer arranque pregunta el idioma */
    bool espanol;             /* idioma: true español, false inglés */
    uint8_t luz;              /* ModoLuz (luz.h): apagada, estado o al ritmo */
    uint8_t luz_brillo;       /* intensidad del LED: 0 baja, 1 media, 2 alta */
    bool hay_ultima;
    EmisoraGuardada ultima;
    EmisoraGuardada favoritos[MAX_FAVORITOS];
    uint8_t num_favoritos;
    EmisoraGuardada recientes[MAX_RECIENTES];  /* la más reciente, primero */
    uint8_t num_recientes;
    FalloEmisora fallos[MAX_FALLOS];  /* el más antiguo, primero */
    uint8_t num_fallos;
    Alarma alarmas[MAX_ALARMAS];
    uint8_t num_alarmas;
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

/* Una emisora no llegó a sonar (con la WiFi bien): se anota y se guarda */
void ajustes_anotar_fallo(const char *url);

/* Una emisora volvió a sonar: se olvidan sus fallos */
void ajustes_olvidar_fallo(const char *url);

/* true si falló varias veces seguidas: va al final de la lista, en gris */
bool ajustes_emisora_floja(const char *url);

/* true si la URL está en favoritos */
bool ajustes_es_favorito(const char *url);

/* Añade o quita una emisora de favoritos y guarda.
 * Devuelve true si ahora es favorita. Si la lista está llena no la añade. */
bool ajustes_alternar_favorito(const Emisora *emisora, const Pais *pais);
