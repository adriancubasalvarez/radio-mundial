#include <Arduino.h>

#include "hal.h"

// GPIO 9 is ADC1_CH8 on the ESP32-S3 (safe to use alongside Wi-Fi)
const int BATTERY_ADC_PIN = 9;

// Divider multiplier (2.0 for two equal resistors, e.g., 100k + 100k)
// Adjust slightly (e.g., 2.02) if you measure a small offset with a multimeter
const float DIVIDER_MULTIPLIER = 2.0f;

// Hysteresis buffer in millivolts to prevent icon flickering
const int HYSTERESIS_MV = 40;

// Battery voltage thresholds (in mV)
const int THRESHOLD_FULL  = 3900;
const int THRESHOLD_3     = 3800;
const int THRESHOLD_2     = 3720;
const int THRESHOLD_1     = 3620;
const int THRESHOLD_EMPTY = 3300;

// Reads averaged battery voltage in millivolts
uint32_t readBatteryVoltageMV() {
    uint32_t totalMilliVolts = 0;
    const int numSamples = 16;

    for (int i = 0; i < numSamples; i++) {
        totalMilliVolts += analogReadMilliVolts(BATTERY_ADC_PIN);
        delay(2); // Short pause between samples
    }

    float avgPinMV = totalMilliVolts / (float)numSamples;
    return (uint32_t)(avgPinMV * DIVIDER_MULTIPLIER);
}

/* ---------- Rizado: ¿hay batería conectada? ----------
 * Con el USB enchufado y SIN batería (interruptor apagado), el cargador deja
 * en BAT+ unos 4,1 V con un rizado lento (~60 ms). Con batería, la batería lo
 * aplana. Medido en la radio de desarrollo (22-nov-2026), con ventanas de
 * 2 ms: desviación ~17 mV sin batería y ~1,9 mV con batería.
 * Cada lectura suelta tiene mucho ruido del ADC (~8 mV): se promedian en
 * ventanas de 2 ms y se mide cuánto varían esas medias entre sí. El rizado
 * sobrevive al promedio; el ruido, no. */

#define VENTANA_US       2000
#define RIZADO_SIN_MV    8.0f   /* por encima: sin batería */
#define RIZADO_CON_MV    5.0f   /* por debajo: con batería (entre los dos, como estaba) */

typedef struct {
    float media, desviacion, pico_a_pico;  /* mV de la batería */
    uint32_t lecturas, ventanas;
} Rizado;

static Rizado medir_rizado(uint32_t duracion_us)
{
    double suma = 0, suma2 = 0;
    double vmin = 1e9, vmax = 0;
    Rizado r = {0, 0, 0, 0, 0};
    uint32_t inicio = micros();
    while (micros() - inicio < duracion_us) {
        double acumulado = 0;
        uint32_t lecturas = 0;
        uint32_t inicio_ventana = micros();
        while (micros() - inicio_ventana < VENTANA_US) {
            acumulado += analogReadMilliVolts(BATTERY_ADC_PIN) * DIVIDER_MULTIPLIER;
            lecturas++;
        }
        double v = acumulado / lecturas;
        suma += v;
        suma2 += v * v;
        if (v < vmin) vmin = v;
        if (v > vmax) vmax = v;
        r.lecturas += lecturas;
        r.ventanas++;
    }
    double media = suma / r.ventanas;
    double varianza = suma2 / r.ventanas - media * media;
    r.media = (float)media;
    r.desviacion = varianza > 0 ? (float)sqrt(varianza) : 0;
    r.pico_a_pico = (float)(vmax - vmin);
    return r;
}

/* Orden «RMV» por el USB (ver mis_emisoras_usb.cpp): 5 tandas de 200 ms con
 * la media, la desviación y el pico a pico. Bloquea ~1 s: solo para
 * diagnóstico. */
void bateria_medir_rizado(void)
{
    Serial.println("[rizado] midiendo...");
    for (int t = 0; t < 5; t++) {
        Rizado r = medir_rizado(200000);
        Serial.printf("[rizado] tanda %d: %u lecturas en %u ventanas, media %.0f mV, desviación %.2f mV, "
                      "pico a pico %.1f mV\n",
                      t + 1, (unsigned)r.lecturas, (unsigned)r.ventanas, r.media, r.desviacion, r.pico_a_pico);
    }
}

/* Una medida corta (40 ms) cada vez; se decide con la media de las tres
 * últimas y con histéresis, para que un pico suelto no cambie nada */
bool hal_bateria_conectada(void)
{
    static float ultimas[3];
    static int cuantas = 0, pos = 0;
    static bool conectada = false;
    static bool primera = true;

    float d = medir_rizado(40000).desviacion;
    if (cuantas == 0) {
        ultimas[0] = ultimas[1] = ultimas[2] = d;  /* al arrancar, decidir ya */
        cuantas = 3;
    }
    ultimas[pos] = d;
    pos = (pos + 1) % 3;
    float media = (ultimas[0] + ultimas[1] + ultimas[2]) / 3;

    bool antes = conectada;
    if (media > RIZADO_SIN_MV) {
        conectada = false;
    } else if (media < RIZADO_CON_MV) {
        conectada = true;
    }
    if (primera || conectada != antes) {
        Serial.printf("[bateria] rizado %.1f mV: %s\n", media, conectada ? "batería conectada" : "sin batería");
        primera = false;
    }
    return conectada;
}

// Returns the appropriate battery level with hysteresis
BatteryLevel getBatteryLevel() {
    static BatteryLevel currentLevel = BAT_LEVEL_FULL;
    static bool initialized = false;

    int vBat = readBatteryVoltageMV();
    BatteryLevel previousLevel = currentLevel;

    // On first boot, snap directly to the closest level without hysteresis
    if (!initialized) {
        if (vBat >= THRESHOLD_FULL)        currentLevel = BAT_LEVEL_FULL;
        else if (vBat >= THRESHOLD_3)      currentLevel = BAT_LEVEL_3;
        else if (vBat >= THRESHOLD_2)      currentLevel = BAT_LEVEL_2;
        else if (vBat >= THRESHOLD_1)      currentLevel = BAT_LEVEL_1;
        else                               currentLevel = BAT_LEVEL_EMPTY;
        initialized = true;
    } else {
        // Apply hysteresis buffer when stepping up or down
        switch (currentLevel) {
            case BAT_LEVEL_FULL:
                if (vBat < (THRESHOLD_FULL - HYSTERESIS_MV)) currentLevel = BAT_LEVEL_3;
                break;

            case BAT_LEVEL_3:
                if (vBat >= (THRESHOLD_FULL + HYSTERESIS_MV))     currentLevel = BAT_LEVEL_FULL;
                else if (vBat < (THRESHOLD_3 - HYSTERESIS_MV))    currentLevel = BAT_LEVEL_2;
                break;

            case BAT_LEVEL_2:
                if (vBat >= (THRESHOLD_3 + HYSTERESIS_MV))        currentLevel = BAT_LEVEL_3;
                else if (vBat < (THRESHOLD_2 - HYSTERESIS_MV))    currentLevel = BAT_LEVEL_1;
                break;

            case BAT_LEVEL_1:
                if (vBat >= (THRESHOLD_2 + HYSTERESIS_MV))        currentLevel = BAT_LEVEL_2;
                else if (vBat < (THRESHOLD_1 - HYSTERESIS_MV))    currentLevel = BAT_LEVEL_EMPTY;
                break;

            case BAT_LEVEL_EMPTY:
                if (vBat >= (THRESHOLD_1 + HYSTERESIS_MV))        currentLevel = BAT_LEVEL_1;
                break;
        }
    }

    // Log only on the first reading and when the level changes, so the
    // serial log isn't flooded every 5 seconds
    static bool logged = false;
    if (!logged || currentLevel != previousLevel) {
        Serial.printf("[bateria] %d mV, nivel %d\n", vBat, (int)currentLevel);
        logged = true;
    }

    return currentLevel;
}
