#ifndef PRINT_H
#define PRINT_H

#include <stdint.h>
#include <stddef.h>

/* ==========================================
 * Guardas para linkage C/C++ mixto
 * ESENCIAL en kernel development
 * ========================================== */
#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================
 * Colores VGA para modo texto
 * ==========================================
 * El color VGA es un byte donde:
 * - Bits 0-3: Color de foreground
 * - Bits 4-7: Color de background
 */
typedef enum PrintColor {
    PRINT_COLOR_BLACK = 0,
    PRINT_COLOR_BLUE = 1,
    PRINT_COLOR_GREEN = 2,
    PRINT_COLOR_CYAN = 3,
    PRINT_COLOR_RED = 4,
    PRINT_COLOR_MAGENTA = 5,
    PRINT_COLOR_BROWN = 6,
    PRINT_COLOR_LIGHT_GRAY = 7,
    PRINT_COLOR_DARK_GRAY = 8,
    PRINT_COLOR_LIGHT_BLUE = 9,
    PRINT_COLOR_LIGHT_GREEN = 10,
    PRINT_COLOR_LIGHT_CYAN = 11,
    PRINT_COLOR_LIGHT_RED = 12,
    PRINT_COLOR_PINK = 13,
    PRINT_COLOR_YELLOW = 14,
    PRINT_COLOR_WHITE = 15,
} PrintColor_t;

/* ==========================================
 * API de funciones de print
 * ========================================== */

/**
 * @brief Detectar hardware VGA antes de usar funciones de print
 * @return true si VGA disponible, false si no
 * @note Debe llamarse antes de print_clear() o cualquier función de print
 */
bool print_detect(void);

/**
 * @brief Limpiar la pantalla completa y resetear cursor
 * @note Requiere que print_detect() haya sido llamado previamente
 */
void print_clear(void);

/**
 * @brief Imprimir un solo caracter
 * @param character Caracter a imprimir (soporta '\n', '\r', '\t')
 */
void print_char(char character);

/**
 * @brief Imprimir string null-terminated
 * @param string Puntero a string (validado internamente para NULL)
 */
void print_str(const char* string);

/**
 * @brief Configurar colores de foreground y background
 * @param foreground Color de foreground (0-15). Valores válidos: PRINT_COLOR_*
 * @param background Color de background (0-15). Valores válidos: PRINT_COLOR_*
 * 
 * @note Si foreground o background están fuera de rango (>15), se usa el color
 *       por defecto (texto blanco sobre fondo negro) de forma segura.
 * @note Esta función es safe para usar con valores no validados - no causa UB.
 * @note Los valores se enmascaran con 0x0F para asegurar compatibilidad.
 * 
 * @example
 *     print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);  // OK
 *     print_set_color(255, 100);  // Invalid - usa default (white on black)
 */
void print_set_color(uint8_t foreground, uint8_t background);

/**
 * @brief Imprimir un entero de 32-bit en hexadecimal
 * @param value Valor a imprimir (sin signo)
 */
void print_hex(uint32_t value);

/**
 * @brief Imprimir un entero de 32-bit en decimal
 * @param value Valor a imprimir (sin signo)
 */
void print_dec(uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* PRINT_H */
