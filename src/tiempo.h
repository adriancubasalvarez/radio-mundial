/**
 * El tiempo, la calidad del aire y la ubicación, para la pantalla de reposo.
 *
 * Sin claves ni servidores propios:
 *   - ubicación por la IP con ip-api.com (gratis para uso no comercial);
 *   - previsión y calidad del aire con Open-Meteo (open-meteo.com, gratis
 *     para uso no comercial; datos con licencia CC BY 4.0).
 * Se descarga en segundo plano cada 30 minutos, sin parar la música.
 */
#pragma once

#include <stdint.h>

#define DIAS_PREVISION 4  /* hoy y los 3 siguientes */

typedef struct {
    int codigo;           /* código WMO del cielo (0 despejado, 61 lluvia...) */
    int maxima, minima;   /* °C, redondeadas */
    int dia_semana;       /* 0 domingo .. 6 sábado */
} DiaPrevision;

typedef struct {
    bool hay;             /* hay datos del tiempo */
    char ciudad[40];
    int temperatura;      /* °C ahora, redondeada */
    int codigo;           /* código WMO del cielo ahora */
    bool de_dia;
    DiaPrevision dia[DIAS_PREVISION];
    char sale_sol[6];     /* "07:46" */
    char pone_sol[6];
    bool hay_aire;        /* hay datos de calidad del aire */
    int indice_aire;      /* índice europeo (EAQI) */
    float pm25;           /* partículas finas, µg/m³ */
} Tiempo;

/* Datos más recientes (hay = false hasta la primera descarga) */
const Tiempo *tiempo_actual(void);

/* Llamarlo en cada vuelta del bucle: lanza la descarga cuando toca y
 * recoge el resultado cuando llega */
void tiempo_atender(void);

/* Símbolo (de la fuente de iconos) y texto corto para un código WMO */
const char *tiempo_icono(int codigo, bool de_dia);
const char *tiempo_texto(int codigo);

/* Nivel del índice europeo de calidad del aire: texto y color (0xRRGGBB) */
const char *aire_texto(int indice);
uint32_t aire_color(int indice);
