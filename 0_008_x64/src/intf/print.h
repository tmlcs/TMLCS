#ifndef PRINT_H
#define PRINT_H

#include <stdint.h>
#include <stddef.h>

/* ==========================================
 * C/C++ linkage guards
 * ESSENTIAL for kernel development
 * ========================================== */
#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================
 * VGA Text Mode Colors
 * ==========================================
 * VGA color is one byte where:
 * - Bits 0-3: Foreground color
 * - Bits 4-7: Background color
 */
typedef enum PrintColor {
    PRINT_COLOR_BLACK = 0,
    PRINT_COLOR_BLUE = 1,
    PRINT_COLOR_GREEN = 2,
    PRINT_COLOR_CYAN = 3,
    PRINT_COLOR_RED = 4,
    PRINT_COLOR_MAGENTA = 5,
    PRINT_COLOR_BROWN = 6,
    PRINT_COLOR_LIGHT_GRAY = 7,
    PRINT_COLOR_DARK_GRAY = 8,
    PRINT_COLOR_LIGHT_BLUE = 9,
    PRINT_COLOR_LIGHT_GREEN = 10,
    PRINT_COLOR_LIGHT_CYAN = 11,
    PRINT_COLOR_LIGHT_RED = 12,
    PRINT_COLOR_PINK = 13,
    PRINT_COLOR_YELLOW = 14,
    PRINT_COLOR_WHITE = 15,
} PrintColor_t;

/* ==========================================
 * Print function API - High-Level Formatting
 * ==========================================
 * This module provides high-level formatted output functions.
 * It uses the low-level VGA driver (vga.h) for hardware access.
 * 
 * Separation of concerns:
 *   - vga.h/vga.cpp: Low-level hardware access, cursor, colors (SMP-safe)
 *   - print.h/print.cpp: High-level formatting (decimal, hex, strings)
 * 
 * SMP Safety:
 *   - All functions are SMP-safe via VGA driver spinlock
 *   - Use print_begin_atomic()/print_end_atomic() for multi-operation atomicity
 * ==========================================
 */

/**
 * @brief Detect VGA hardware before using print functions
 * @return true if VGA available, false otherwise
 * @note Must be called before print_clear() or any print function
 */
bool print_detect(void);

/**
 * @brief Initialize VGA driver (calls vga_init)
 * @note Call after print_detect()
 */
void print_init(void);

/**
 * @brief Clear entire screen and reset cursor
 * @note Requires print_detect() to be called beforehand
 */
void print_clear(void);

/**
 * @brief Print a single character
 * @param character Character to print (supports '\n', '\r', '\t')
 */
void print_char(char character);

/**
 * @brief Print null-terminated string
 * @param string Pointer to string (internally validated for NULL)
 */
void print_str(const char* string);

/**
 * @brief Print newline
 */
void print_newline(void);

/**
 * @brief Set foreground and background colors
 * @param foreground Foreground color (0-15). Valid values: PRINT_COLOR_*
 * @param background Background color (0-15). Valid values: PRINT_COLOR_*
 *
 * @note If foreground or background are out of range (>15), default color
 *       (white text on black background) is used safely.
 * @note This function is safe to use with unvalidated values - no UB.
 * @note Values are masked with 0x0F for compatibility.
 *
 * @example
 *     print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);  // OK
 *     print_set_color(255, 100);  // Invalid - uses default (white on black)
 */
void print_set_color(uint8_t foreground, uint8_t background);

/**
 * @brief Get current color attribute
 * @return Combined color byte (fg | (bg << 4))
 */
uint8_t print_get_color(void);

/**
 * @brief Build color attribute from foreground/background
 * @param fg Foreground color (0-15)
 * @param bg Background color (0-15)
 * @return Combined color byte
 */
uint8_t print_make_color(uint8_t fg, uint8_t bg);

/**
 * @brief Get current cursor column
 * @return Column index (0-79)
 */
size_t print_get_cursor_col(void);

/**
 * @brief Get current cursor row
 * @return Row index (0-24)
 */
size_t print_get_cursor_row(void);

/**
 * @brief Get current cursor position (legacy compatibility)
 * @param col Pointer to store column (can be NULL)
 * @param row Pointer to store row (can be NULL)
 * @note If col or row are NULL, that parameter is not written
 */
void print_get_cursor(size_t* col, size_t* row);

/**
 * @brief Set cursor position
 * @param col New column (0-79)
 * @param row New row (0-24)
 * @note Values outside valid range are clamped
 */
void print_set_cursor(size_t col, size_t row);

/**
 * @brief Advance cursor (handles newline automatically)
 * @return true if scrolled, false otherwise
 */
bool print_advance_cursor(void);

/**
 * @brief Print 32-bit unsigned integer in hexadecimal
 * @param value Value to print (unsigned)
 */
void print_hex(uint32_t value);

/**
 * @brief Print 32-bit unsigned integer in decimal
 * @param value Value to print (unsigned)
 */
void print_dec(uint32_t value);

/**
 * @brief Print 64-bit unsigned integer in hexadecimal
 * @param value Value to print (unsigned)
 * @note Prints 16 hexadecimal digits with "0x" prefix
 */
void print_hex64(uint64_t value);

/**
 * @brief Print 64-bit unsigned integer in decimal
 * @param value Value to print (unsigned)
 * @note Supports values up to 18,446,744,073,709,551,615
 */
void print_dec64(uint64_t value);

/**
 * @brief Print 32-bit signed integer in decimal
 * @param value Value to print (signed)
 * @note Handles negative values with '-' prefix
 */
void print_dec_signed(int32_t value);

/**
 * @brief Print 64-bit signed integer in decimal
 * @param value Value to print (signed)
 * @note Handles negative values with '-' prefix
 */
void print_dec64_signed(int64_t value);

/**
 * @brief Check if VGA has been initialized
 * @return true if VGA is initialized and ready for use
 */
int print_is_initialized(void);

/* ==========================================
 * Atomic Operations (for SMP)
 * ==========================================
 * Use these for multiple operations that must be atomic.
 * The VGA spinlock is acquired/released as a pair.
 * ==========================================
 */

/**
 * @brief Begin atomic operation (acquire VGA spinlock)
 * @note Use with print_end_atomic() for multi-operation atomicity
 */
void print_begin_atomic(void);

/**
 * @brief End atomic operation (release VGA spinlock)
 * @note Must be paired with print_begin_atomic()
 */
void print_end_atomic(void);

/**
 * @brief Try to begin atomic operation (non-blocking)
 * @return true if lock acquired, false otherwise
 */
bool print_try_begin_atomic(void);

#ifdef __cplusplus
}
#endif

#endif /* PRINT_H */
