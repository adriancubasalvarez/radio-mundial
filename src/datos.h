/**
 * Índice de regiones y países (data/regiones.json, generado por
 * tools/build_maps.py).
 */
#pragma once

#include <stdint.h>

#define MAX_REGIONES 16

typedef struct {
    char id[24];
    char nombre[48];
    uint8_t indice;   /* índice de color de la región en el mapamundi */
    char imagen[40];  /* mapa de la región */
    char zonas[40];   /* zonas táctiles del mapa de la región */
} Region;

typedef struct {
    char imagen[40];  /* mapamundi */
    char zonas[40];
    uint16_t ancho;
    uint16_t alto;
    Region regiones[MAX_REGIONES];
    uint8_t num_regiones;
} IndiceMundo;

/* Lee data/regiones.json. Devuelve false si no existe o está mal. */
bool datos_cargar_indice(IndiceMundo *indice);

/* Busca la región que tiene un índice de color del mapamundi; NULL si ninguna */
const Region *datos_region_por_indice(const IndiceMundo *indice, uint8_t color);

/* ---------- Países de una región ---------- */

#define MAX_PAISES 64

typedef struct {
    char codigo[3];   /* ISO 3166-1 alfa-2, p. ej. "ES" */
    char nombre[48];
    uint8_t indice;   /* índice de color del país en el mapa de su región */
    uint8_t emisoras; /* número de emisoras (de emisoras/indice.json) */
} Pais;

typedef struct {
    Pais paises[MAX_PAISES];
    uint8_t num;
} ListaPaises;

/* Lee de regiones.json los países de una región (ordenados por nombre) y
 * cuántas emisoras tiene cada uno (emisoras/indice.json) */
bool datos_cargar_paises(const char *id_region, ListaPaises *lista);

/* Busca el país que tiene un índice de color del mapa de región */
const Pais *datos_pais_por_indice(const ListaPaises *lista, uint8_t color);

/* ---------- Emisoras de un país ---------- */

typedef struct {
    char nombre[64];
    char url[256];
    char codec[8];
    uint16_t bitrate;  /* kbps; 0 si no se sabe */
} Emisora;

typedef struct {
    Emisora *emisoras; /* reservado con malloc; liberar con datos_liberar_emisoras */
    uint16_t num;
} ListaEmisoras;

/* Lee data/emisoras/<codigo>.json. Devuelve false si el país no tiene archivo. */
bool datos_cargar_emisoras(const char *codigo, ListaEmisoras *lista);

void datos_liberar_emisoras(ListaEmisoras *lista);

/* Elige una emisora al azar: primero un país al azar entre los que tienen
 * emisoras y luego una emisora al azar de ese país. */
bool datos_emisora_aleatoria(Emisora *emisora, Pais *pais);

/* Busca el nombre de un país por su código ISO */
bool datos_buscar_pais(const char *codigo, Pais *pais);

/* ---------- Búsqueda y géneros (buscar.txt) ---------- */

typedef struct {
    char letra;          /* la misma letra que GENEROS en build_stations.py */
    const char *nombre;
} Genero;

extern const Genero GENEROS[];
extern const int NUM_GENEROS;

#define MAX_RESULTADOS 40

typedef struct {
    char codigo[3];      /* país */
    uint16_t posicion;   /* posición de la emisora en emisoras/<codigo>.json */
    char nombre[64];
} Resultado;

/* Busca emisoras por nombre y/o género, de la más popular a la menos.
 * texto: palabras tecleadas (todas deben aparecer en el nombre; mayúsculas
 * y tildes dan igual); "" para no filtrar por nombre.
 * genero: letra de GENEROS, o 0 para no filtrar por género.
 * Devuelve cuántos resultados ha escrito (como mucho max). */
int datos_buscar(const char *texto, char genero, Resultado *resultados, int max);

/* Carga la emisora completa de un resultado de búsqueda */
bool datos_emisora_de_resultado(const Resultado *resultado, Emisora *emisora, Pais *pais);

/* ---------- Emisoras propias en la tarjeta SD ----------
 * Archivo mis_emisoras.csv en la raíz de la SD, una emisora por línea:
 *     nombre;url
 * Las líneas vacías y las que empiezan por # se ignoran. */
#define ARCHIVO_MIS_EMISORAS "mis_emisoras.csv"

/* true si hay tarjeta con mis_emisoras.csv */
bool datos_hay_mis_emisoras(void);

/* Lee mis_emisoras.csv. Devuelve false si no hay tarjeta o archivo. */
bool datos_cargar_mis_emisoras(ListaEmisoras *lista);
