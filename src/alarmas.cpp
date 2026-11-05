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

#define ESPERA_EMISORA_MS 15000UL  /* si no arranca en este tiempo, tono */
#define RAMPA_MS          30000UL  /* la emisora llega a su volumen en 30 s */
#define RAMPA_TONO_MS     15000UL  /* el tono, antes: tiene que despertar */
#define TOPE_MS (30UL * 60 * 1000)  /* sin apagarla, se para sola a la media hora */
/* Empieza 30 puntos por debajo (unos 14 dB). El volumen va en decibelios:
 * un porcentaje del final, como el 30 %, apenas se oiría. */
#define BAJADA_INICIAL    30
#define VOLUMEN_MINIMO    25

typedef enum {
    SUENA_MUSICA,   /* ya sonaba algo: se deja como está */
    SUENA_EMISORA,
    SUENA_TONO,
} Sonido;

static uint32_t ultimo_ms;
static int ultimo_minuto = -1;  /* para no disparar dos veces en el mismo minuto */
static bool sonando;
static Sonido sonido;
static uint32_t desde_ms;       /* cuándo saltó la alarma */
static bool subiendo;           /* ya hay sonido y el volumen va subiendo */
static uint32_t rampa_desde_ms;
static uint8_t volumen_final;
static int volumen_puesto;

static void poner_volumen(int v)
{
    if (v != volumen_puesto) {
        volumen_puesto = v;
        audio_volumen((uint8_t)v);
    }
}

static int volumen_inicial(void)
{
    int v = volumen_final - BAJADA_INICIAL;
    return v < VOLUMEN_MINIMO ? (volumen_final < VOLUMEN_MINIMO ? volumen_final : VOLUMEN_MINIMO) : v;
}

static void poner_tono(void)
{
    printf("Alarma: tono de emergencia\n");
    sonido = SUENA_TONO;
    nav.hay_sonando = false;
    poner_volumen(volumen_inicial());
    audio_tono();
    subiendo = true;
    rampa_desde_ms = hal_millis();
}

static void poner_emisora(const EmisoraGuardada *e)
{
    sonido = SUENA_EMISORA;
    subiendo = false;
    poner_volumen(volumen_inicial());
    nav.emisora = e->emisora;
    memset(&nav.pais, 0, sizeof(nav.pais));
    strncpy(nav.pais.codigo, e->pais, sizeof(nav.pais.codigo) - 1);
    strncpy(nav.pais.nombre, e->nombre_pais, sizeof(nav.pais.nombre) - 1);
    nav.aleatoria = false;
    nav_reproducir();
}

static void disparar(int hora, int minuto, const Alarma *a)
{
    printf("Alarma de las %02d:%02d\n", hora, minuto);
    sonando = true;
    desde_ms = hal_millis();
    volumen_final = a->volumen;
    volumen_puesto = -1;
    reposo_despertar();

    EstadoAudio estado = audio_estado();
    const EmisoraGuardada *e = a->con_emisora ? &a->emisora : (ajustes.hay_ultima ? &ajustes.ultima : NULL);
    if (nav.hay_sonando && (estado == AUDIO_SONANDO || estado == AUDIO_CONECTANDO)) {
        sonido = SUENA_MUSICA;
    } else if (e == NULL || hal_red_estado() != RED_CONECTADA) {
        poner_tono();
    } else {
        poner_emisora(e);
    }
    alarma_mostrar(hora, minuto);
}

/* Mientras suena: pasa al tono si falla la emisora y sube el volumen */
static void seguir_sonando(uint32_t ahora)
{
    if (ahora - desde_ms > TOPE_MS) {
        alarmas_apagar();
        alarma_cerrar();
        return;
    }
    if (sonido == SUENA_MUSICA) {
        return;
    }
    if (sonido == SUENA_EMISORA) {
        EstadoAudio estado = audio_estado();
        if (estado == AUDIO_ERROR || estado == AUDIO_PARADO ||
            (!subiendo && ahora - desde_ms > ESPERA_EMISORA_MS && estado != AUDIO_SONANDO)) {
            poner_tono();
        } else if (!subiendo && estado == AUDIO_SONANDO) {
            subiendo = true;
            rampa_desde_ms = ahora;
        }
    }
    if (subiendo) {
        uint32_t rampa = sonido == SUENA_TONO ? RAMPA_TONO_MS : RAMPA_MS;
        uint32_t pasado = ahora - rampa_desde_ms;
        float k = pasado >= rampa ? 1.0f : (float)pasado / rampa;
        int inicial = volumen_inicial();
        poner_volumen(inicial + (int)((volumen_final - inicial) * k));
    }
}

void alarmas_atender(void)
{
    uint32_t ahora = hal_millis();
    if (ahora - ultimo_ms < PERIODO_MS) {
        return;
    }
    ultimo_ms = ahora;
    if (sonando) {
        seguir_sonando(ahora);
    }

    int hora, minuto, anio, mes, dia, semana;
    if (!hal_hora(&hora, &minuto) || !hal_fecha(&anio, &mes, &dia, &semana)) {
        return;  /* aún no se sabe la hora */
    }
    int este_minuto = hora * 60 + minuto;
    if (este_minuto == ultimo_minuto) {
        return;
    }
    ultimo_minuto = este_minuto;

    const Alarma *suena = NULL;
    bool cambio = false;
    for (uint8_t i = 0; i < ajustes.num_alarmas; i++) {
        Alarma *a = &ajustes.alarmas[i];
        if (!a->activa || a->hora != hora || a->minuto != minuto) {
            continue;
        }
        if (a->dias == 0) {
            a->activa = false;  /* «una vez»: se apaga sola */
            cambio = true;
            suena = a;
        } else if (a->dias & (1 << semana)) {
            suena = a;
        }
    }
    if (cambio) {
        ajustes_guardar();
    }
    if (suena != NULL && !sonando) {
        disparar(hora, minuto, suena);
    }
}

void alarmas_apagar(void)
{
    if (sonando && sonido != SUENA_MUSICA) {
        audio_parar();
        nav.hay_sonando = false;
        audio_volumen(ajustes.volumen);
    }
    sonando = false;
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
