#include "alarmas.h"
#include "ajustes.h"
#include "hal.h"
#include "idioma.h"
#include "ui/nav.h"
#include "ui/ui.h"

#include <lvgl.h>

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

#define MARGEN_MINIMO_S 120    /* despierta al menos 2 minutos antes: WiFi y hora */
#define MARGEN_DIVISOR  50     /* y un 2 % de lo dormido: el reloj de la placa
                                  dormida se desvía; así no llega tarde */
#define ESPERA_RED_MS   45000UL  /* lo que se da a la WiFi y a la hora */
#define DORMIR_OTRA_VEZ_S 60   /* si aún falta para despertar, vuelve a dormir */

#define POSPONER_MS (10UL * 60 * 1000)
#define AMANECER_MIN 10        /* el LED empieza a subir 10 minutos antes */
#define AMANECER_MS (AMANECER_MIN * 60UL * 1000)
#define BAJADA_DORMIR_MS 60000UL  /* el temporizador baja el volumen en el último minuto */

/* Duraciones del temporizador para dormir, en minutos */
static const uint8_t OPCIONES_DORMIR[] = {0, 15, 30, 60, 90};
#define NUM_OPCIONES_DORMIR (sizeof(OPCIONES_DORMIR) / sizeof(OPCIONES_DORMIR[0]))

static void alarmas_dormir_cambiar_a(uint8_t opcion);

static uint32_t ultimo_ms;
static bool en_espera;
static bool mirar_arranque = true;
static uint32_t espera_desde_ms;
static uint32_t inactivo_antes;
static int ultimo_minuto = -1;  /* para no disparar dos veces en el mismo minuto */
static bool sonando;
static Sonido sonido;
static uint32_t desde_ms;       /* cuándo saltó la alarma */
static bool subiendo;           /* ya hay sonido y el volumen va subiendo */
static uint32_t rampa_desde_ms;
static uint8_t volumen_final;
static int volumen_puesto;
static Alarma alarma_actual;    /* la que sonó: posponer la repite tal cual */
static bool pospuesta;
static uint32_t pospuesta_desde_ms;
static bool en_amanecer;
static uint32_t amanecer_desde_ms;
static uint8_t opcion_dormir;   /* posición en OPCIONES_DORMIR */
static uint32_t dormir_fin_ms;
static int volumen_dormir = -1;

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
    if (a != &alarma_actual) {
        alarma_actual = *a;
    }
    a = &alarma_actual;
    pospuesta = false;
    sonando = true;
    desde_ms = hal_millis();
    volumen_final = a->volumen;
    volumen_puesto = -1;
    reposo_despertar();
    if (en_espera) {
        en_espera = false;
        hal_brillo(100);
    }

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

/* Minutos hasta la próxima vez que suene esta alarma (-1 = nunca) */
static int minutos_hasta(const Alarma *a, int ahora, int semana)
{
    if (!a->activa) {
        return -1;
    }
    for (int d = 0; d <= 7; d++) {
        int m = a->hora * 60 + a->minuto + d * 1440 - ahora;
        if (m > 0 && (a->dias == 0 || (a->dias & (1 << ((semana + d) % 7))))) {
            return m;
        }
    }
    return -1;
}

/* Enciende o apaga el amanecer; se mira una vez por minuto. Si ya hay música
 * no empieza: esa persona está despierta. */
static void mirar_amanecer(int ahora, int semana)
{
    int faltan = -1;
    for (uint8_t i = 0; i < ajustes.num_alarmas; i++) {
        const Alarma *a = &ajustes.alarmas[i];
        int m = a->amanecer ? minutos_hasta(a, ahora, semana) : -1;
        if (m > 0 && m <= AMANECER_MIN && (faltan < 0 || m < faltan)) {
            faltan = m;
        }
    }
    if (faltan > 0 && !en_amanecer && !nav.hay_sonando) {
        en_amanecer = true;
        amanecer_desde_ms = hal_millis() - (AMANECER_MIN - faltan) * 60UL * 1000;
    } else if (faltan < 0 && en_amanecer && !sonando) {
        en_amanecer = false;
    }
}

/* Dispara las alarmas de este minuto */
static void comprobar_reloj(void)
{
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
    mirar_amanecer(este_minuto, semana);
}

/* Si la alarma salta antes, el temporizador se cancela; si no, baja el volumen
 * en el último minuto y apaga la radio (despertará para la próxima alarma) */
static void seguir_temporizador(uint32_t ahora)
{
    int32_t resto = (int32_t)(dormir_fin_ms - ahora);
    if (sonando) {
        alarmas_dormir_cambiar_a(0);
    } else if (resto <= 0) {
        printf("Temporizador: a dormir\n");
        audio_parar();
        ajustes_guardar();
        hal_apagar(alarmas_segundos_para_despertar());
    } else if ((uint32_t)resto < BAJADA_DORMIR_MS) {
        int v = (int)(ajustes.volumen * (uint32_t)resto / BAJADA_DORMIR_MS);
        if (v != volumen_dormir) {
            volumen_dormir = v;
            audio_volumen((uint8_t)v);
        }
    }
}

/* Despertó sola y a oscuras: un toque enciende la pantalla; si la alarma aún
 * queda lejos (ya con la hora corregida por Internet), a dormir otra vez */
static void seguir_esperando(uint32_t ahora)
{
    uint32_t inactivo = lv_disp_get_inactive_time(NULL);
    bool tocada = inactivo < inactivo_antes;
    inactivo_antes = inactivo;
    if (tocada) {
        en_espera = false;
        hal_brillo(100);
        return;
    }
    if (ahora - espera_desde_ms > ESPERA_RED_MS) {
        uint32_t segundos = alarmas_segundos_para_despertar();
        if (segundos == 0 || segundos > DORMIR_OTRA_VEZ_S) {
            hal_apagar(segundos);
        }
    }
}

void alarmas_atender(void)
{
    uint32_t ahora = hal_millis();
    if (ahora - ultimo_ms < PERIODO_MS) {
        return;
    }
    ultimo_ms = ahora;
    if (mirar_arranque) {
        mirar_arranque = false;
        en_espera = hal_despertado_por_alarma();
        espera_desde_ms = ahora;
        inactivo_antes = lv_disp_get_inactive_time(NULL);
    }
    if (sonando) {
        seguir_sonando(ahora);
    }
    if (pospuesta && !sonando && ahora - pospuesta_desde_ms >= POSPONER_MS) {
        int hora = 0, minuto = 0;
        hal_hora(&hora, &minuto);
        disparar(hora, minuto, &alarma_actual);
    }
    comprobar_reloj();
    if (opcion_dormir != 0) {
        seguir_temporizador(ahora);
    }
    /* Después de comprobar el reloj: en el minuto de la alarma, esta ya no
     * cuenta como «próxima» y la radio se volvería a dormir sin sonar */
    if (en_espera) {
        seguir_esperando(ahora);
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
    pospuesta = false;
    en_amanecer = false;
}

void alarmas_posponer(void)
{
    alarmas_apagar();
    pospuesta = true;
    pospuesta_desde_ms = hal_millis();
    printf("Alarma pospuesta 10 minutos\n");
}

float alarmas_amanecer(void)
{
    if (!en_amanecer) {
        return -1.0f;
    }
    uint32_t pasado = hal_millis() - amanecer_desde_ms;
    return pasado >= AMANECER_MS ? 1.0f : (float)pasado / AMANECER_MS;
}

static void alarmas_dormir_cambiar_a(uint8_t opcion)
{
    if (volumen_dormir >= 0) {  /* estaba bajando: vuelve el volumen de siempre */
        volumen_dormir = -1;
        audio_volumen(ajustes.volumen);
    }
    opcion_dormir = opcion;
    dormir_fin_ms = hal_millis() + OPCIONES_DORMIR[opcion] * 60UL * 1000;
}

void alarmas_dormir_cambiar(void)
{
    alarmas_dormir_cambiar_a((uint8_t)((opcion_dormir + 1) % NUM_OPCIONES_DORMIR));
}

int alarmas_dormir_restante(void)
{
    if (opcion_dormir == 0) {
        return 0;
    }
    int32_t resto = (int32_t)(dormir_fin_ms - hal_millis());
    return resto <= 0 ? 1 : (int)(resto / 60000) + 1;  /* redondeado hacia arriba */
}

bool alarmas_sonando(void)
{
    return sonando;
}

bool alarmas_en_espera(void)
{
    return en_espera;
}

uint32_t alarmas_segundos_para_despertar(void)
{
    int hora, minuto, anio, mes, dia, semana;
    if (!hal_hora(&hora, &minuto) || !hal_fecha(&anio, &mes, &dia, &semana)) {
        return 0;
    }
    int ahora = hora * 60 + minuto;
    bool hay = false;
    int faltan = 0;  /* minutos hasta que haya que estar despierta */
    for (uint8_t i = 0; i < ajustes.num_alarmas; i++) {
        const Alarma *a = &ajustes.alarmas[i];
        int m = minutos_hasta(a, ahora, semana);
        if (m < 0) {
            continue;
        }
        if (a->amanecer) {
            m -= AMANECER_MIN;
        }
        if (!hay || m < faltan) {
            hay = true;
            faltan = m;
        }
    }
    if (!hay) {
        return 0;
    }
    if (faltan <= 0) {
        return 1;  /* ya está dentro del amanecer */
    }
    uint32_t segundos = (uint32_t)faltan * 60;
    uint32_t margen = segundos / MARGEN_DIVISOR;
    if (margen < MARGEN_MINIMO_S) {
        margen = MARGEN_MINIMO_S;
    }
    return segundos > margen ? segundos - margen : 1;
}

const char *alarmas_texto_dias(uint8_t dias)
{
    static char texto[24];
    if (dias == 0) return TR("Once", "Una vez");
    if (dias == TODOS) return TR("Every day", "Todos los días");
    if (dias == LUN_A_VIE) return TR("Mon to Fri", "Lun a vie");
    if (dias == FIN_DE_SEMANA) return TR("Weekend", "Sáb y dom");

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
