#ifndef VGA_H
#define VGA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "constants.h"

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
 * =============================================================================
 * INTERRUPT SAFETY WARNING - CRITICAL
 * =============================================================================
 *
 * DO NOT call VGA functions from interrupt handlers!
 *
 * DEADLOCK SCENARIO:
 *   1. Main code acquires vga_lock() in vga_put_string()
 *   2. vga_put_string() holds lock for up to 4096 character iterations
 *   3. Interrupt occurs on SAME CPU
 *   4. Interrupt handler calls print_str() -> vga_put_string()
 *   5. vga_put_string() tries to acquire vga_lock() - DEADLOCK!
 *
 * SOLUTIONS:
 *   - Use vga_put_string_early() for early panic (no lock, direct MMIO)
 *   - Use serial output from interrupt handlers (has separate lock)
 *   - Buffer messages and print from main context
 *
 * Functions marked [INTERRUPT-UNSAFE] must NOT be called from:
 *   - ISR (Interrupt Service Routines)
 *   - Exception handlers
 *   - NMI handlers
 *   - Any code that may be interrupted
 *
 * Functions marked [INTERRUPT-SAFE] can be called from interrupt context:
 *   - vga_put_string_early() - Direct MMIO, no lock
 *   - early_panic() in kernel/main.cpp - Uses vga_put_string_early()
 *
 * Tracking issue: #SMP-003 (VGA interrupt safety)
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
 * Type-Safe Position Types - Prevents parameter swapping bugs
 * =============================================================================
 * Using distinct types for column and row prevents accidentally swapping them
 * when calling functions. This is a common source of bugs in VGA drivers.
 *
 * These are NewType wrappers - zero-cost at runtime, but provide compile-time
 * type safety. The compiler will reject swapped arguments.
 *
 * Usage:
 *   vga_pos_t pos = vga_make_pos(40, 12);  // col=40, row=12
 *   vga_set_cursor(pos);
 *
 * Named constructors make intent clear:
 *   vga_pos_t center = vga_pos_center();    // (40, 12) - center of 80x25
 *   vga_pos_t origin = vga_pos_origin();    // (0, 0)  - top-left
 * =============================================================================
 */

/**
 * @brief Type-safe column index (0-79)
 * @note Distinct from vga_row_t to prevent parameter swapping
 */
typedef struct {
    size_t value;
} vga_col_t;

/**
 * @brief Type-safe row index (0-24)
 * @note Distinct from vga_col_t to prevent parameter swapping
 */
typedef struct {
    size_t value;
} vga_row_t;

/**
 * @brief VGA position with type-safe column and row
 */
typedef struct {
    vga_col_t col; /**< Column (0-79) - type-safe */
    vga_row_t row; /**< Row (0-24) - type-safe */
} vga_pos_t;

/**
 * @brief Create a column value
 * @param value Column index (0-79)
 * @return vga_col_t wrapper
 */
static inline vga_col_t vga_col(size_t value) {
    vga_col_t c;
    c.value = value;
    return c;
}

/**
 * @brief Create a row value
 * @param value Row index (0-24)
 * @return vga_row_t wrapper
 */
static inline vga_row_t vga_row(size_t value) {
    vga_row_t r;
    r.value = value;
    return r;
}

/**
 * Create a position from column and row
 * @param col Column wrapper from vga_col()
 * @param row Row wrapper from vga_row()
 * @return vga_pos_t struct
 *
 * @example
 *   // Type-safe: compiler rejects vga_make_pos(vga_row(12), vga_col(40))
 *   vga_pos_t pos = vga_make_pos(vga_col(40), vga_row(12));
 */
static inline vga_pos_t vga_make_pos(vga_col_t col, vga_row_t row) {
    vga_pos_t pos;
    pos.col = col;
    pos.row = row;
    return pos;
}

/**
 * Get origin position (top-left corner)
 * @return vga_pos_t (0, 0)
 */
static inline vga_pos_t vga_pos_origin(void) {
    vga_pos_t pos;
    pos.col = vga_col(0);
    pos.row = vga_row(0);
    return pos;
}

/**
 * Get center position (middle of 80x25 screen)
 * @return vga_pos_t (40, 12)
 */
static inline vga_pos_t vga_pos_center(void) {
    vga_pos_t pos;
    pos.col = vga_col(VGA_COLS / 2);
    pos.row = vga_row(VGA_ROWS / 2);
    return pos;
}

/**
 * @brief VGA color attribute with foreground and background
 * @note Grouping colors in a struct prevents swapping with position parameters
 */
typedef struct {
    uint8_t fg; /**< Foreground color (0-15) */
    uint8_t bg; /**< Background color (0-15) */
} vga_colors_t;

/**
 * @brief Create a color attribute
 * @param fg Foreground color (0-15)
 * @param bg Background color (0-15)
 * @return vga_colors_t struct
 */
static inline vga_colors_t vga_colors(uint8_t fg, uint8_t bg) {
    vga_colors_t c;
    c.fg = fg;
    c.bg = bg;
    return c;
}

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
 * @param pos Position struct with column (0-79) and row (0-24)
 *
 * @note Using vga_pos_t prevents accidentally swapping col/row
 * @see vga_make_pos(), vga_pos_origin(), vga_pos_center()
 */
void vga_set_cursor(vga_pos_t pos);

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
 * @param col Column wrapper from vga_col()
 * @param row Row wrapper from vga_row()
 * @param colors Color attribute from vga_colors()
 *
 * @note Using type-safe wrappers prevents accidentally swapping parameters
 * @note Invalid colors are clamped to safe defaults (white on black)
 * @note Position is validated - invalid positions are silently ignored
 * @note SMP-safe via spinlock protection
 *
 * @example
 *   vga_put_char_at('A', vga_col(40), vga_row(12), vga_colors(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
 */
void vga_put_char_at(char character, vga_col_t col, vga_row_t row, vga_colors_t colors);

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

/* =============================================================================
 * Interrupt-Safe Functions (EARLY PANIC / DEBUG)
 * =============================================================================
 * These functions DO NOT use locks and are safe to call from:
 *   - Interrupt handlers
 *   - Exception handlers
 *   - Early boot code (before spinlock initialization)
 *   - Panic/error paths where deadlock is unacceptable
 *
 * WARNING: These functions are NOT SMP-safe!
 *   - No locking means concurrent access may corrupt output
 *   - Only use when system is already in fatal state
 *   - Do NOT use for normal operation
 *
 * Tracking issue: #SMP-003 (VGA interrupt safety)
 * =============================================================================
 */

/**
 * Write character directly to VGA buffer without locking
 * [INTERRUPT-SAFE] - Does not acquire spinlock
 *
 * @param character ASCII character to write
 * @param pos Position struct with column (0-79) and row (0-24)
 * @param color Color attribute byte
 *
 * @note This function is NOT SMP-safe - use only in panic/fatal paths
 * @note No bounds checking - caller must ensure valid positions
 * @note No cursor tracking - direct buffer write only
 * @note Using vga_pos_t prevents accidentally swapping col/row
 *
 * @see early_panic() in kernel/main.cpp for usage example
 * @see vga_make_pos(), vga_pos_origin()
 */
void vga_put_char_early(char character, vga_pos_t pos, uint8_t color);

/**
 * Write string directly to VGA buffer without locking
 * [INTERRUPT-SAFE] - Does not acquire spinlock
 *
 * @param str Null-terminated string to write
 * @param pos Position struct with starting column (0-79) and row (0-24)
 * @param color Color attribute byte
 *
 * @note This function is NOT SMP-safe - use only in panic/fatal paths
 * @note No bounds checking beyond screen dimensions
 * @note No cursor tracking - direct buffer writes
 * @note Stops at newline or end of row
 * @note Using vga_pos_t prevents accidentally swapping col/row
 *
 * @see early_panic() in kernel/main.cpp for usage example
 * @see vga_make_pos(), vga_pos_origin()
 */
void vga_put_string_early(const char* str, vga_pos_t pos, uint8_t color);

#ifdef __cplusplus
}
#endif

#endif /* VGA_H */
