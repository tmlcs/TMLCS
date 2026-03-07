#ifndef DEBUG_H
#define DEBUG_H

#include "serial.h"
#include "print.h"

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
 * Macros de debugging - Nivel Básico
 * ========================================== */

#if DEBUG_ENABLE

/**
 * @brief Imprimir mensaje de debug simple
 * Uso: DEBUG_PRINT("Mensaje: "); DEBUG_PRINT_HEX(value); DEBUG_PRINT("\r\n");
 */
#define DEBUG_PRINT(str) serial_write_str(str)
#define DEBUG_PRINT_CHAR(c) serial_write_char(c)
#define DEBUG_PRINT_HEX(val) serial_write_hex(val)
#define DEBUG_PRINT_DEC(val) serial_write_dec(val)

/**
 * @brief Imprimir mensaje con prefijo [DEBUG]
 */
#define DEBUG_LOG(msg) do { \
    serial_write_str("[DEBUG] "); \
    serial_write_str(msg); \
    serial_write_str("\r\n"); \
} while(0)

/**
 * @brief Imprimir variable con nombre y valor hexadecimal
 * Uso: DEBUG_VAR(myVar, value); -> "[DEBUG] myVar = 0xCAFEBABE"
 */
#define DEBUG_VAR(name, val) do { \
    serial_write_str("[DEBUG] " #name " = "); \
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

/* ==========================================
 * Macros de debugging - Nivel Avanzado [D001, D002, D003]
 * ========================================== */

/**
 * @brief [D001] DEBUG_PRINTF - Imprimir con formato limitado
 * Soporta: %s (string), %x (hex), %d (decimal), %c (char)
 * Uso: DEBUG_PRINTF("Value: %x, Name: %s\r\n", hexVal, str);
 * 
 * @note Implementación simple sin varargs para kernel freestanding
 *       Usar múltiples llamadas para formatos complejos
 */
#define DEBUG_PRINTF(fmt, arg) do { \
    const char* _fmt = fmt; \
    size_t _i = 0; \
    while (_fmt[_i] != '\0') { \
        if (_fmt[_i] == '%' && _fmt[_i + 1] != '\0') { \
            _i++; \
            switch (_fmt[_i]) { \
                case 's': serial_write_str((const char*)(arg)); break; \
                case 'x': serial_write_hex((uint32_t)(arg)); break; \
                case 'd': serial_write_dec((uint32_t)(arg)); break; \
                case 'c': serial_write_char((char)(arg)); break; \
                case '%': serial_write_char('%'); break; \
                default: serial_write_char('%'); serial_write_char(_fmt[_i]); break; \
            } \
        } else { \
            serial_write_char(_fmt[_i]); \
        } \
        _i++; \
    } \
} while(0)

/**
 * @brief [D002] DEBUG_PRINTLN - Imprimir con newline automático
 * Similar a DEBUG_LOG pero sin prefijo [DEBUG]
 * Uso: DEBUG_PRINTLN("Simple message");
 */
#define DEBUG_PRINTLN(msg) do { \
    serial_write_str(msg); \
    serial_write_str("\r\n"); \
} while(0)

/**
 * @brief [D003] DEBUG_ASSERT - Assert para kernel
 * Verifica condición y hace breakpoint si es falsa
 * Uso: DEBUG_ASSERT(myPtr != NULL);
 * 
 * @note En modo release (DEBUG_ENABLE=0), no genera código
 * @note Usa DEBUG_BREAK() para reportar ubicación del fallo
 */
#define DEBUG_ASSERT(cond) do { \
    if (!(cond)) { \
        serial_write_str("\r\n[ASSERT FAILED] "); \
        serial_write_str(__FILE__); \
        serial_write_str(":"); \
        serial_write_dec(__LINE__); \
        serial_write_str(" - Condition: " #cond "\r\n"); \
        DEBUG_BREAK(); \
    } \
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
#define DEBUG_PRINTF(fmt, arg) ((void)0)
#define DEBUG_PRINTLN(msg) ((void)0)
#define DEBUG_ASSERT(cond) ((void)0)

#endif

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_H */
