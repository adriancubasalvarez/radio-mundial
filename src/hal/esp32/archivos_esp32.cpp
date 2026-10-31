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

struct HalArchivo {
    File f;
};

static bool sd_montada = false;
static SPIClass bus_sd(VSPI);

/* Monta LittleFS y, si hay tarjeta, la SD. Lo llama main_esp32.cpp. */
bool archivos_iniciar(void)
{
    bool interna = LittleFS.begin(false);
    if (!interna) {
        Serial.println("No se pudo montar LittleFS: ¿has subido los datos (uploadfs)?");
    }
    bus_sd.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    sd_montada = SD.begin(PIN_SD_CS, bus_sd, 20000000) && SD.cardType() != CARD_NONE;
    Serial.printf("Tarjeta SD: %s\n", sd_montada ? "sí" : "no");
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
    return new HalArchivo{f};
}

bool hal_archivo_linea(HalArchivo *archivo, char *linea, size_t tam)
{
    if (!archivo->f.available()) {
        return false;
    }
    size_t n = archivo->f.readBytesUntil('\n', linea, tam - 1);
    linea[n] = '\0';
    if (n == tam - 1) {
        /* Línea más larga que el búfer: descartar el resto */
        while (archivo->f.available() && archivo->f.read() != '\n') {
        }
    }
    while (n > 0 && linea[n - 1] == '\r') {
        linea[--n] = '\0';
    }
    return true;
}

bool hal_archivo_leer_en(HalArchivo *archivo, uint32_t pos, void *destino, size_t n)
{
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
