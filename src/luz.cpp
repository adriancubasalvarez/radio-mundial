#include "luz.h"
#include "ajustes.h"
#include "hal.h"
#include "ui/nav.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PERIODO_MS     20     /* cada cuánto se recalcula el LED */
#define BRILLO         0.6f   /* intensidad máxima del LED (0..1) */
#define DESTELLOS_MS   1500   /* tres destellos rojos al caerse una emisora */
#define COLOR_SIN_REGION 0xFFC845  /* ámbar de la interfaz */
#define ARCOIRIS_MS    20000  /* una vuelta entera al arcoíris en modo ritmo */

/* Paleta del mapamundi (.bin de LVGL): tras la cabecera de 4 bytes, 256
 * colores en orden azul, verde, rojo, alfa (ver tools/build_maps.py) */
#define INICIO_PALETA 4

static uint32_t color_emisora = COLOR_SIN_REGION;
static uint32_t ultimo_ms;
static uint32_t error_desde;
static EstadoAudio estado_antes = AUDIO_PARADO;
static float envolvente;        /* nivel de la música suavizado, 0..1 */
static float maximo = 1.0f;     /* máximo reciente: el ritmo no depende del volumen */

/* Color puro (saturado) de un tono, 0..1 alrededor del círculo cromático */
static uint32_t tono(float h)
{
    float x = h * 6.0f;
    int sector = (int)x % 6;
    uint8_t sube = (uint8_t)((x - (int)x) * 255);
    uint8_t baja = 255 - sube;
    switch (sector) {
    case 0: return 0xFF0000 | sube << 8;      /* rojo -> amarillo */
    case 1: return (uint32_t)baja << 16 | 0xFF00;  /* amarillo -> verde */
    case 2: return 0x00FF00 | sube;           /* verde -> cian */
    case 3: return (uint32_t)baja << 8 | 0xFF;     /* cian -> azul */
    case 4: return (uint32_t)sube << 16 | 0xFF;    /* azul -> magenta */
    default: return 0xFF0000 | baja;          /* magenta -> rojo */
    }
}

/* Los colores del mapa son suaves (pastel) y en el LED se ven casi blancos:
 * se pasa al color puro del mismo tono */
static uint32_t saturar(uint32_t color)
{
    float r = (color >> 16 & 0xFF) / 255.0f, g = (color >> 8 & 0xFF) / 255.0f, b = (color & 0xFF) / 255.0f;
    float max = fmaxf(r, fmaxf(g, b)), min = fminf(r, fminf(g, b)), d = max - min;
    if (d < 0.02f) {
        return COLOR_SIN_REGION;  /* gris: sin tono que saturar */
    }
    float h = max == r ? (g - b) / d : max == g ? 2 + (b - r) / d : 4 + (r - g) / d;
    h /= 6.0f;
    return tono(h < 0 ? h + 1 : h);
}

/* Color de una región: el de su índice en la paleta del mapamundi */
static uint32_t color_region(const Region *region)
{
    HalArchivo *archivo = hal_archivo_abrir(nav.indice.imagen);
    if (archivo == NULL) {
        return COLOR_SIN_REGION;
    }
    uint8_t bgra[4];
    bool ok = hal_archivo_leer_en(archivo, INICIO_PALETA + region->indice * 4, bgra, 4);
    hal_archivo_cerrar(archivo);
    return ok ? saturar((uint32_t)bgra[2] << 16 | bgra[1] << 8 | bgra[0]) : COLOR_SIN_REGION;
}

void luz_emisora(const char *codigo_pais)
{
    /* La región se busca en regiones/<id>.json (1–3 KB cada una) */
    char buscado[20];
    snprintf(buscado, sizeof(buscado), "\"codigo\":\"%s\"", codigo_pais);
    color_emisora = COLOR_SIN_REGION;
    for (uint8_t i = 0; i < nav.indice.num_regiones; i++) {
        const Region *region = &nav.indice.regiones[i];
        char ruta[48];
        snprintf(ruta, sizeof(ruta), "regiones/%s.json", region->id);
        size_t tam = 0;
        char *texto = (char *)hal_leer_archivo(ruta, &tam);
        if (texto == NULL) {
            continue;
        }
        bool esta = strstr(texto, buscado) != NULL;
        free(texto);
        if (esta) {
            color_emisora = color_region(region);
            return;
        }
    }
}

/* Respiración: de 0,1 a 1 y vuelta en periodo_ms */
static float respirar(uint32_t ahora, uint32_t periodo_ms)
{
    float fase = (float)(ahora % periodo_ms) / periodo_ms;
    return 0.1f + 0.9f * (0.5f - 0.5f * cosf(6.2831853f * fase));
}

static void encender(uint32_t color, float k)
{
    k *= BRILLO;
    hal_led((uint8_t)((color >> 16 & 0xFF) * k), (uint8_t)((color >> 8 & 0xFF) * k), (uint8_t)((color & 0xFF) * k));
}

/* «Ojo mágico»: sube de golpe con cada golpe de música y baja despacio */
static float ritmo(void)
{
    float nivel = audio_nivel() / 255.0f;
    maximo = fmaxf(nivel, maximo * 0.995f);
    if (maximo < 0.05f) {
        maximo = 0.05f;
    }
    float relativo = nivel / maximo;
    envolvente = relativo > envolvente ? relativo : envolvente * 0.85f + relativo * 0.15f;
    return 0.15f + 0.85f * envolvente * envolvente;
}

void luz_atender(void)
{
    uint32_t ahora = hal_millis();
    if (ahora - ultimo_ms < PERIODO_MS) {
        return;
    }
    ultimo_ms = ahora;

    EstadoAudio estado = audio_estado();
    if (estado == AUDIO_ERROR && estado_antes != AUDIO_ERROR) {
        error_desde = ahora;
    }
    estado_antes = estado;

    if (ajustes.luz == LUZ_APAGADA) {
        encender(0, 0);
    } else if (hal_red_estado() == RED_PORTAL) {
        encender(0x0050FF, respirar(ahora, 2000));
    } else if (estado == AUDIO_ERROR) {
        uint32_t t = ahora - error_desde;
        bool destello = t < DESTELLOS_MS && t % 500 < 200;
        encender(0xFF0000, destello ? 1.0f : 0.0f);
    } else if (estado == AUDIO_CONECTANDO || (nav.hay_sonando && hal_red_estado() == RED_CONECTANDO)) {
        encender(0xFF8C00, respirar(ahora, 1500));
    } else if (estado == AUDIO_SONANDO) {
        if (ajustes.luz == LUZ_RITMO) {
            /* el arcoíris gira despacio y el brillo late con la música */
            encender(tono((float)(ahora % ARCOIRIS_MS) / ARCOIRIS_MS), ritmo());
        } else {
            encender(color_emisora, 1.0f);
        }
    } else {
        encender(0, 0);
    }
}
