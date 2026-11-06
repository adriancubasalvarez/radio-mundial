/**
 * Alarmas de la radio: cada una tiene hora, días de la semana y si está
 * activa. Se guardan con los ajustes.
 */
#pragma once

#include <stdint.h>

/* Mira el reloj; llamarlo en cada vuelta del bucle principal */
void alarmas_atender(void);

/* Para la alarma que está sonando */
void alarmas_apagar(void);

/* Para la alarma y la repite dentro de 10 minutos */
void alarmas_posponer(void);

/* Progreso del amanecer con el LED, de 0 a 1; negativo si no hay ninguno */
float alarmas_amanecer(void);

/* Temporizador para dormir: cada llamada pasa a la siguiente duración */
void alarmas_dormir_cambiar(void);

/* Minutos que le quedan al temporizador (0 = desactivado) */
int alarmas_dormir_restante(void);

/* true mientras suena una alarma */
bool alarmas_sonando(void);

/* true si la radio despertó sola y espera a oscuras a que llegue la alarma */
bool alarmas_en_espera(void);

/* Segundos que puede dormir la radio antes de despertar para la próxima
 * alarma (0 = no hay ninguna activa) */
uint32_t alarmas_segundos_para_despertar(void);

/* Días de una alarma en texto corto: «Una vez», «Lun a vie», «L M X»... */
const char *alarmas_texto_dias(uint8_t dias);
