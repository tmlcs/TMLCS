#ifndef DEBUG_H
#define DEBUG_H

#include "serial.h"
#include "print.h"

#ifdef __cplusplus
extern "C" {
#endif

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
#define DEBUG_LOG(msg) do { \
    serial_write_str("[DEBUG] "); \
    serial_write_str(msg); \
    serial_write_str("\r\n"); \
} while(0)

/**
 * @brief Print variable with name and hexadecimal value
 * Usage: DEBUG_VAR(myVar, value); -> "[DEBUG] myVar = 0xCAFEBABE"
 */
#define DEBUG_VAR(name, val) do { \
    serial_write_str("[DEBUG] " #name " = "); \
    serial_write_hex(val); \
    serial_write_str("\r\n"); \
} while(0)

/**
 * @brief Breakpoint (prints location)
 */
#define DEBUG_BREAK() do { \
    serial_write_str("\r\n[BREAK] "); \
    serial_write_str(__FILE__); \
    serial_write_str(":"); \
    serial_write_dec(__LINE__); \
    serial_write_str("\r\n"); \
} while(0)

/* ==========================================
 * Debugging Macros - Advanced Level [D001, D002, D003]
 * ========================================== */

/**
 * @brief [D001] DEBUG_PRINTF - Print with limited format support
 * Supports: %s (string), %x (hex), %d (decimal), %c (char)
 * Usage: DEBUG_PRINTF("Value: %x, Name: %s\r\n", hexVal, str);
 *
 * @note Simple implementation without varargs for freestanding kernel
 *       Use multiple calls for complex formats
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
 * @brief [D002] DEBUG_PRINTLN - Print with automatic newline
 * Similar to DEBUG_LOG but without [DEBUG] prefix
 * Usage: DEBUG_PRINTLN("Simple message");
 */
#define DEBUG_PRINTLN(msg) do { \
    serial_write_str(msg); \
    serial_write_str("\r\n"); \
} while(0)

/**
 * @brief [D003] DEBUG_ASSERT - Assert for kernel
 * Verifies condition and breaks if false
 * Usage: DEBUG_ASSERT(myPtr != NULL);
 *
 * @note In release mode (DEBUG_ENABLE=0), generates no code
 * @note Uses DEBUG_BREAK() to report failure location
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

/* Debugging disabled - macros generate no code */
#define DEBUG_PRINT(str) ((void)0)
#define DEBUG_PRINT_CHAR(c) ((void)0)
#define DEBUG_PRINT_HEX(val) ((void)0)
#define DEBUG_PRINT_HEX64(val) ((void)0)
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
