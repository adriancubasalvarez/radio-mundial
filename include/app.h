/**
 * Punto de entrada común de la aplicación.
 * En el PC lo llama main(); en la placa, setup() y loop() de Arduino.
 */
#pragma once

/* Inicializa LVGL, la pantalla y la interfaz */
void app_inicio(void);

/* Una vuelta del bucle principal. Devuelve false si hay que salir. */
bool app_bucle(void);
