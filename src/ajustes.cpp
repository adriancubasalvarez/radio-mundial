#include "ajustes.h"
#include "hal.h"
#include "luz.h"
#include "idioma.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Ajustes ajustes;

/* Idioma de la interfaz (ver idioma.h); inglés por defecto */
bool idioma_es = false;

/* Copia de seguridad en la raíz de la tarjeta SD (si hay tarjeta). Sirve
 * para no perder favoritos al reflashear la placa o para pasarlos a otra
 * radio: si la memoria interna está vacía, se recupera de aquí. */
#define COPIA_SD "radio_mundial_ajustes.json"

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
    ajustes.luz = LUZ_RITMO;
    ajustes.luz_brillo = 0;

    char *texto = hal_ajustes_leer();
    bool de_la_sd = false;
    if (texto == NULL) {
        size_t tam = 0;
        texto = (char *)hal_sd_leer(COPIA_SD, &tam);
        de_la_sd = texto != NULL;
    }
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
    copiar(ajustes.version_emisoras, sizeof(ajustes.version_emisoras), doc["v_emisoras"]);
    ajustes.luz = doc["luz"] | ajustes.luz;
    if (ajustes.luz >= NUM_MODOS_LUZ) {
        ajustes.luz = LUZ_RITMO;
    }
    ajustes.luz_brillo = doc["luz_brillo"] | ajustes.luz_brillo;
    if (ajustes.luz_brillo >= NUM_BRILLOS_LUZ) {
        ajustes.luz_brillo = 0;
    }
    /* Idioma: "en" o "es"; si no está, el primer arranque lo pregunta */
    const char *idioma = doc["idioma"] | "";
    ajustes.idioma_elegido = idioma[0] != '\0';
    ajustes.espanol = strcmp(idioma, "es") == 0;
    idioma_es = ajustes.espanol;
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
    for (JsonArrayConst f : doc["fallos"].as<JsonArrayConst>()) {
        if (ajustes.num_fallos >= MAX_FALLOS) {
            break;
        }
        ajustes.fallos[ajustes.num_fallos].resumen = f[0] | 0u;
        ajustes.fallos[ajustes.num_fallos++].veces = f[1] | 1;
    }
    for (JsonObjectConst r : doc["recientes"].as<JsonArrayConst>()) {
        if (ajustes.num_recientes >= MAX_RECIENTES) {
            break;
        }
        de_json(r, &ajustes.recientes[ajustes.num_recientes++]);
    }
    for (JsonObjectConst o : doc["alarmas"].as<JsonArrayConst>()) {
        if (ajustes.num_alarmas >= MAX_ALARMAS) {
            break;
        }
        Alarma *a = &ajustes.alarmas[ajustes.num_alarmas++];
        a->hora = o["h"] | 7;
        a->minuto = o["m"] | 0;
        a->dias = o["d"] | 0;
        a->activa = o["a"] | false;
        a->volumen = o["v"] | 85;
        a->amanecer = o["s"] | false;
        if (o["e"].is<JsonObjectConst>()) {
            de_json(o["e"], &a->emisora);
            a->con_emisora = true;
        }
    }

    if (de_la_sd) {
        printf("Ajustes recuperados de la tarjeta SD (%d favoritos)\n", ajustes.num_favoritos);
        ajustes_guardar(); /* pasarlos también a la memoria interna */
    }
}

void ajustes_guardar(void)
{
    JsonDocument doc;
    doc["volumen"] = ajustes.volumen;
    doc["reposo"] = ajustes.reposo_s;
    doc["reanudar"] = ajustes.reanudar;
    doc["v_emisoras"] = ajustes.version_emisoras;
    doc["luz"] = ajustes.luz;
    doc["luz_brillo"] = ajustes.luz_brillo;
    if (ajustes.idioma_elegido) {
        doc["idioma"] = ajustes.espanol ? "es" : "en";
    }
    if (ajustes.hay_ultima) {
        a_json(doc["ultima"].to<JsonObject>(), &ajustes.ultima);
    }
    JsonArray fallos = doc["fallos"].to<JsonArray>();
    for (uint8_t i = 0; i < ajustes.num_fallos; i++) {
        JsonArray f = fallos.add<JsonArray>();
        f.add(ajustes.fallos[i].resumen);
        f.add(ajustes.fallos[i].veces);
    }
    JsonArray recientes = doc["recientes"].to<JsonArray>();
    for (uint8_t i = 0; i < ajustes.num_recientes; i++) {
        a_json(recientes.add<JsonObject>(), &ajustes.recientes[i]);
    }
    JsonArray favoritos = doc["favoritos"].to<JsonArray>();
    for (uint8_t i = 0; i < ajustes.num_favoritos; i++) {
        a_json(favoritos.add<JsonObject>(), &ajustes.favoritos[i]);
    }
    JsonArray alarmas = doc["alarmas"].to<JsonArray>();
    for (uint8_t i = 0; i < ajustes.num_alarmas; i++) {
        JsonObject o = alarmas.add<JsonObject>();
        o["h"] = ajustes.alarmas[i].hora;
        o["m"] = ajustes.alarmas[i].minuto;
        o["d"] = ajustes.alarmas[i].dias;
        o["a"] = ajustes.alarmas[i].activa;
        o["v"] = ajustes.alarmas[i].volumen;
        o["s"] = ajustes.alarmas[i].amanecer;
        if (ajustes.alarmas[i].con_emisora) {
            a_json(o["e"].to<JsonObject>(), &ajustes.alarmas[i].emisora);
        }
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
    hal_sd_escribir(COPIA_SD, texto); /* copia en la SD; si no hay tarjeta no hace nada */
    free(texto);
}

void ajustes_poner_idioma(bool espanol)
{
    ajustes.espanol = espanol;
    ajustes.idioma_elegido = true;
    idioma_es = espanol;
    ajustes_guardar();
}

/* La pone la primera de Recientes; si ya estaba, no se repite */
static void recordar_reciente(const EmisoraGuardada *e)
{
    int pos = -1;
    for (int i = 0; i < ajustes.num_recientes; i++) {
        if (strcmp(ajustes.recientes[i].emisora.url, e->emisora.url) == 0) {
            pos = i;
            break;
        }
    }
    if (pos < 0) {
        if (ajustes.num_recientes < MAX_RECIENTES) {
            ajustes.num_recientes++;
        }
        pos = ajustes.num_recientes - 1;
    }
    memmove(&ajustes.recientes[1], &ajustes.recientes[0], pos * sizeof(ajustes.recientes[0]));
    ajustes.recientes[0] = *e;
}

void ajustes_recordar_ultima(const Emisora *emisora, const Pais *pais)
{
    guardada(&ajustes.ultima, emisora, pais);
    ajustes.hay_ultima = true;
    recordar_reciente(&ajustes.ultima);
    ajustes_guardar();
}

#define FALLOS_PARA_BAJAR 2

/* Resumen de la URL (hash FNV-1a, Fowler-Noll-Vo): con 40 emisoras en la
 * lista, dos URL distintas casi nunca darán el mismo */
static uint32_t resumen_url(const char *url)
{
    uint32_t h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)url; *p; p++) {
        h = (h ^ *p) * 16777619u;
    }
    return h;
}

static int buscar_fallo(const char *url)
{
    uint32_t resumen = resumen_url(url);
    for (int i = 0; i < ajustes.num_fallos; i++) {
        if (ajustes.fallos[i].resumen == resumen) {
            return i;
        }
    }
    return -1;
}

static void quitar_fallo(int pos)
{
    memmove(&ajustes.fallos[pos], &ajustes.fallos[pos + 1],
            (ajustes.num_fallos - pos - 1) * sizeof(ajustes.fallos[0]));
    ajustes.num_fallos--;
}

void ajustes_anotar_fallo(const char *url)
{
    int pos = buscar_fallo(url);
    if (pos >= 0) {
        if (ajustes.fallos[pos].veces < 255) {
            ajustes.fallos[pos].veces++;
        }
    } else {
        if (ajustes.num_fallos >= MAX_FALLOS) {
            quitar_fallo(0);  /* la lista llena olvida el más antiguo */
        }
        ajustes.fallos[ajustes.num_fallos].resumen = resumen_url(url);
        ajustes.fallos[ajustes.num_fallos++].veces = 1;
    }
    ajustes_guardar();
}

void ajustes_olvidar_fallo(const char *url)
{
    int pos = buscar_fallo(url);
    if (pos >= 0) {
        quitar_fallo(pos);
        ajustes_guardar();
    }
}

bool ajustes_emisora_floja(const char *url)
{
    int pos = buscar_fallo(url);
    return pos >= 0 && ajustes.fallos[pos].veces >= FALLOS_PARA_BAJAR;
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
