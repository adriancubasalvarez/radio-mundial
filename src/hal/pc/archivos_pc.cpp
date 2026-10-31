/**
 * Archivos del simulador.
 *
 *   - Memoria interna: carpeta data/ del proyecto (variable RADIO_DATA).
 *   - Tarjeta SD: carpeta sd/ del proyecto (variable RADIO_SD). Si la
 *     carpeta existe, el simulador hace como si hubiera tarjeta.
 *
 * Los datos se buscan primero en sd/radio/ y luego en data/.
 */
#include "hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>

struct HalArchivo {
    FILE *f;
};

static std::string carpeta_data(void)
{
    const char *base = getenv("RADIO_DATA");
    return base ? base : "data";
}

static std::string carpeta_sd(void)
{
    const char *base = getenv("RADIO_SD");
    return base ? base : "sd";
}

bool hal_sd_presente(void)
{
    struct stat info;
    return stat(carpeta_sd().c_str(), &info) == 0 && (info.st_mode & S_IFDIR);
}

/* Abre un archivo de datos: primero en la SD (sd/radio/), luego en data/ */
static FILE *abrir_datos(const char *ruta)
{
    if (hal_sd_presente()) {
        FILE *f = fopen((carpeta_sd() + "/radio/" + ruta).c_str(), "rb");
        if (f != NULL) {
            return f;
        }
    }
    return fopen((carpeta_data() + "/" + ruta).c_str(), "rb");
}

static uint8_t *leer_entero(FILE *f, size_t *tam)
{
    fseek(f, 0, SEEK_END);
    long largo = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *datos = (uint8_t *)malloc(largo + 1);
    if (datos != NULL) {
        size_t leidos = fread(datos, 1, largo, f);
        datos[leidos] = 0; /* por comodidad, si es texto queda terminado en cero */
        *tam = leidos;
    }
    fclose(f);
    return datos;
}

uint8_t *hal_leer_archivo(const char *ruta, size_t *tam)
{
    FILE *f = abrir_datos(ruta);
    if (f == NULL) {
        fprintf(stderr, "No se encuentra %s\n", ruta);
        return NULL;
    }
    return leer_entero(f, tam);
}

HalArchivo *hal_archivo_abrir(const char *ruta)
{
    FILE *f = abrir_datos(ruta);
    if (f == NULL) {
        return NULL;
    }
    HalArchivo *archivo = (HalArchivo *)malloc(sizeof(HalArchivo));
    archivo->f = f;
    return archivo;
}

bool hal_archivo_linea(HalArchivo *archivo, char *linea, size_t tam)
{
    if (fgets(linea, (int)tam, archivo->f) == NULL) {
        return false;
    }
    size_t largo = strlen(linea);
    if (largo > 0 && linea[largo - 1] != '\n' && !feof(archivo->f)) {
        /* Línea más larga que el búfer: descartar el resto */
        int c;
        while ((c = fgetc(archivo->f)) != EOF && c != '\n') {
        }
    }
    while (largo > 0 && (linea[largo - 1] == '\n' || linea[largo - 1] == '\r')) {
        linea[--largo] = '\0';
    }
    return true;
}

bool hal_archivo_leer_en(HalArchivo *archivo, uint32_t pos, void *destino, size_t n)
{
    return fseek(archivo->f, (long)pos, SEEK_SET) == 0 && fread(destino, 1, n, archivo->f) == n;
}

void hal_archivo_cerrar(HalArchivo *archivo)
{
    if (archivo != NULL) {
        fclose(archivo->f);
        free(archivo);
    }
}

uint8_t *hal_sd_leer(const char *ruta, size_t *tam)
{
    if (!hal_sd_presente()) {
        return NULL;
    }
    FILE *f = fopen((carpeta_sd() + "/" + ruta).c_str(), "rb");
    return f ? leer_entero(f, tam) : NULL;
}

bool hal_sd_escribir(const char *ruta, const char *texto)
{
    if (!hal_sd_presente()) {
        return false;
    }
    FILE *f = fopen((carpeta_sd() + "/" + ruta).c_str(), "wb");
    if (f == NULL) {
        return false;
    }
    fputs(texto, f);
    fclose(f);
    return true;
}
