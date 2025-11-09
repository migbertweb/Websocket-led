#ifndef FONTS_H
#define FONTS_H

#include <stdint.h>

/**
 * @file fonts.h
 * @brief Fuente monoespaciada 5x7 y API de acceso.
 *
 * Provee la tabla de glifos `font_5x7` (95 caracteres imprimibles desde
 * ASCII 32 a 126) y funciones de acceso para obtener punteros y dimensiones.
 *
 * Autor: migbertweb
 * Fecha: 2025-11-09
 */

/*
 * Tabla de fuentes (definición en fonts.c). Cada carácter ocupa 5 columnas
 * de 8 bits (solo se usan 7 filas). El índice 0 corresponde a ASCII 32 (espacio).
 */
extern const uint8_t font_5x7[95][5];

/**
 * @brief Devuelve un puntero a la tabla de fuentes 5x7.
 * @return const uint8_t* Apunta al primer elemento de `font_5x7`.
 */
const uint8_t* fonts_get_font_5x7(void);

/**
 * @brief Anchura en columnas de cada carácter (en pixeles)
 * @return int  ancho en columnas (5)
 */
int fonts_get_char_width(void);

/**
 * @brief Altura de cada carácter (en pixeles)
 * @return int altura en filas (7)
 */
int fonts_get_char_height(void);

#endif // FONTS_H