/**
 * Idioma de la interfaz: inglés (por defecto) o español.
 *
 * Uso: TR("Tap a region", "Toca una región"). El inglés va siempre primero.
 * El idioma se elige en el primer arranque y se guarda en los ajustes.
 */
#pragma once

/* true: español; false: inglés */
extern bool idioma_es;

#define TR(en, es) (idioma_es ? (es) : (en))
