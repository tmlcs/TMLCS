#ifndef DEBUG_H
#define DEBUG_H

#include "print.h"
#include "serial.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================
 * SECURITY NOTE
 * ==========================================
 * The original variadic DEBUG_PRINTF(fmt, ...) macro was removed due to
 * critical buffer overflow vulnerability. It accepted a single argument
 * but could process multiple format specifiers, causing:
 *   - Use of uninitialized memory
 *   - Type confusion (same value interpreted as different types)
 *   - Potential stack corruption
 *
 * REPLACEMENT: Use type-safe single-argument macros:
 *   - DEBUG_PRINTF_HEX(label, val)   - Hexadecimal output
 *   - DEBUG_PRINTF_DEC(label, val)   - Decimal output
 *   - DEBUG_PRINTF_STR(label, val)   - String output
 *   - DEBUG_PRINTF_CHAR(label, val)  - Character output
 *   - DEBUG_PRINTF_PTR(label, val)   - Pointer output
 *
 * For multi-value output, use multiple macro calls:
 *   DEBUG_PRINTF_HEX("addr", addr);
 *   DEBUG_PRINTF_DEC("size", size);
 * ==========================================
 */

/* ==========================================
 * Debugging Control
 * ==========================================
 * Define DEBUG_ENABLE to activate serial output
 */
#ifndef DEBUG_ENABLE
/* #define DEBUG_ENABLE 1 */
#endif

/* ==========================================
 * Debugging Macros - Basic Level
 * ========================================== */

#if DEBUG_ENABLE

/**
 * @brief Print simple debug message
 * Usage: DEBUG_PRINT("Message: "); DEBUG_PRINT_HEX(value); DEBUG_PRINT("\r\n");
 */
#define DEBUG_PRINT(str) serial_write_str(str)
#define DEBUG_PRINT_CHAR(c) serial_write_char(c)
#define DEBUG_PRINT_HEX(val) serial_write_hex(val)
#define DEBUG_PRINT_HEX64(val) serial_write_hex64(val)
#define DEBUG_PRINT_DEC(val) serial_write_dec(val)

/**
 * @brief Print message with [DEBUG] prefix
 */
#define DEBUG_LOG(msg)                                                                             \
    do {                                                                                           \
        serial_write_str("[DEBUG] ");                                                              \
        serial_write_str(msg);                                                                     \
        serial_write_str("\r\n");                                                                  \
    } while (0)

/**
 * @brief Print variable with name and hexadecimal value
 * Usage: DEBUG_VAR(myVar, value); -> "[DEBUG] myVar = 0xCAFEBABE"
 */
#define DEBUG_VAR(name, val)                                                                       \
    do {                                                                                           \
        serial_write_str("[DEBUG] " #name " = ");                                                  \
        serial_write_hex(val);                                                                     \
        serial_write_str("\r\n");                                                                  \
    } while (0)

/**
 * @brief Breakpoint (prints location)
 */
#define DEBUG_BREAK()                                                                              \
    do {                                                                                           \
        serial_write_str("\r\n[BREAK] ");                                                          \
        serial_write_str(__FILE__);                                                                \
        serial_write_str(":");                                                                     \
        serial_write_dec(__LINE__);                                                                \
        serial_write_str("\r\n");                                                                  \
    } while (0)

/* ==========================================
 * Debugging Macros - Advanced Level 
 * ========================================== */

/**
 * @brief DEBUG_PRINTF_HEX - Print single hexadecimal value with label
 * Usage: DEBUG_PRINTF_HEX("value", myVar); -> "[DEBUG] value = 0xCAFEBABE"
 *
 * @note Safe single-argument alternative to removed variadic DEBUG_PRINTF
 * @see DEBUG_PRINTF_DEC, DEBUG_PRINTF_STR, DEBUG_PRINTF_CHAR for other types
 */
#define DEBUG_PRINTF_HEX(label, val)                                                               \
    do {                                                                                           \
        serial_write_str("[DEBUG] " label " = 0x");                                                \
        serial_write_hex(val);                                                                     \
        serial_write_str("\r\n");                                                                  \
    } while (0)

/**
 * @brief DEBUG_PRINTF_DEC - Print single decimal value with label
 * Usage: DEBUG_PRINTF_DEC("count", myVar); -> "[DEBUG] count = 1234"
 *
 * @note Safe single-argument alternative to removed variadic DEBUG_PRINTF
 */
#define DEBUG_PRINTF_DEC(label, val)                                                               \
    do {                                                                                           \
        serial_write_str("[DEBUG] " label " = ");                                                  \
        serial_write_dec(val);                                                                     \
        serial_write_str("\r\n");                                                                  \
    } while (0)

/**
 * @brief DEBUG_PRINTF_STR - Print single string value with label
 * Usage: DEBUG_PRINTF_STR("name", myStr); -> "[DEBUG] name = hello"
 *
 * @note Safe single-argument alternative to removed variadic DEBUG_PRINTF
 * @note Does not validate pointer - ensure string is valid
 */
#define DEBUG_PRINTF_STR(label, val)                                                               \
    do {                                                                                           \
        serial_write_str("[DEBUG] " label " = ");                                                  \
        serial_write_str(val);                                                                     \
        serial_write_str("\r\n");                                                                  \
    } while (0)

/**
 * @brief DEBUG_PRINTF_CHAR - Print single char value with label
 * Usage: DEBUG_PRINTF_CHAR("char", myChar); -> "[DEBUG] char = 'A'"
 *
 * @note Safe single-argument alternative to removed variadic DEBUG_PRINTF
 */
#define DEBUG_PRINTF_CHAR(label, val)                                                              \
    do {                                                                                           \
        serial_write_str("[DEBUG] " label " = '");                                                 \
        serial_write_char(val);                                                                    \
        serial_write_str("'\r\n");                                                                 \
    } while (0)

/**
 * @brief DEBUG_PRINTF_PTR - Print pointer address with label
 * Usage: DEBUG_PRINTF_PTR("ptr", myPtr); -> "[DEBUG] ptr = 0x00007FFF"
 *
 * @note Safe single-argument alternative for pointer debugging
 */
#define DEBUG_PRINTF_PTR(label, val)                                                               \
    do {                                                                                           \
        serial_write_str("[DEBUG] " label " = 0x");                                                \
        serial_write_hex64((uint64_t) (val));                                                      \
        serial_write_str("\r\n");                                                                  \
    } while (0)

/**
 * @brief DEBUG_PRINTLN - Print with automatic newline
 * Similar to DEBUG_LOG but without [DEBUG] prefix
 * Usage: DEBUG_PRINTLN("Simple message");
 */
#define DEBUG_PRINTLN(msg)                                                                         \
    do {                                                                                           \
        serial_write_str(msg);                                                                     \
        serial_write_str("\r\n");                                                                  \
    } while (0)

/**
 * @brief DEBUG_ASSERT - Assert for kernel
 * Verifies condition and breaks if false
 * Usage: DEBUG_ASSERT(myPtr != NULL);
 *
 * @note In release mode (DEBUG_ENABLE=0), generates no code
 * @note Uses DEBUG_BREAK() to report failure location
 */
#define DEBUG_ASSERT(cond)                                                                         \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            serial_write_str("\r\n[ASSERT FAILED] ");                                              \
            serial_write_str(__FILE__);                                                            \
            serial_write_str(":");                                                                 \
            serial_write_dec(__LINE__);                                                            \
            serial_write_str(" - Condition: " #cond "\r\n");                                       \
            DEBUG_BREAK();                                                                         \
        }                                                                                          \
    } while (0)

#else

/* Debugging disabled - macros generate no code */
#define DEBUG_PRINT(str) ((void) 0)
#define DEBUG_PRINT_CHAR(c) ((void) 0)
#define DEBUG_PRINT_HEX(val) ((void) 0)
#define DEBUG_PRINT_HEX64(val) ((void) 0)
#define DEBUG_PRINT_DEC(val) ((void) 0)
#define DEBUG_LOG(msg) ((void) 0)
#define DEBUG_VAR(name, val) ((void) 0)
#define DEBUG_BREAK() ((void) 0)
#define DEBUG_PRINTF_HEX(label, val) ((void) 0)
#define DEBUG_PRINTF_DEC(label, val) ((void) 0)
#define DEBUG_PRINTF_STR(label, val) ((void) 0)
#define DEBUG_PRINTF_CHAR(label, val) ((void) 0)
#define DEBUG_PRINTF_PTR(label, val) ((void) 0)
#define DEBUG_PRINTLN(msg) ((void) 0)
#define DEBUG_ASSERT(cond) ((void) 0)

#endif

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_H */
