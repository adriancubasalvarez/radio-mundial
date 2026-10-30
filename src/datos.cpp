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
static bool buscar_pais(const char *codigo, Pais *pais)
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
        if (!buscar_pais(codigo, pais)) {
            memset(pais, 0, sizeof(Pais));
            strncpy(pais->codigo, codigo, sizeof(pais->codigo) - 1);
            strncpy(pais->nombre, codigo, sizeof(pais->nombre) - 1);
        }
        return true;
    }
    return false;
}
