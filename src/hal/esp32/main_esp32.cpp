/**
 * Punto de entrada en la placa (Arduino): setup() y loop().
 */
#include "app.h"
#include "hal.h"

#include <Arduino.h>

bool archivos_iniciar(void);
void red_iniciar(void);
void red_atender(void);
void audio_iniciar(void);

/* Informe de memoria por el puerto serie cada cierto tiempo */
#define PERIODO_INFORME 10000

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("=== Radio Mundial ===");
    Serial.printf("Memoria libre al arrancar: %u bytes (bloque mayor %u)\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

    archivos_iniciar();
    red_iniciar();
    audio_iniciar();
    app_inicio();
    Serial.printf("Memoria libre tras iniciar la interfaz: %u bytes (bloque mayor %u)\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

void loop()
{
    app_bucle();
    red_atender();

    static uint32_t ultimo = 0;
    if (millis() - ultimo > PERIODO_INFORME) {
        ultimo = millis();
        Serial.printf("[sistema] libre %u, mínimo %u, bloque mayor %u\n",
                      (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
                      (unsigned)ESP.getMaxAllocHeap());
    }
}
