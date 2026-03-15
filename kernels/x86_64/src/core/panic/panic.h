#ifndef PANIC_H
#define PANIC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================
 * Kernel Panic - Fatal system error
 * ==========================================
 * When an unrecoverable error occurs,
 * the kernel must:
 * 1. Print error message (VGA and serial)
 * 2. Stop execution safely
 * 3. Never return
 */

/**
 * @brief Kernel panic - unrecoverable fatal error
 * @param message Descriptive error message
 * @param error_code Optional error code (0 if not applicable)
 *
 * @note This function NEVER returns. Halts the kernel indefinitely.
 * @note Prints to VGA (if available) and serial (if initialized)
 */
void panic(const char* message, uint32_t error_code);

/**
 * @brief Kernel panic with simple message (no error code)
 * @param message Descriptive error message
 */
void panic_simple(const char* message);

/**
 * @brief Check condition and panic if false
 * @param condition Condition that must be true
 * @param message Error message if condition is false
 *
 * @note Convenience macro - expands to panic_simple() if fails
 */
#define PANIC_IF_FALSE(condition, message)                                                         \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            panic_simple(message);                                                                 \
        }                                                                                          \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* PANIC_H */
