/**
 * Archivos en la placa.
 *
 *   - Memoria interna: LittleFS (se sube con «pio run -e cyd -t uploadfs»).
 *   - Tarjeta microSD opcional (FAT32). Los datos se buscan primero en su
 *     carpeta /radio/ y luego en LittleFS.
 *   - Ajustes: NVS (Non-Volatile Storage) con la librería Preferences.
 */
#include "hal.h"
#include "placa.h"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>

/* Archivo abierto con un búfer propio: leer de LittleFS byte a byte es muy
 * lento (buscar en buscar.txt tardaba 2 s), por bloques es mucho más rápido */
#define TAM_BUFER_ARCHIVO 512

struct HalArchivo {
    File f;
    uint8_t bufer[TAM_BUFER_ARCHIVO];
    size_t pos;     /* siguiente byte a entregar del búfer */
    size_t largo;   /* bytes válidos en el búfer */
};

/* Siguiente byte del archivo, o -1 al final */
static int siguiente_byte(HalArchivo *a)
{
    if (a->pos >= a->largo) {
        a->largo = a->f.read(a->bufer, sizeof(a->bufer));
        a->pos = 0;
        if (a->largo == 0) {
            return -1;
        }
    }
    return a->bufer[a->pos++];
}

static bool sd_montada = false;
static SPIClass bus_sd(VSPI);

/* Monta LittleFS y, si hay tarjeta, la SD. Lo llama main_esp32.cpp. */
bool archivos_iniciar(void)
{
    bool interna = LittleFS.begin(false);
    if (!interna) {
        Serial.println("No se pudo montar LittleFS: ¿has subido los datos (uploadfs)?");
    }
    Serial.printf("[memoria] tras LittleFS: libre %u, DMA libre %u\n", (unsigned)ESP.getFreeHeap(),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA));
#ifdef SIN_SD
    /* La SD gasta unos 28 KB de RAM: en placas sin PSRAM se reservan para el audio */
    Serial.println("Tarjeta SD: desactivada en esta placa");
#else
    bus_sd.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    sd_montada = SD.begin(PIN_SD_CS, bus_sd, 20000000) && SD.cardType() != CARD_NONE;
    Serial.printf("Tarjeta SD: %s\n", sd_montada ? "sí" : "no");
#endif
    return interna;
}

bool hal_sd_presente(void)
{
    return sd_montada;
}

/* Abre un archivo de datos: primero en la SD (/radio/...), luego en LittleFS */
static File abrir_datos(const char *ruta)
{
    String r = String("/") + ruta;
    if (sd_montada) {
        String en_sd = "/radio" + r;
        if (SD.exists(en_sd)) {
            return SD.open(en_sd, FILE_READ);
        }
    }
    return LittleFS.open(r, FILE_READ);
}

static uint8_t *leer_entero(File &f, size_t *tam)
{
    size_t largo = f.size();
    uint8_t *datos = (uint8_t *)malloc(largo + 1);
    if (datos != NULL) {
        size_t leidos = f.read(datos, largo);
        datos[leidos] = 0; /* por comodidad, si es texto queda terminado en cero */
        *tam = leidos;
    }
    f.close();
    return datos;
}

uint8_t *hal_leer_archivo(const char *ruta, size_t *tam)
{
    File f = abrir_datos(ruta);
    if (!f) {
        Serial.printf("No se encuentra %s\n", ruta);
        return NULL;
    }
    return leer_entero(f, tam);
}

HalArchivo *hal_archivo_abrir(const char *ruta)
{
    File f = abrir_datos(ruta);
    if (!f) {
        return NULL;
    }
    HalArchivo *a = new HalArchivo;
    a->f = f;
    a->pos = 0;
    a->largo = 0;
    return a;
}

bool hal_archivo_linea(HalArchivo *archivo, char *linea, size_t tam)
{
    int c = siguiente_byte(archivo);
    if (c < 0) {
        return false;
    }
    size_t n = 0;
    while (c >= 0 && c != '\n') {
        if (n < tam - 1) {
            linea[n++] = (char)c;  /* si la línea no cabe, se descarta el resto */
        }
        c = siguiente_byte(archivo);
    }
    while (n > 0 && linea[n - 1] == '\r') {
        n--;
    }
    linea[n] = '\0';
    return true;
}

size_t hal_archivo_leer(HalArchivo *archivo, void *destino, size_t n)
{
    uint8_t *d = (uint8_t *)destino;
    size_t leidos = 0;
    while (leidos < n) {
        if (archivo->pos >= archivo->largo) {
            archivo->largo = archivo->f.read(archivo->bufer, sizeof(archivo->bufer));
            archivo->pos = 0;
            if (archivo->largo == 0) {
                break;
            }
        }
        size_t trozo = archivo->largo - archivo->pos;
        if (trozo > n - leidos) {
            trozo = n - leidos;
        }
        memcpy(d + leidos, archivo->bufer + archivo->pos, trozo);
        archivo->pos += trozo;
        leidos += trozo;
    }
    return leidos;
}

bool hal_archivo_leer_en(HalArchivo *archivo, uint32_t pos, void *destino, size_t n)
{
    archivo->pos = archivo->largo = 0;  /* el búfer deja de valer al saltar */
    return archivo->f.seek(pos) && archivo->f.read((uint8_t *)destino, n) == n;
}

void hal_archivo_cerrar(HalArchivo *archivo)
{
    if (archivo != NULL) {
        archivo->f.close();
        delete archivo;
    }
}

uint8_t *hal_sd_leer(const char *ruta, size_t *tam)
{
    if (!sd_montada) {
        return NULL;
    }
    File f = SD.open(String("/") + ruta, FILE_READ);
    return f ? leer_entero(f, tam) : NULL;
}

bool hal_sd_escribir(const char *ruta, const char *texto)
{
    if (!sd_montada) {
        return false;
    }
    File f = SD.open(String("/") + ruta, FILE_WRITE);
    if (!f) {
        return false;
    }
    f.print(texto);
    f.close();
    return true;
}

/* ---------- Ajustes en NVS ---------- */

static Preferences preferencias;

char *hal_ajustes_leer(void)
{
    preferencias.begin("radio", true);
    size_t tam = preferencias.getBytesLength("ajustes");
    char *texto = NULL;
    if (tam > 0) {
        texto = (char *)malloc(tam + 1);
        if (texto != NULL) {
            preferencias.getBytes("ajustes", texto, tam);
            texto[tam] = '\0';
        }
    }
    preferencias.end();
    return texto;
}

bool hal_ajustes_guardar(const char *json)
{
    preferencias.begin("radio", false);
    size_t escritos = preferencias.putBytes("ajustes", json, strlen(json));
    preferencias.end();
    return escritos > 0;
}
