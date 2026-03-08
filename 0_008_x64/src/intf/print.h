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
 * Print function API
 * ========================================== */

/**
 * @brief Detect VGA hardware before using print functions
 * @return true if VGA available, false otherwise
 * @note Must be called before print_clear() or any print function
 */
bool print_detect(void);

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
 * @brief Get current cursor position
 * @param col Pointer to store column (can be NULL)
 * @param row Pointer to store row (can be NULL)
 * @note If col or row are NULL, that parameter is not written
 */
void print_get_cursor(size_t* col, size_t* row);

/**
 * @brief Get current colors
 * @param fg Pointer to store foreground (can be NULL)
 * @param bg Pointer to store background (can be NULL)
 * @note Values are in range 0-15
 */
void print_get_color(uint8_t* fg, uint8_t* bg);

/**
 * @brief Set cursor position
 * @param col New column (0-79)
 * @param row New row (0-24)
 * @note Values outside valid range are clamped:
 *       - col >= 80 → col = 79
 *       - row >= 25 → row = 24
 * @note size_t is unsigned; negative values passed via cast will wrap
 *       and be clamped to maximum (79 or 24 respectively).
 * @note Does not check vga_detected - caller must ensure VGA is initialized.
 * @note Uses memory barriers for thread-safe cursor updates.
 */
void print_set_cursor(size_t col, size_t row);

#ifdef __cplusplus
}
#endif

#endif /* PRINT_H */
