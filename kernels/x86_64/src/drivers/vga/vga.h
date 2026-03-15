#ifndef VGA_H
#define VGA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * VGA Text Mode Driver API
 * =============================================================================
 *
 * Low-level VGA text mode driver for x86_64.
 * Provides direct access to VGA hardware at 0xB8000.
 *
 * Features:
 *   - 80x25 text mode
 *   - 16 colors (4-bit foreground + 4-bit background)
 *   - Hardware cursor control
 *   - SMP-safe with spinlock protection
 *
 * Memory Layout:
 *   - VGA Buffer: 0xB8000 (80 * 25 * 2 = 4000 bytes)
 *   - Each cell: 2 bytes (char + attribute)
 *
 * SMP Safety:
 *   - All functions are SMP-safe via spinlock
 *   - Lock is acquired per-operation
 *   - Use vga_begin_atomic()/vga_end_atomic() for multi-operation atomicity
 *
 * Usage:
 *   // Single operation (automatically locked)
 *   vga_put_char('A', 0, 0, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
 *
 *   // Multiple operations (manually locked)
 *   vga_begin_atomic();
 *   vga_set_cursor(0, 0);
 *   vga_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
 *   vga_put_string("Hello");
 *   vga_end_atomic();
 *
 * Tracking issue: #SMP-001
 * =============================================================================
 */

/* =============================================================================
 * VGA Color Definitions
 * =============================================================================
 * 4-bit color values for foreground/background
 * =============================================================================
 */
typedef enum {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_YELLOW = 14,
    VGA_COLOR_WHITE = 15
} vga_color_t;

/* =============================================================================
 * VGA Hardware Constants
 * =============================================================================
 * Note: VGA_BUFFER_ADDRESS, VGA_ROWS, VGA_COLS, VGA_CELL_SIZE are defined
 *       in constants.h. We re-define VGA_BUFFER_SIZE here with correct byte size.
 * =============================================================================
 */
#define VGA_BUFFER_SIZE_BYTES (VGA_ROWS * VGA_COLS * VGA_CELL_SIZE)

/* =============================================================================
 * VGA Cell Structure (packed for 2 bytes)
 * =============================================================================
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t character;
    uint8_t color;
} vga_cell_t;
#pragma pack(pop)

/* =============================================================================
 * Initialization & Detection
 * =============================================================================
 */

/**
 * Detect VGA hardware
 * @return true if VGA detected, false otherwise
 */
bool vga_detect(void);

/**
 * Initialize VGA driver
 * @return true if initialization successful
 */
bool vga_init(void);

/**
 * Check if VGA is initialized
 * @return true if initialized
 */
bool vga_is_initialized(void);

/**
 * Clear entire screen
 */
void vga_clear(void);

/**
 * Clear a specific row
 * @param row Row number (0-24)
 */
void vga_clear_row(size_t row);

/* =============================================================================
 * Color Control
 * =============================================================================
 */

/**
 * Set current color attribute
 * @param foreground Foreground color (0-15)
 * @param background Background color (0-15)
 * @return Combined color attribute byte
 *
 * @note Invalid colors (>15) are clamped to safe defaults
 *       (white foreground, black background)
 * @note This function validates inputs before use
 */
uint8_t vga_set_color(uint8_t foreground, uint8_t background);

/**
 * Get current color attribute
 * @return Combined color attribute byte
 */
uint8_t vga_get_color(void);

/**
 * Build color attribute byte from foreground/background
 * @param fg Foreground color (0-15)
 * @param bg Background color (0-15)
 * @return Combined color byte
 *
 * @note Colors are clamped to 4-bit range (0-15) using bitwise AND
 *       This prevents overflow into higher bits
 * @note No error returned - invalid colors are silently clamped
 */
uint8_t vga_make_color(uint8_t fg, uint8_t bg);

/* =============================================================================
 * Cursor Control
 * =============================================================================
 */

/**
 * Get current cursor column
 * @return Column index (0-79)
 */
size_t vga_get_cursor_col(void);

/**
 * Get current cursor row
 * @return Row index (0-24)
 */
size_t vga_get_cursor_row(void);

/**
 * Set cursor position
 * @param col Column (0-79)
 * @param row Row (0-24)
 */
void vga_set_cursor(size_t col, size_t row);

/**
 * Move cursor to next position (handles newline)
 * @return true if scrolled, false otherwise
 */
bool vga_advance_cursor(void);

/* =============================================================================
 * Low-Level Character Output
 * =============================================================================
 */

/**
 * Write character at specific position with color
 * @param character ASCII character
 * @param col Column (0-79)
 * @param row Row (0-24)
 * @param fg Foreground color (0-15)
 * @param bg Background color (0-15)
 *
 * @note Invalid colors are clamped to safe defaults
 *       (white foreground, black background) before use
 * @note Position is validated - invalid positions are silently ignored
 * @note SMP-safe via spinlock protection
 */
void vga_put_char_at(char character, size_t col, size_t row, uint8_t fg, uint8_t bg);

/**
 * Write character at cursor position with current color
 * @param character ASCII character
 */
void vga_put_char(char character);

/**
 * Write null-terminated string at cursor position
 * @param str Null-terminated string
 */
void vga_put_string(const char* str);

/**
 * Write newline (carriage return + line feed)
 */
void vga_newline(void);

/* =============================================================================
 * Atomic Operations (for SMP)
 * =============================================================================
 * Use these for multiple operations that must be atomic
 * =============================================================================
 */

/**
 * Begin atomic operation (acquire spinlock)
 */
void vga_begin_atomic(void);

/**
 * End atomic operation (release spinlock)
 */
void vga_end_atomic(void);

/**
 * Try to begin atomic operation (non-blocking)
 * @return true if lock acquired, false otherwise
 */
bool vga_try_begin_atomic(void);

#ifdef __cplusplus
}
#endif

#endif /* VGA_H */
