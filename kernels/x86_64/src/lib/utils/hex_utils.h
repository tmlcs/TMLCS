#ifndef HEX_UTILS_H
#define HEX_UTILS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================
 * Internal Hex Conversion Utilities
 * ==========================================
 * DRY (Don't Repeat Yourself) - Shared by print.cpp and serial.cpp
 *
 * These functions convert integers to hexadecimal strings.
 * Output format: "0x" prefix + fixed-width hex digits
 *
 * @note Buffers MUST be provided by caller - no internal static buffers
 * @note Output includes leading zeros for consistent width
 * ========================================== */

/**
 * @brief Convert uint64 to hex string
 *
 * Output format: "0x" + 16 hex digits (e.g., "0x123456789ABCDEF0")
 *
 * @param buffer Output buffer (MUST be at least 19 bytes: 2 + 16 + 1)
 * @param value 64-bit unsigned value to convert
 * @return Pointer to buffer (for chaining)
 */
inline char* uint64_to_hex_string(char* buffer, uint64_t value) {
    static const char hex_chars[] = "0123456789ABCDEF";

    buffer[0] = '0';
    buffer[1] = 'x';

    // Convert each nibble (4 bits) to hex character
    // Start from most significant nibble (bits 60-63)
    for (int i = 0; i < 16; i++) {
        buffer[2 + i] = hex_chars[(value >> (60 - i * 4)) & 0xF];
    }

    buffer[18] = '\0';  // Null terminator

    return buffer;
}

/**
 * @brief Convert uint32 to hex string
 *
 * Output format: "0x" + 8 hex digits (e.g., "0x12345678")
 *
 * @param buffer Output buffer (MUST be at least 11 bytes: 2 + 8 + 1)
 * @param value 32-bit unsigned value to convert
 * @return Pointer to buffer (for chaining)
 */
inline char* uint32_to_hex_string(char* buffer, uint32_t value) {
    static const char hex_chars[] = "0123456789ABCDEF";

    buffer[0] = '0';
    buffer[1] = 'x';

    // Convert each nibble (4 bits) to hex character
    // Start from most significant nibble (bits 28-31)
    for (int i = 0; i < 8; i++) {
        buffer[2 + i] = hex_chars[(value >> (28 - i * 4)) & 0xF];
    }

    buffer[10] = '\0';  // Null terminator

    return buffer;
}

#ifdef __cplusplus
}
#endif

#endif /* HEX_UTILS_H */
