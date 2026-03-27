#ifndef LOG_H
#define LOG_H

#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * GLOBEX_OS Logging System
 * =============================================================================
 *
 * Multi-level logging for kernel development and debugging.
 * Supports runtime and compile-time log level filtering.
 *
 * Log Levels (in order of severity):
 *   - LOG_LEVEL_DEBUG (0): Detailed debugging information
 *   - LOG_LEVEL_INFO (1): General informational messages
 *   - LOG_LEVEL_WARN (2): Warning conditions (non-fatal)
 *   - LOG_LEVEL_ERROR (3): Error conditions (may be recoverable)
 *   - LOG_LEVEL_PANIC (4): Fatal errors (system will halt)
 *
 * Usage:
 *   @code
 *   LOG_INFO("System initialized");
 *   LOG_WARN("Low memory: %u bytes", free_mem);
 *   LOG_ERROR("Failed to initialize device: %d", error_code);
 *   LOG_PANIC("Critical failure", error_code);
 *   @endcode
 *
 * Compile-time Configuration:
 *   Define LOG_MAX_LEVEL to filter logs at compile time:
 *   - #define LOG_MAX_LEVEL LOG_LEVEL_INFO  // Only INFO and above
 *   - #define LOG_MAX_LEVEL LOG_LEVEL_ERROR // Only ERROR and above
 *
 * Runtime Configuration:
 *   Use log_set_level() to change minimum log level at runtime.
 *
 * Output Destinations:
 *   - Serial console (always, if initialized)
 *   - VGA console (optional, controlled by LOG_TO_VGA)
 *
 * Thread Safety:
 *   - All log functions are SMP-safe via internal spinlock
 *   - Safe to call from interrupt handlers (with limitations)
 *
 * =============================================================================
 */

/* =============================================================================
 * Log Level Definitions
 * =============================================================================
 */
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_PANIC 4

/* =============================================================================
 * Compile-time Configuration
 * =============================================================================
 */
#ifndef LOG_MAX_LEVEL
#define LOG_MAX_LEVEL LOG_LEVEL_DEBUG /* Default: all logs enabled */
#endif

#ifndef LOG_TO_VGA
#define LOG_TO_VGA 0 /* Disabled by default to avoid screen corruption */
                     /* Set to 1 to enable VGA output for logs */
#endif

/* =============================================================================
 * Log Function API
 * =============================================================================
 */

/**
 * @brief Initialize logging system
 * @return 1 if success, 0 if failure
 *
 * Must be called before any LOG_* macros.
 * Initializes serial port if not already initialized.
 */
int log_init(void);

/**
 * @brief Set minimum log level at runtime
 * @param level Minimum level to output (LOG_LEVEL_*)
 *
 * Messages below this level will be suppressed.
 * Does not affect compile-time filtering (LOG_MAX_LEVEL).
 *
 * @example
 *   log_set_level(LOG_LEVEL_WARN);  // Only WARN, ERROR, PANIC
 */
void log_set_level(int level);

/**
 * @brief Get current log level
 * @return Current minimum log level
 */
int log_get_level(void);

/**
 * @brief Check if logging is initialized
 * @return 1 if initialized, 0 otherwise
 */
int log_is_initialized(void);

/* =============================================================================
 * CRIT-SEC-001 FIX: Format String Security Functions
 * =============================================================================
 */

/**
 * @brief Sanitize a user-controlled string for safe logging
 * @param dst Destination buffer
 * @param dst_size Destination buffer size
 * @param src Source string (potentially untrusted user input)
 * @return Number of characters written (excluding null terminator)
 *
 * This function escapes '%' characters to prevent format string attacks
 * when logging untrusted user input.
 *
 * Usage:
 *   @code
 *   char safe_buf[256];
 *   log_sanitize_string(safe_buf, sizeof(safe_buf), user_input);
 *   LOG_INFO("User provided: %s", safe_buf);  // Safe - % escaped as %%
 *   @endcode
 *
 * @warning Always use this for user-controlled strings before logging
 */
size_t log_sanitize_string(char* dst, size_t dst_size, const char* src);

/**
 * @brief Validate a format string for security
 * @param fmt Format string to validate
 * @return 1 if valid/safe, 0 if suspicious/dangerous
 *
 * Checks for:
 *   - %n specifier (write attack vector)
 *   - Excessive format specifiers (stack read attack)
 *   - Unknown/invalid specifiers
 *
 * Usage:
 *   @code
 *   if (!log_validate_format(fmt)) {
 *       LOG_ERROR("Rejected invalid format string");
 *       return;
 *   }
 *   @endcode
 *
 * @note This is automatically called by log_output() - use for custom validation
 */
int log_validate_format(const char* fmt);

/* =============================================================================
 * Internal Logging Functions (do not call directly)
 * =============================================================================
 * Use LOG_* macros instead.
 * These are exposed for macro expansion only.
 *
 * SECURITY WARNING - FORMAT STRING VULNERABILITY:
 * =============================================================================
 * The `fmt` parameter MUST NEVER come from untrusted user input.
 *
 * Vulnerability: Format String Attack
 * -----------------------------------
 * If an attacker can control the format string, they can:
 *   - Read arbitrary memory (using %x, %s specifiers)
 *   - Write to arbitrary memory (using %n specifier)
 *   - Crash the system (using invalid specifiers)
 *   - Leak sensitive information (pointers, addresses)
 *
 * Example ATTACK VECTOR (DO NOT DO THIS):
 *   @code
 *   // VULNERABLE - attacker controls user_input
 *   char* user_input = get_user_data();  // e.g., "%x %x %x %x"
 *   LOG_INFO(user_input);  // SECURITY VULNERABILITY!
 *   // Result: Attacker can read stack memory
 *   @endcode
 *
 * Example SAFE USAGE:
 *   @code
 *   // SAFE - format string is a literal
 *   LOG_INFO("User provided value: %s", user_input);
 *   // Result: user_input is treated as DATA, not format
 *   @endcode
 *
 * Rules for Safe Usage:
 * ---------------------
 *   1. ALWAYS use string literals for the `fmt` parameter
 *   2. NEVER pass user input, external data, or variables as `fmt`
 *   3. ALWAYS use %s to log untrusted strings (they become data, not format)
 *   4. Consider sanitizing input before logging (remove % characters)
 *
 * Implementation Note:
 *   This is a variadic function (printf-style). The format string is parsed
 *   and used to interpret subsequent arguments. If the format string contains
 *   more specifiers than arguments, undefined behavior occurs (stack read).
 *
 * References:
 *   - OWASP: Format String Attack
 *   - CWE-134: Use of Externally-Controlled Format String
 *   - CERT C: FIO30-C. Exclude user input from format strings
 */

/**
 * @brief Internal log output function
 * @param level Log level of message
 * @param module Module name (e.g., "KERNEL", "DRIVER")
 * @param file Source file name
 * @param line Line number
 * @param fmt Format string (supports %d, %x, %s) - MUST be a string literal
 * @param ... Format arguments
 *
 * @warning SECURITY: `fmt` must NEVER be user-controlled input
 */
void log_output(int level, const char* module, const char* file, int line, const char* fmt, ...);

/**
 * @brief Internal log output without location info
 * @param level Log level of message
 * @param fmt Format string (supports %d, %x, %s) - MUST be a string literal
 * @param ... Format arguments
 *
 * @warning SECURITY: `fmt` must NEVER be user-controlled input
 */
void log_output_simple(int level, const char* fmt, ...);

/* =============================================================================
 * Public Logging Macros
 * =============================================================================
 */

/* Level-specific macros with automatic location info */
#if LOG_LEVEL_DEBUG >= LOG_MAX_LEVEL
#define LOG_DEBUG(fmt, ...)                                                                        \
    log_output(LOG_LEVEL_DEBUG, "DBG", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) /* Compiled out */
#endif

#if LOG_LEVEL_INFO >= LOG_MAX_LEVEL
// clang-format off
#define LOG_INFO(fmt, ...)                                                                           \
    log_output(LOG_LEVEL_INFO, "INF", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
// clang-format on
#else
#define LOG_INFO(fmt, ...) /* Compiled out */
#endif

#if LOG_LEVEL_WARN >= LOG_MAX_LEVEL
// clang-format off
#define LOG_WARN(fmt, ...)                                                                           \
    log_output(LOG_LEVEL_WARN, "WRN", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
// clang-format on
#else
#define LOG_WARN(fmt, ...) /* Compiled out */
#endif

#if LOG_LEVEL_ERROR >= LOG_MAX_LEVEL
#define LOG_ERROR(fmt, ...)                                                                        \
    log_output(LOG_LEVEL_ERROR, "ERR", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...) /* Compiled out */
#endif

#if LOG_LEVEL_PANIC >= LOG_MAX_LEVEL
#define LOG_PANIC(fmt, ...)                                                                        \
    log_output(LOG_LEVEL_PANIC, "PAN", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_PANIC(fmt, ...) /* Compiled out */
#endif

/* Simple macros without location info (for less verbose output) */
#if LOG_LEVEL_DEBUG >= LOG_MAX_LEVEL
// clang-format off
#define LOG_DEBUG_SIMPLE(fmt, ...)                                                                 \
    log_output_simple(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
// clang-format on
#else
#define LOG_DEBUG_SIMPLE(fmt, ...) /* Compiled out */
#endif

#if LOG_LEVEL_INFO >= LOG_MAX_LEVEL
// clang-format off
#define LOG_INFO_SIMPLE(fmt, ...)                                                                  \
    log_output_simple(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
// clang-format on
#else
#define LOG_INFO_SIMPLE(fmt, ...) /* Compiled out */
#endif

#if LOG_LEVEL_WARN >= LOG_MAX_LEVEL
// clang-format off
#define LOG_WARN_SIMPLE(fmt, ...)                                                                  \
    log_output_simple(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
// clang-format on
#else
#define LOG_WARN_SIMPLE(fmt, ...) /* Compiled out */
#endif

#if LOG_LEVEL_ERROR >= LOG_MAX_LEVEL
#define LOG_ERROR_SIMPLE(fmt, ...) log_output_simple(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR_SIMPLE(fmt, ...) /* Compiled out */
#endif

/* =============================================================================
 * Convenience Macros
 * =============================================================================
 */

/**
 * @brief Log function entry (DEBUG level)
 * @param func Function name (use __func__)
 */
#define LOG_ENTER(func) LOG_DEBUG("-> %s()", func)

/**
 * @brief Log function exit (DEBUG level)
 * @param func Function name (use __func__)
 */
#define LOG_EXIT(func) LOG_DEBUG("<- %s()", func)

/**
 * @brief Log function exit with return value (DEBUG level)
 * @param func Function name
 * @param retval Return value to log
 */
#define LOG_EXIT_VALUE(func, retval) LOG_DEBUG("<- %s() = %d", func, retval)

/**
 * @brief Log a hex dump of memory (DEBUG level)
 * @param label Label for the dump
 * @param addr Starting address
 * @param len Number of bytes to dump
 */
#define LOG_HEX_DUMP(label, addr, len) log_hex_dump(label, addr, len)

/* =============================================================================
 * Hex Dump Function
 * =============================================================================
 */

/**
 * @brief Output a hex dump of memory region
 * @param label Label for the dump
 * @param addr Starting address
 * @param len Number of bytes to dump
 */
void log_hex_dump(const char* label, const void* addr, size_t len);

/**
 * @brief Format into caller-supplied buffer using the same formatter as log_output.
 *
 * Truncates output longer than cap-1 characters by replacing the last three
 * characters with "...". Returns the number of characters written (excluding null).
 * Safe to call with cap == 0 (writes nothing, returns 0).
 *
 * For use in tests that verify truncation behavior without intercepting serial output.
 *
 * @param dst  Destination buffer
 * @param cap  Buffer capacity in bytes (including space for null terminator)
 * @param fmt  Format string (same specifiers as log_output)
 * @return     Number of characters written (excluding null terminator)
 */
size_t log_format_to_buf(char* dst, size_t cap, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* LOG_H */
