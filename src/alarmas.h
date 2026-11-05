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

/* true mientras suena una alarma */
bool alarmas_sonando(void);

/* Días de una alarma en texto corto: «Una vez», «Lun a vie», «L M X»... */
const char *alarmas_texto_dias(uint8_t dias);
