#ifndef DECIMAL_UTILS_H
#define DECIMAL_UTILS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================
 * Internal Decimal Conversion Utilities
 * ==========================================
 * DRY (Don't Repeat Yourself) - Shared by print.cpp and serial.cpp
 *
 * These functions convert integers to decimal strings.
 * Output format: fixed-width decimal digits (no leading zeros except for 0)
 *
 * @note Buffers MUST be provided by caller - no internal static buffers
 * @note Output does NOT include leading zeros (except for value=0)
 * ========================================== */

/**
 * @brief Convert uint32 to decimal string
 *
 * Output format: decimal digits (e.g., "12345")
 *
 * @param buffer Output buffer (MUST be at least 12 bytes: 10 digits + null terminator + guard byte)
 * @param value 32-bit unsigned value to convert
 * @return Pointer to first character of result (for chaining)
 *
 * @note Maximum uint32 value is 4294967295 (10 digits)
 * @note Buffer layout: [unused][digits...][\0][guard]
 *       Function writes from buffer[10] down to buffer[1], then null at buffer[11]
 */
inline char* uint32_to_decimal_string(char* buffer, uint32_t value) {
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return buffer;
    }

    // Start from position 11 (end of buffer), work backwards
    // Maximum 10 digits means we use positions 1-10, with null at 11
    // FIX CRIT-001: Changed condition from "i > 1" to "i > 0" to allow
    // writing 10-digit numbers (UINT32_MAX = 4294967295)
    int i = 11;
    buffer[i] = '\0';

    while (value > 0 && i > 0) {
        buffer[--i] = '0' + (value % 10);
        value /= 10;
    }

    return &buffer[i];
}

/**
 * @brief Convert uint64 to decimal string
 *
 * Output format: decimal digits (e.g., "123456789012345")
 *
 * @param buffer Output buffer (MUST be at least 22 bytes: 20 digits + null terminator + guard byte)
 * @param value 64-bit unsigned value to convert
 * @return Pointer to first character of result (for chaining)
 *
 * @note Maximum uint64 value is 18446744073709551615 (20 digits)
 * @note Buffer layout: [unused][digits...][\0][guard]
 *       Function writes from buffer[20] down to buffer[1], then null at buffer[21]
 */
inline char* uint64_to_decimal_string(char* buffer, uint64_t value) {
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return buffer;
    }

    // Start from position 21 (end of buffer), work backwards
    // Maximum 20 digits means we use positions 1-20, with null at 21
    // FIX CRIT-002: Changed condition from "i > 1" to "i > 0" to allow
    // writing 20-digit numbers (UINT64_MAX = 18446744073709551615)
    int i = 21;
    buffer[i] = '\0';

    while (value > 0 && i > 0) {
        buffer[--i] = '0' + (value % 10);
        value /= 10;
    }

    return &buffer[i];
}

#ifdef __cplusplus
}
#endif

#endif /* DECIMAL_UTILS_H */
