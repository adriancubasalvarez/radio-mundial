/**
 * «Mis emisoras» por el USB: la página de instalación lee y escribe la
 * lista de emisoras propias con órdenes de texto por el puerto serie.
 *
 * Conviven con Improv (que usa paquetes binarios que empiezan por «IMPROV»):
 * cada byte que llega va a los dos, y aquí solo cuentan las líneas que
 * empiezan por «RM». Las respuestas también empiezan por «RM», para que la
 * página las distinga del resto del registro de la radio.
 *
 *   RM?              la radio responde una línea «RM=» por emisora y «RM.»
 *   RM-              empieza una lista nueva (aún no se guarda)
 *   RM+n\tu\tc\tb    añade una emisora: nombre, URL, códec y kbps
 *                    (la radio responde «RM>» y cuántas lleva)
 *   RM!              guarda la lista nueva («RMOK» y cuántas, o «RMERR»)
 *
 * Una emisora por línea y una respuesta a cada una: así nunca se llena el
 * búfer de entrada del USB, aunque la radio esté ocupada pintando.
 */
#include "hal.h"
#include "datos.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#define MAX_LINEA 400  /* nombre (64) + URL (256) + códec y kbps, con margen */

static char linea[MAX_LINEA];
static size_t largo;
static bool desbordada;          /* línea demasiado larga: se descarta */
static JsonDocument *nueva;      /* lista que llega, hasta «RM!» */

/* Corta «texto» por el tabulador: devuelve el trozo y avanza */
static char *trozo(char **texto)
{
    char *inicio = *texto;
    char *tab = strchr(inicio, '\t');
    if (tab != NULL) {
        *tab = '\0';
        *texto = tab + 1;
    } else {
        *texto = inicio + strlen(inicio);
    }
    return inicio;
}

static void enviar_lista(void)
{
    char *texto = hal_mis_emisoras_leer();
    JsonDocument doc;
    if (texto != NULL) {
        deserializeJson(doc, texto);
        free(texto);
    }
    for (JsonObjectConst e : doc.as<JsonArrayConst>()) {
        Serial.printf("RM=%s\t%s\t%s\t%d\n", (const char *)(e["n"] | ""), (const char *)(e["u"] | ""),
                      (const char *)(e["c"] | ""), (int)(e["b"] | 0));
    }
    Serial.println("RM.");
}

static void anadir(char *resto)
{
    if (nueva == NULL) {
        Serial.println("RMERR sin RM-");
        return;
    }
    JsonArray lista = nueva->as<JsonArray>();
    const char *nombre = trozo(&resto);
    const char *url = trozo(&resto);
    const char *codec = trozo(&resto);
    int kbps = atoi(trozo(&resto));
    if (lista.size() >= MAX_MIS_EMISORAS || nombre[0] == '\0' ||
        (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) || strlen(url) > 255) {
        Serial.println("RMERR emisora no válida");
        return;
    }
    JsonObject e = lista.add<JsonObject>();
    e["n"] = String(nombre);  /* String(): que ArduinoJson copie el texto, «linea» se reutiliza */
    e["u"] = String(url);
    if (codec[0] != '\0') {
        e["c"] = String(codec);
    }
    if (kbps > 0) {
        e["b"] = kbps;
    }
    Serial.printf("RM>%u\n", (unsigned)lista.size());
}

static void guardar(void)
{
    if (nueva == NULL) {
        Serial.println("RMERR sin RM-");
        return;
    }
    String texto;
    serializeJson(*nueva, texto);
    size_t num = nueva->as<JsonArray>().size();
    delete nueva;
    nueva = NULL;
    if (hal_mis_emisoras_guardar(texto.c_str())) {
        Serial.printf("RMOK %u\n", (unsigned)num);
    } else {
        Serial.println("RMERR no se pudo guardar");
    }
}

static void orden(void)
{
    if (linea[0] != 'R' || linea[1] != 'M') {
        return;  /* otra cosa (o un paquete Improv): no es para nosotros */
    }
    switch (linea[2]) {
    case '?':
        enviar_lista();
        break;
    case '-':
        delete nueva;
        nueva = new JsonDocument;
        nueva->to<JsonArray>();
        Serial.println("RM>0");
        break;
    case '+':
        anadir(linea + 3);
        break;
    case '!':
        guardar();
        break;
    default:
        break;
    }
}

/* Lo llama red_esp32.cpp con cada byte que llega por el USB */
void mis_emisoras_usb_byte(uint8_t b)
{
    if (b == '\n' || b == '\r') {
        if (largo > 0 && !desbordada) {
            linea[largo] = '\0';
            orden();
        }
        largo = 0;
        desbordada = false;
        return;
    }
    if (largo + 1 >= sizeof(linea)) {
        desbordada = true;
        return;
    }
    linea[largo++] = (char)b;
}
