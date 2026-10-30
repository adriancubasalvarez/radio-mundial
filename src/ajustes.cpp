#include "ajustes.h"
#include "hal.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Ajustes ajustes;

static void copiar(char *destino, size_t tam, JsonVariantConst origen)
{
    const char *texto = origen | "";
    strncpy(destino, texto, tam - 1);
    destino[tam - 1] = '\0';
}

static void a_json(JsonObject o, const EmisoraGuardada *e)
{
    o["n"] = e->emisora.nombre;
    o["u"] = e->emisora.url;
    o["c"] = e->emisora.codec;
    o["b"] = e->emisora.bitrate;
    o["p"] = e->pais;
    o["np"] = e->nombre_pais;
}

static void de_json(JsonObjectConst o, EmisoraGuardada *e)
{
    copiar(e->emisora.nombre, sizeof(e->emisora.nombre), o["n"]);
    copiar(e->emisora.url, sizeof(e->emisora.url), o["u"]);
    copiar(e->emisora.codec, sizeof(e->emisora.codec), o["c"]);
    e->emisora.bitrate = o["b"] | 0;
    copiar(e->pais, sizeof(e->pais), o["p"]);
    copiar(e->nombre_pais, sizeof(e->nombre_pais), o["np"]);
}

static void guardada(EmisoraGuardada *destino, const Emisora *emisora, const Pais *pais)
{
    destino->emisora = *emisora;
    strncpy(destino->pais, pais->codigo, sizeof(destino->pais));
    strncpy(destino->nombre_pais, pais->nombre, sizeof(destino->nombre_pais));
    destino->pais[sizeof(destino->pais) - 1] = '\0';
    destino->nombre_pais[sizeof(destino->nombre_pais) - 1] = '\0';
}

void ajustes_cargar(void)
{
    /* Valores de fábrica */
    memset(&ajustes, 0, sizeof(ajustes));
    ajustes.volumen = 70;
    ajustes.reposo_s = 120;
    ajustes.reanudar = true;

    char *texto = hal_ajustes_leer();
    if (texto == NULL) {
        return;
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, texto);
    free(texto);
    if (error) {
        fprintf(stderr, "Ajustes dañados (%s): se usan los de fábrica\n", error.c_str());
        return;
    }

    ajustes.volumen = doc["volumen"] | ajustes.volumen;
    ajustes.reposo_s = doc["reposo"] | ajustes.reposo_s;
    ajustes.reanudar = doc["reanudar"] | ajustes.reanudar;
    if (doc["ultima"].is<JsonObjectConst>()) {
        de_json(doc["ultima"], &ajustes.ultima);
        ajustes.hay_ultima = ajustes.ultima.emisora.url[0] != '\0';
    }
    for (JsonObjectConst f : doc["favoritos"].as<JsonArrayConst>()) {
        if (ajustes.num_favoritos >= MAX_FAVORITOS) {
            break;
        }
        de_json(f, &ajustes.favoritos[ajustes.num_favoritos++]);
    }
}

void ajustes_guardar(void)
{
    JsonDocument doc;
    doc["volumen"] = ajustes.volumen;
    doc["reposo"] = ajustes.reposo_s;
    doc["reanudar"] = ajustes.reanudar;
    if (ajustes.hay_ultima) {
        a_json(doc["ultima"].to<JsonObject>(), &ajustes.ultima);
    }
    JsonArray favoritos = doc["favoritos"].to<JsonArray>();
    for (uint8_t i = 0; i < ajustes.num_favoritos; i++) {
        a_json(favoritos.add<JsonObject>(), &ajustes.favoritos[i]);
    }

    size_t tam = measureJsonPretty(doc) + 1;
    char *texto = (char *)malloc(tam);
    if (texto == NULL) {
        return;
    }
    serializeJsonPretty(doc, texto, tam);
    if (!hal_ajustes_guardar(texto)) {
        fprintf(stderr, "No se pudieron guardar los ajustes\n");
    }
    free(texto);
}

void ajustes_recordar_ultima(const Emisora *emisora, const Pais *pais)
{
    guardada(&ajustes.ultima, emisora, pais);
    ajustes.hay_ultima = true;
    ajustes_guardar();
}

static int buscar_favorito(const char *url)
{
    for (uint8_t i = 0; i < ajustes.num_favoritos; i++) {
        if (strcmp(ajustes.favoritos[i].emisora.url, url) == 0) {
            return i;
        }
    }
    return -1;
}

bool ajustes_es_favorito(const char *url)
{
    return buscar_favorito(url) >= 0;
}

bool ajustes_alternar_favorito(const Emisora *emisora, const Pais *pais)
{
    int i = buscar_favorito(emisora->url);
    if (i >= 0) {
        /* Quitar: se corre el resto de la lista una posición */
        memmove(&ajustes.favoritos[i], &ajustes.favoritos[i + 1],
                (ajustes.num_favoritos - i - 1) * sizeof(EmisoraGuardada));
        ajustes.num_favoritos--;
        ajustes_guardar();
        return false;
    }
    if (ajustes.num_favoritos >= MAX_FAVORITOS) {
        return false;
    }
    guardada(&ajustes.favoritos[ajustes.num_favoritos++], emisora, pais);
    ajustes_guardar();
    return true;
}
