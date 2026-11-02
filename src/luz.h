/**
 * Luz de la radio con el LED RGB de la placa:
 *   - conectando: ámbar respirando;  portal WiFi: azul respirando;
 *   - emisora caída: tres destellos rojos;
 *   - sonando, modo «estado»: el color de la región de la emisora en el
 *     mapamundi (saturado, que los del mapa son pastel);
 *   - sonando, modo «ritmo»: el arcoíris girando despacio, con el brillo
 *     al ritmo de la música, como el «ojo mágico» de las radios de válvulas.
 */
#pragma once

typedef enum {
    LUZ_APAGADA,
    LUZ_ESTADO,
    LUZ_RITMO,
    NUM_MODOS_LUZ,
} ModoLuz;

/* Busca el color de la región del país que empieza a sonar */
void luz_emisora(const char *codigo_pais);

/* Anima el LED; llamarlo en cada vuelta del bucle principal */
void luz_atender(void);
