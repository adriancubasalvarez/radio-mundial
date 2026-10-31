#include "datos.h"
#include "hal.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Copia un texto JSON a un char[] fijo sin pasarse del tamaño */
static void copiar(char *destino, size_t tam, JsonVariantConst origen)
{
    const char *texto = origen | "";
    strncpy(destino, texto, tam - 1);
    destino[tam - 1] = '\0';
}

bool datos_cargar_indice(IndiceMundo *indice)
{
    memset(indice, 0, sizeof(IndiceMundo));

    size_t tam = 0;
    uint8_t *texto = hal_leer_archivo("regiones.json", &tam);
    if (texto == NULL) {
        return false;
    }

    /* Solo nos interesan los campos de las regiones, no la lista de países */
    JsonDocument filtro;
    filtro["mundo"] = true;
    JsonObject f = filtro["regiones"][0].to<JsonObject>();
    f["id"] = true;
    f["nombre"] = true;
    f["indice"] = true;
    f["imagen"] = true;
    f["zonas"] = true;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char *)texto, tam,
                                                 DeserializationOption::Filter(filtro));
    free(texto);
    if (error) {
        fprintf(stderr, "regiones.json: %s\n", error.c_str());
        return false;
    }

    JsonObjectConst mundo = doc["mundo"];
    copiar(indice->imagen, sizeof(indice->imagen), mundo["imagen"]);
    copiar(indice->zonas, sizeof(indice->zonas), mundo["zonas"]);
    indice->ancho = mundo["ancho"] | 0;
    indice->alto = mundo["alto"] | 0;

    for (JsonObjectConst r : doc["regiones"].as<JsonArrayConst>()) {
        if (indice->num_regiones >= MAX_REGIONES) {
            break;
        }
        Region *region = &indice->regiones[indice->num_regiones++];
        copiar(region->id, sizeof(region->id), r["id"]);
        copiar(region->nombre, sizeof(region->nombre), r["nombre"]);
        copiar(region->imagen, sizeof(region->imagen), r["imagen"]);
        copiar(region->zonas, sizeof(region->zonas), r["zonas"]);
        region->indice = r["indice"] | 0;
    }
    return true;
}

const Region *datos_region_por_indice(const IndiceMundo *indice, uint8_t color)
{
    for (uint8_t i = 0; i < indice->num_regiones; i++) {
        if (indice->regiones[i].indice == color) {
            return &indice->regiones[i];
        }
    }
    return NULL;
}

/* Rellena cuántas emisoras tiene cada país. Si no existe el índice, se
 * supone que todos tienen alguna para no bloquear la navegación. */
static void contar_emisoras(ListaPaises *lista)
{
    size_t tam = 0;
    uint8_t *texto = hal_leer_archivo("emisoras/indice.json", &tam);
    JsonDocument doc;
    bool hay_indice = texto != NULL && !deserializeJson(doc, (const char *)texto, tam);
    free(texto);

    for (uint8_t i = 0; i < lista->num; i++) {
        Pais *pais = &lista->paises[i];
        pais->emisoras = hay_indice ? (doc[pais->codigo] | 0) : 1;
    }
}

bool datos_cargar_paises(const char *id_region, ListaPaises *lista)
{
    memset(lista, 0, sizeof(ListaPaises));

    size_t tam = 0;
    uint8_t *texto = hal_leer_archivo("regiones.json", &tam);
    if (texto == NULL) {
        return false;
    }

    JsonDocument filtro;
    filtro["regiones"][0]["id"] = true;
    filtro["regiones"][0]["paises"] = true;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char *)texto, tam,
                                                 DeserializationOption::Filter(filtro));
    free(texto);
    if (error) {
        fprintf(stderr, "regiones.json: %s\n", error.c_str());
        return false;
    }

    for (JsonObjectConst r : doc["regiones"].as<JsonArrayConst>()) {
        if (strcmp(r["id"] | "", id_region) != 0) {
            continue;
        }
        for (JsonObjectConst p : r["paises"].as<JsonArrayConst>()) {
            if (lista->num >= MAX_PAISES) {
                break;
            }
            Pais *pais = &lista->paises[lista->num++];
            copiar(pais->codigo, sizeof(pais->codigo), p["codigo"]);
            copiar(pais->nombre, sizeof(pais->nombre), p["nombre"]);
            pais->indice = p["indice"] | 0;
        }
        contar_emisoras(lista);
        return true;
    }
    return false;
}

const Pais *datos_pais_por_indice(const ListaPaises *lista, uint8_t color)
{
    for (uint8_t i = 0; i < lista->num; i++) {
        if (lista->paises[i].indice == color) {
            return &lista->paises[i];
        }
    }
    return NULL;
}

bool datos_cargar_emisoras(const char *codigo, ListaEmisoras *lista)
{
    memset(lista, 0, sizeof(ListaEmisoras));

    char ruta[32];
    snprintf(ruta, sizeof(ruta), "emisoras/%s.json", codigo);
    size_t tam = 0;
    uint8_t *texto = hal_leer_archivo(ruta, &tam);
    if (texto == NULL) {
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char *)texto, tam);
    free(texto);
    if (error) {
        fprintf(stderr, "%s: %s\n", ruta, error.c_str());
        return false;
    }

    JsonArrayConst arr = doc.as<JsonArrayConst>();
    lista->emisoras = (Emisora *)calloc(arr.size(), sizeof(Emisora));
    if (lista->emisoras == NULL) {
        return false;
    }
    for (JsonObjectConst e : arr) {
        Emisora *emisora = &lista->emisoras[lista->num++];
        copiar(emisora->nombre, sizeof(emisora->nombre), e["n"]);
        copiar(emisora->url, sizeof(emisora->url), e["u"]);
        copiar(emisora->codec, sizeof(emisora->codec), e["c"]);
        emisora->bitrate = e["b"] | 0;
    }
    return true;
}

void datos_liberar_emisoras(ListaEmisoras *lista)
{
    free(lista->emisoras);
    lista->emisoras = NULL;
    lista->num = 0;
}

/* Busca el nombre de un país en regiones.json */
bool datos_buscar_pais(const char *codigo, Pais *pais)
{
    size_t tam = 0;
    uint8_t *texto = hal_leer_archivo("regiones.json", &tam);
    if (texto == NULL) {
        return false;
    }
    JsonDocument filtro;
    filtro["regiones"][0]["paises"][0]["codigo"] = true;
    filtro["regiones"][0]["paises"][0]["nombre"] = true;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char *)texto, tam,
                                                 DeserializationOption::Filter(filtro));
    free(texto);
    if (error) {
        return false;
    }
    for (JsonObjectConst r : doc["regiones"].as<JsonArrayConst>()) {
        for (JsonObjectConst p : r["paises"].as<JsonArrayConst>()) {
            if (strcmp(p["codigo"] | "", codigo) == 0) {
                memset(pais, 0, sizeof(Pais));
                copiar(pais->codigo, sizeof(pais->codigo), p["codigo"]);
                copiar(pais->nombre, sizeof(pais->nombre), p["nombre"]);
                return true;
            }
        }
    }
    return false;
}

bool datos_emisora_aleatoria(Emisora *emisora, Pais *pais)
{
    size_t tam = 0;
    uint8_t *texto = hal_leer_archivo("emisoras/indice.json", &tam);
    if (texto == NULL) {
        return false;
    }
    JsonDocument indice;
    DeserializationError error = deserializeJson(indice, (const char *)texto, tam);
    free(texto);
    if (error) {
        return false;
    }

    /* Países con alguna emisora */
    JsonObjectConst paises = indice.as<JsonObjectConst>();
    size_t con_emisoras = 0;
    for (JsonPairConst p : paises) {
        con_emisoras += (p.value().as<int>() > 0);
    }

    /* Se intenta unas cuantas veces por si algún archivo falla */
    for (int intento = 0; intento < 5 && con_emisoras > 0; intento++) {
        size_t elegido = hal_aleatorio() % con_emisoras;
        const char *codigo = NULL;
        for (JsonPairConst p : paises) {
            if (p.value().as<int>() > 0 && elegido-- == 0) {
                codigo = p.key().c_str();
                break;
            }
        }
        ListaEmisoras lista;
        if (codigo == NULL || !datos_cargar_emisoras(codigo, &lista) || lista.num == 0) {
            continue;
        }
        *emisora = lista.emisoras[hal_aleatorio() % lista.num];
        datos_liberar_emisoras(&lista);
        if (!datos_buscar_pais(codigo, pais)) {
            memset(pais, 0, sizeof(Pais));
            strncpy(pais->codigo, codigo, sizeof(pais->codigo) - 1);
            strncpy(pais->nombre, codigo, sizeof(pais->nombre) - 1);
        }
        return true;
    }
    return false;
}

/* ---------- Búsqueda y géneros ---------- */

/* Misma tabla de letras que GENEROS en tools/build_stations.py */
const Genero GENEROS[] = {
    {'p', "Pop y éxitos"},
    {'r', "Rock"},
    {'n', "Noticias y tertulia"},
    {'d', "Deportes"},
    {'j', "Jazz, blues y soul"},
    {'c', "Clásica"},
    {'e', "Electrónica y dance"},
    {'l', "Latina"},
    {'h', "Hip-hop y R&B"},
    {'o', "Éxitos de siempre"},
    {'f', "Religiosa"},
    {'t', "Folk, country y tradicional"},
    {'a', "Chill y lounge"},
};
const int NUM_GENEROS = sizeof(GENEROS) / sizeof(GENEROS[0]);

#define MAX_PALABRAS 6

/* Separa la línea de buscar.txt en sus 5 campos (modifica la línea) */
static bool campos(char *linea, char *partes[5])
{
    int n = 0;
    partes[n++] = linea;
    for (char *p = linea; *p && n < 5; p++) {
        if (*p == '\t') {
            *p = '\0';
            partes[n++] = p + 1;
        }
    }
    return n == 5;
}

int datos_buscar(const char *texto, char genero, Resultado *resultados, int max)
{
    /* El texto tecleado se pasa a minúsculas y se parte en palabras.
     * El teclado solo tiene letras sin tilde, números y espacio, igual que
     * el campo «nombre para buscar» de buscar.txt. */
    char consulta[64];
    size_t i = 0;
    for (; texto[i] && i < sizeof(consulta) - 1; i++) {
        char c = texto[i];
        consulta[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    consulta[i] = '\0';

    char *palabras[MAX_PALABRAS];
    int num_palabras = 0;
    for (char *p = strtok(consulta, " "); p && num_palabras < MAX_PALABRAS; p = strtok(NULL, " ")) {
        palabras[num_palabras++] = p;
    }
    if (num_palabras == 0 && genero == 0) {
        return 0;
    }

    HalArchivo *archivo = hal_archivo_abrir("buscar.txt");
    if (archivo == NULL) {
        return 0;
    }
    int encontrados = 0;
    char linea[200];
    while (encontrados < max && hal_archivo_linea(archivo, linea, sizeof(linea))) {
        char *partes[5];
        if (!campos(linea, partes)) {
            continue;
        }
        if (genero != 0 && strchr(partes[2], genero) == NULL) {
            continue;
        }
        bool vale = true;
        for (int k = 0; k < num_palabras && vale; k++) {
            vale = strstr(partes[3], palabras[k]) != NULL;
        }
        if (!vale) {
            continue;
        }
        Resultado *r = &resultados[encontrados++];
        strncpy(r->codigo, partes[0], sizeof(r->codigo) - 1);
        r->codigo[sizeof(r->codigo) - 1] = '\0';
        r->posicion = (uint16_t)atoi(partes[1]);
        strncpy(r->nombre, partes[4], sizeof(r->nombre) - 1);
        r->nombre[sizeof(r->nombre) - 1] = '\0';
    }
    hal_archivo_cerrar(archivo);
    return encontrados;
}

bool datos_emisora_de_resultado(const Resultado *resultado, Emisora *emisora, Pais *pais)
{
    ListaEmisoras lista;
    if (!datos_cargar_emisoras(resultado->codigo, &lista)) {
        return false;
    }
    bool ok = resultado->posicion < lista.num;
    if (ok) {
        *emisora = lista.emisoras[resultado->posicion];
    }
    datos_liberar_emisoras(&lista);
    if (ok && !datos_buscar_pais(resultado->codigo, pais)) {
        memset(pais, 0, sizeof(Pais));
        strncpy(pais->codigo, resultado->codigo, sizeof(pais->codigo) - 1);
        strncpy(pais->nombre, resultado->codigo, sizeof(pais->nombre) - 1);
    }
    return ok;
}

/* ---------- Emisoras propias en la SD ---------- */

bool datos_hay_mis_emisoras(void)
{
    size_t tam = 0;
    uint8_t *texto = hal_sd_leer(ARCHIVO_MIS_EMISORAS, &tam);
    free(texto);
    return texto != NULL;
}

bool datos_cargar_mis_emisoras(ListaEmisoras *lista)
{
    memset(lista, 0, sizeof(ListaEmisoras));
    size_t tam = 0;
    char *texto = (char *)hal_sd_leer(ARCHIVO_MIS_EMISORAS, &tam);
    if (texto == NULL) {
        return false;
    }

    /* Primera pasada: contar líneas para reservar memoria */
    size_t lineas = 1;
    for (size_t i = 0; i < tam; i++) {
        lineas += (texto[i] == '\n');
    }
    lista->emisoras = (Emisora *)calloc(lineas, sizeof(Emisora));
    if (lista->emisoras == NULL) {
        free(texto);
        return false;
    }

    /* Segunda pasada: «nombre;url» en cada línea */
    for (char *linea = strtok(texto, "\r\n"); linea; linea = strtok(NULL, "\r\n")) {
        while (*linea == ' ') {
            linea++;
        }
        char *separador = strchr(linea, ';');
        if (*linea == '#' || separador == NULL) {
            continue;
        }
        *separador = '\0';
        const char *url = separador + 1;
        while (*url == ' ') {
            url++;
        }
        if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
            continue;
        }
        Emisora *e = &lista->emisoras[lista->num++];
        strncpy(e->nombre, linea, sizeof(e->nombre) - 1);
        strncpy(e->url, url, sizeof(e->url) - 1);
        /* La URL puede acabar en ;algo (columna extra): se corta ahí */
        char *fin = strchr(e->url, ';');
        if (fin) {
            *fin = '\0';
        }
        strcpy(e->codec, "?");
    }
    free(texto);
    return true;
}