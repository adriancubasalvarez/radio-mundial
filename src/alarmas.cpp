#include "alarmas.h"
#include "ajustes.h"
#include "hal.h"
#include "idioma.h"
#include "ui/nav.h"
#include "ui/ui.h"

#include <stdio.h>
#include <string.h>

#define PERIODO_MS 1000  /* cada cuánto se mira el reloj */

#define LUN_A_VIE 0x3E   /* bits 1..5 */
#define FIN_DE_SEMANA 0x41  /* bits 0 y 6 */
#define TODOS 0x7F

static uint32_t ultimo_ms;
static int ultimo_minuto = -1;  /* para no disparar dos veces en el mismo minuto */
static bool sonando;
static bool puso_audio;         /* la radio estaba callada y la encendió la alarma */

/* Enciende la última emisora que sonó; si ya suena algo, se deja como está */
static void poner_audio(void)
{
    EstadoAudio estado = audio_estado();
    bool ya_suena = nav.hay_sonando && (estado == AUDIO_SONANDO || estado == AUDIO_CONECTANDO);
    puso_audio = false;
    if (ya_suena || !ajustes.hay_ultima) {
        return;
    }
    nav.emisora = ajustes.ultima.emisora;
    memset(&nav.pais, 0, sizeof(nav.pais));
    strncpy(nav.pais.codigo, ajustes.ultima.pais, sizeof(nav.pais.codigo) - 1);
    strncpy(nav.pais.nombre, ajustes.ultima.nombre_pais, sizeof(nav.pais.nombre) - 1);
    nav.aleatoria = false;
    nav_reproducir();
    puso_audio = true;
}

static void disparar(int hora, int minuto)
{
    printf("Alarma de las %02d:%02d\n", hora, minuto);
    sonando = true;
    reposo_despertar();
    poner_audio();
    alarma_mostrar(hora, minuto);
}

void alarmas_atender(void)
{
    uint32_t ahora = hal_millis();
    if (ahora - ultimo_ms < PERIODO_MS) {
        return;
    }
    ultimo_ms = ahora;

    int hora, minuto, anio, mes, dia, semana;
    if (!hal_hora(&hora, &minuto) || !hal_fecha(&anio, &mes, &dia, &semana)) {
        return;  /* aún no se sabe la hora */
    }
    int este_minuto = hora * 60 + minuto;
    if (este_minuto == ultimo_minuto) {
        return;
    }
    ultimo_minuto = este_minuto;

    bool hay = false, cambio = false;
    for (uint8_t i = 0; i < ajustes.num_alarmas; i++) {
        Alarma *a = &ajustes.alarmas[i];
        if (!a->activa || a->hora != hora || a->minuto != minuto) {
            continue;
        }
        if (a->dias == 0) {
            a->activa = false;  /* «una vez»: se apaga sola */
            cambio = true;
            hay = true;
        } else if (a->dias & (1 << semana)) {
            hay = true;
        }
    }
    if (cambio) {
        ajustes_guardar();
    }
    if (hay && !sonando) {
        disparar(hora, minuto);
    }
}

void alarmas_apagar(void)
{
    sonando = false;
    if (puso_audio) {
        audio_parar();
        nav.hay_sonando = false;
        puso_audio = false;
    }
}

bool alarmas_sonando(void)
{
    return sonando;
}

const char *alarmas_texto_dias(uint8_t dias)
{
    static char texto[24];
    if (dias == 0) return TR("Once", "Una vez");
    if (dias == TODOS) return TR("Every day", "Todos los días");
    if (dias == LUN_A_VIE) return TR("Mon to Fri", "Lun a vie");
    if (dias == FIN_DE_SEMANA) return TR("Weekend", "Fin de semana");

    /* De lunes a domingo; los bits van del domingo (0) al sábado (6) */
    static const uint8_t ORDEN[] = {1, 2, 3, 4, 5, 6, 0};
    const char *letras = TR("MTWTFSS", "LMXJVSD");
    char *p = texto;
    for (int i = 0; i < 7; i++) {
        if (dias & (1 << ORDEN[i])) {
            *p++ = letras[i];
            *p++ = ' ';
        }
    }
    p[p > texto ? -1 : 0] = '\0';
    return texto;
}
