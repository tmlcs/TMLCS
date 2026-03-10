#include "print.h"
#include "vga.h"
#include "string.h"
#include "hex_utils.h"
#include "decimal_utils.h"
#include "constants.h"

/* =============================================================================
 * Print Driver - High-Level Formatting API
 * =============================================================================
 * 
 * This module provides high-level formatted output functions.
 * It uses the low-level VGA driver (vga.h) for hardware access.
 * 
 * Separation of concerns:
 *   - vga.h/vga.cpp: Low-level hardware access, cursor, colors (SMP-safe)
 *   - print.h/print.cpp: High-level formatting (decimal, hex, strings)
 * 
 * SMP Safety:
 *   - VGA driver functions are already SMP-safe
 *   - This module calls vga_*() functions which handle locking internally
 * =============================================================================
 */

/* =============================================================================
 * VGA Detection & Initialization
 * =============================================================================
 */

bool print_detect(void) {
    return vga_detect();
}

void print_init(void) {
    vga_init();
}

int print_is_initialized(void) {
    return vga_is_initialized() ? 1 : 0;
}

/* =============================================================================
 * Basic Output Functions
 * =============================================================================
 */

void print_char(char character) {
    vga_put_char(character);
}

void print_str(const char* str) {
    vga_put_string(str);
}

void print_newline(void) {
    vga_newline();
}

void print_clear(void) {
    vga_clear();
}

void print_clear_row(size_t row) {
    vga_clear_row(row);
}

/* =============================================================================
 * Color Functions
 * =============================================================================
 */

void print_set_color(uint8_t foreground, uint8_t background) {
    vga_set_color(foreground, background);
}

uint8_t print_get_color(void) {
    return vga_get_color();
}

uint8_t print_make_color(uint8_t fg, uint8_t bg) {
    return vga_make_color(fg, bg);
}

/* =============================================================================
 * Cursor Functions
 * =============================================================================
 */

size_t print_get_cursor_col(void) {
    return vga_get_cursor_col();
}

size_t print_get_cursor_row(void) {
    return vga_get_cursor_row();
}

/* Legacy compatibility function */
void print_get_cursor(size_t* col, size_t* row) {
    if (col) *col = vga_get_cursor_col();
    if (row) *row = vga_get_cursor_row();
}

void print_set_cursor(size_t col, size_t row) {
    vga_set_cursor(col, row);
}

bool print_advance_cursor(void) {
    return vga_advance_cursor();
}

/* =============================================================================
 * Formatted Output - Hexadecimal
 * =============================================================================
 */

void print_hex(uint32_t value) {
    char buffer[11];  /* "0x" + 8 digits + null = 11 bytes */
    uint32_to_hex_string(buffer, value);
    vga_put_string(buffer);
}

void print_hex64(uint64_t value) {
    char buffer[19];  /* "0x" + 16 digits + null = 19 bytes */
    uint64_to_hex_string(buffer, value);
    vga_put_string(buffer);
}

/* =============================================================================
 * Formatted Output - Decimal
 * =============================================================================
 */

void print_dec(uint32_t value) {
    char buffer[12];  /* Maximum 10 digits + null */
    vga_put_string(uint32_to_decimal_string(buffer, value));
}

void print_dec_signed(int32_t value) {
    if (value < 0) {
        vga_put_char('-');
        /* Use two's complement to avoid undefined behavior.
         * For INT32_MIN (-2147483648), negation would overflow in signed arithmetic.
         * Casting to uint32_t first, then negating in unsigned arithmetic is safe.
         * Example: INT32_MIN -> (uint32_t)0x80000000 -> -0x80000000 = 0x80000000 = 2147483648
         */
        uint32_t abs_value = 0 - static_cast<uint32_t>(value);
        print_dec(abs_value);
    } else {
        print_dec(static_cast<uint32_t>(value));
    }
}

void print_dec64(uint64_t value) {
    char buffer[22];  /* Maximum 20 digits + null */
    vga_put_string(uint64_to_decimal_string(buffer, value));
}

void print_dec64_signed(int64_t value) {
    if (value < 0) {
        vga_put_char('-');
        /* Use two's complement to avoid undefined behavior.
         * For INT64_MIN (-9223372036854775808), negation would overflow in signed arithmetic.
         * Casting to uint64_t first, then negating in unsigned arithmetic is safe.
         * Example: INT64_MIN -> (uint64_t)0x8000000000000000 -> -0x8000... = 0x8000... = 9223372036854775808
         */
        uint64_t abs_value = 0 - static_cast<uint64_t>(value);
        print_dec64(abs_value);
    } else {
        print_dec64(static_cast<uint64_t>(value));
    }
}

/* =============================================================================
 * Atomic Operations (for SMP)
 * =============================================================================
 * Expose VGA atomic operations for multi-operation atomicity
 * =============================================================================
 */

void print_begin_atomic(void) {
    vga_begin_atomic();
}

void print_end_atomic(void) {
    vga_end_atomic();
}

bool print_try_begin_atomic(void) {
    return vga_try_begin_atomic();
}
