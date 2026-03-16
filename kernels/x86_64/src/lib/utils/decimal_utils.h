#ifndef DECIMAL_UTILS_H
#define DECIMAL_UTILS_H

#include <stddef.h>
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

/* Buffer size constants for decimal conversion
 *
 * Layout explanation:
 *   uint32: max 10 digits (4294967295) + 1 null terminator = 11 bytes minimum
 *   uint64: max 20 digits + 1 null terminator = 21 bytes minimum
 *
 * Extra byte (+1) provides:
 *   - Safety margin to prevent off-by-one errors
 *   - Space for potential future extensions (e.g., sign character)
 *   - Alignment padding
 *
 * NOT a "guard" byte in the security sense - it's usable space.
 */
#define DECIMAL_UINT32_BUFFER_SIZE 12 /* 10 digits + 1 null terminator (+1 safety margin) */
#define DECIMAL_UINT64_BUFFER_SIZE 22 /* 20 digits + 1 null terminator (+1 safety margin) */
#define DECIMAL_UINT32_MAX_DIGITS 10
#define DECIMAL_UINT64_MAX_DIGITS 20

/**
 * @brief Convert uint32 to decimal string
 *
 * Output format: decimal digits (e.g., "12345")
 *
 * @param buffer Output buffer (MUST be at least DECIMAL_UINT32_BUFFER_SIZE bytes)
 * @param value 32-bit unsigned value to convert
 * @return Pointer to first character of result (for chaining)
 *         Returns buffer pointing to "\0" if bounds check fails
 *
 * @note Maximum uint32 value is 4294967295 (10 digits)
 * @note Buffer layout: [digits 0-9][null terminator at 10 or 11]
 *       Function writes from end of buffer backwards to beginning
 *       Maximum 10 digits uses positions 1-10 or 0-9 depending on value
 *       Position 11 is always null terminator (or position 10 for 10-digit numbers)
 *
 * @security Bounds-checked to prevent buffer underflow.
 *           - Pre-counts digits before writing
 *           - Explicit bounds check in loop (i > 0)
 *           - Returns empty string if bounds check fails
 */
inline char* uint32_to_decimal_string(char* buffer, uint32_t value) {
    /* Handle zero case */
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return buffer;
    }

    /* Bounds check: count digits before writing to ensure we don't overflow
     * Maximum uint32 value is 4294967295 (10 digits).
     * DECIMAL_UINT32_BUFFER_SIZE = 12 (10 digits + 1 null terminator + 1 safety margin)
     */
    uint32_t temp = value;
    size_t digit_count = 0;
    while (temp > 0) {
        temp /= 10;
        digit_count++;
    }

    /* Safety check for buffer bounds
     * This should never trigger for uint32_t (max 10 digits), but protects against:
     *   - Future changes to digit_count calculation
     *     - Buffer size misconfiguration
     *     - Caller providing undersized buffer
     */
    if (digit_count > DECIMAL_UINT32_MAX_DIGITS) {
        buffer[0] = '\0';
        return buffer;
    }

    /* Null pointer safety check */
    if (buffer == nullptr) {
        return nullptr;
    }

    /* Start from position 11 (end of buffer), work backwards
     * Buffer indices: [0..10] for digits, [11] for null terminator
     * Maximum 10 digits uses positions 1-10 or 0-9 depending on value
     * The extra byte at position 11 provides safety margin
     */
    size_t i = DECIMAL_UINT32_BUFFER_SIZE - 1; /* i = 11 */
    buffer[i] = '\0';

    /* Write digits from right to left with explicit bounds check
     * Loop invariant: i > 0 before decrement, so --i >= 0
     * This ensures we never write before buffer[0]
     *
     * @assembly
     *   Operations: modulo, division, array store
     *   No special instructions required
     *   Cycles: ~10-20 per digit (depends on division latency)
     */
    while (value > 0) {
        /* Explicit bounds check to prevent underflow
         * If i reaches 0, we've exhausted buffer space - abort
         * This is a safety net - digit_count check above should prevent this
         */
        if (i == 0) {
            /* Buffer overflow - should never happen with correct buffer size */
            buffer[0] = '\0';
            return buffer;
        }

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
 * @param buffer Output buffer (MUST be at least DECIMAL_UINT64_BUFFER_SIZE bytes)
 * @param value 64-bit unsigned value to convert
 * @return Pointer to first character of result (for chaining)
 *         Returns buffer pointing to "\0" if bounds check fails
 *
 * @note Maximum uint64 value is 18446744073709551615 (20 digits)
 * @note Buffer layout: [digits 0-19][null terminator at 20 or 21]
 *       Function writes from end of buffer backwards to beginning
 *       Maximum 20 digits uses positions 1-20 or 0-19 depending on value
 *       Position 21 is always null terminator (or position 20 for 20-digit numbers)
 *
 * @security Bounds-checked to prevent buffer underflow.
 *           - Pre-counts digits before writing
 *           - Explicit bounds check in loop (i > 0)
 *           - Returns empty string if bounds check fails
 */
inline char* uint64_to_decimal_string(char* buffer, uint64_t value) {
    /* Handle zero case */
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return buffer;
    }

    /* Bounds check: count digits before writing to ensure we don't overflow
     * Maximum uint64 value is 18446744073709551615 (20 digits).
     * DECIMAL_UINT64_BUFFER_SIZE = 22 (20 digits + 1 null terminator + 1 safety margin)
     */
    uint64_t temp = value;
    size_t digit_count = 0;
    while (temp > 0) {
        temp /= 10;
        digit_count++;
    }

    /* Safety check for buffer bounds
     * This should never trigger for uint64_t (max 20 digits), but protects against:
     *   - Future changes to digit_count calculation
     *     - Buffer size misconfiguration
     *     - Caller providing undersized buffer
     */
    if (digit_count > DECIMAL_UINT64_MAX_DIGITS) {
        buffer[0] = '\0';
        return buffer;
    }

    /* Null pointer safety check */
    if (buffer == nullptr) {
        return nullptr;
    }

    /* Start from position 21 (end of buffer), work backwards
     * Buffer indices: [0..20] for digits, [21] for null terminator
     * Maximum 20 digits uses positions 1-20 or 0-19 depending on value
     * The extra byte at position 21 provides safety margin
     */
    size_t i = DECIMAL_UINT64_BUFFER_SIZE - 1; /* i = 21 */
    buffer[i] = '\0';

    /* Write digits from right to left with explicit bounds check
     * Loop invariant: i > 0 before decrement, so --i >= 0
     * This ensures we never write before buffer[0]
     *
     * @assembly
     *   Operations: modulo, division, array store (64-bit)
     *   No special instructions required
     *   Cycles: ~20-40 per digit (64-bit division is slower)
     */
    while (value > 0) {
        /* Explicit bounds check to prevent underflow
         * If i reaches 0, we've exhausted buffer space - abort
         * This is a safety net - digit_count check above should prevent this
         */
        if (i == 0) {
            /* Buffer overflow - should never happen with correct buffer size */
            buffer[0] = '\0';
            return buffer;
        }

        buffer[--i] = '0' + (value % 10);
        value /= 10;
    }

    return &buffer[i];
}

#ifdef __cplusplus
}
#endif

#endif /* DECIMAL_UTILS_H */
