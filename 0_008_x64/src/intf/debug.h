#ifndef DEBUG_H
#define DEBUG_H

#include "serial.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================
 * Control de debugging
 * ==========================================
 * Definir DEBUG_ENABLE para activar output serial
 */
#ifndef DEBUG_ENABLE
/* #define DEBUG_ENABLE 1 */
#endif

/* ==========================================
 * Macros de debugging
 * ========================================== */

#if DEBUG_ENABLE

/**
 * @brief Imprimir mensaje de debug con formato simple
 * Uso: DEBUG_PRINT("Mensaje: "); DEBUG_PRINT_HEX(value); DEBUG_PRINT("\r\n");
 */
#define DEBUG_PRINT(str) serial_write_str(str)
#define DEBUG_PRINT_CHAR(c) serial_write_char(c)
#define DEBUG_PRINT_HEX(val) serial_write_hex(val)
#define DEBUG_PRINT_DEC(val) serial_write_dec(val)

/**
 * @brief Imprimir mensaje con prefijo de archivo/línea
 */
#define DEBUG_LOG(msg) do { \
    serial_write_str("[DEBUG] "); \
    serial_write_str(msg); \
    serial_write_str("\r\n"); \
} while(0)

/**
 * @brief Imprimir variable con nombre
 */
#define DEBUG_VAR(name, val) do { \
    serial_write_str("[DEBUG] " #name " = 0x"); \
    serial_write_hex(val); \
    serial_write_str("\r\n"); \
} while(0)

/**
 * @brief Punto de breakpoint (imprime ubicación)
 */
#define DEBUG_BREAK() do { \
    serial_write_str("\r\n[BREAK] "); \
    serial_write_str(__FILE__); \
    serial_write_str(":"); \
    serial_write_dec(__LINE__); \
    serial_write_str("\r\n"); \
} while(0)

#else

/* Debugging deshabilitado - macros no generan código */
#define DEBUG_PRINT(str) ((void)0)
#define DEBUG_PRINT_CHAR(c) ((void)0)
#define DEBUG_PRINT_HEX(val) ((void)0)
#define DEBUG_PRINT_DEC(val) ((void)0)
#define DEBUG_LOG(msg) ((void)0)
#define DEBUG_VAR(name, val) ((void)0)
#define DEBUG_BREAK() ((void)0)

#endif

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_H */
