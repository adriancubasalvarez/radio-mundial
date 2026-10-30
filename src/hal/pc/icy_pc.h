/**
 * Lector de metadatos ICY del simulador.
 *
 * ffplay no dice qué canción suena, así que el simulador abre una segunda
 * conexión a la emisora pidiendo los metadatos ICY (cabecera
 * "Icy-MetaData: 1") y lee el campo StreamTitle. Solo funciona con
 * emisoras http:// (las https:// necesitarían TLS); en la placa, la librería
 * de audio ya da el título.
 */
#pragma once

#include <string>

/* Empieza a leer los títulos de una URL (para el lector anterior, si lo hay) */
void icy_empezar(const std::string &url);

/* Deja de leer */
void icy_parar(void);

/* Último título recibido (UTF-8), "" si ninguno */
std::string icy_titulo(void);
