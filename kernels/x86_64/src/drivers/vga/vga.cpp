#include "vga.h"
#include "barriers.h"
#include "constants.h"
#include "spinlock.h"

/* Use centralized barriers from barriers.h
 * Previous: Local #define mb()/rmb()/wmb() definitions
 * Now: #include "barriers.h" provides consistent barriers
 * @see src/arch/x86_64/include/barriers.h
 */

/* =============================================================================
 * VGA Driver Implementation
 * =============================================================================
 *
 * Low-level VGA text mode driver for x86_64.
 * All public functions are SMP-safe via spinlock protection.
 *
 * Internal functions (static) assume lock is already held.
 * Public functions acquire/release lock per-operation.
 *
 * MEMORY ORDERING:
 *   - volatile prevents compiler optimization/caching
 *   - Spinlock provides mutual exclusion and memory barriers
 *   - Explicit memory barriers ensure ordering within critical sections
 *   - x86_64 has strong memory ordering, but barriers prevent compiler reordering
 *   - MAJ-001: Barriers now centralized in barriers.h
 * =============================================================================
 */

/* =============================================================================
 * VGA Hardware State
 * =============================================================================
 * SMP SAFETY:
 *   These variables are protected by g_vga_lock spinlock.
 *   All accesses use memory barriers to ensure:
 *     - Proper ordering across CPUs
 *     - Visibility of changes to all processors
 *     - Prevention of compiler reordering
 * =============================================================================
 */

/* Video buffer - VOLATILE for hardware MMIO */
static volatile vga_cell_t* vga_buffer = reinterpret_cast<volatile vga_cell_t*>(VGA_BUFFER_ADDRESS);
static volatile bool vga_detected = false;
static volatile bool vga_initialized = false;

/* Cursor state */
static volatile size_t cursor_col = 0;
static volatile size_t cursor_row = 0;

/* Current color attribute */
static uint8_t current_color = 0;

/* Token saved by vga_begin_atomic() and consumed by vga_end_atomic().
 *
 * Single-CPU safety: the VGA lock is non-reentrant, so once CPU 0 holds it
 * (between vga_begin_atomic and vga_end_atomic) no second acquisition on the
 * same CPU can succeed and overwrite the token.
 *
 * NOTE (SMP): On a multi-CPU system a second CPU calling vga_try_begin_atomic
 * could win the CAS and overwrite g_vga_atomic_tok while CPU 0 is still inside
 * its atomic section, corrupting the saved RFLAGS/IF state.  Until a per-CPU
 * token or a caller-supplied token is used, vga_try_begin_atomic must not be
 * called concurrently with vga_begin_atomic on a different CPU. */
static spinlock_token_t g_vga_atomic_tok;

/* =============================================================================
 * Internal Helper Functions (lock must be held)
 * =============================================================================
 * These functions assume the VGA lock is already held.
 * They are used internally to avoid deadlock.
 * =============================================================================
 */

/* Calculate linear index in buffer */
static inline size_t vga_index(size_t row, size_t col) {
    return row * VGA_COLS + col;
}

/* Validate bounds before accessing */
static inline bool is_valid_position(size_t row, size_t col) {
    return (row < VGA_ROWS) && (col < VGA_COLS);
}

/* Validate VGA color range (0-15) */
static inline bool is_valid_color(uint8_t color) {
    return color <= 15;
}

/* Internal clear row - assumes lock held */
static void vga_clear_row_internal(size_t row) {
    if (!is_valid_position(row, 0)) {
        return;
    }

    for (size_t col = 0; col < VGA_COLS; col++) {
        size_t idx = vga_index(row, col);
        vga_buffer[idx].character = ' ';
        vga_buffer[idx].color = current_color;
    }
    mb();
}

/* Internal newline - assumes lock held */
static void vga_newline_internal(void) {
    cursor_col = 0;
    mb();

    if (cursor_row < VGA_ROWS - 1) {
        cursor_row++;
        mb();
        return;
    }

    /* Scroll: move rows 1..24 to rows 0..23 */
    for (size_t r = 0; r < VGA_ROWS - 1; r++) {
        volatile uint16_t* dst = reinterpret_cast<volatile uint16_t*>(&vga_buffer[vga_index(r, 0)]);
        const volatile uint16_t* src =
            reinterpret_cast<const volatile uint16_t*>(&vga_buffer[vga_index(r + 1, 0)]);

        for (size_t c = 0; c < VGA_COLS; c++) {
            dst[c] = src[c];
        }
    }
    mb();

    vga_clear_row_internal(VGA_ROWS - 1);
    mb();
}

/* =============================================================================
 * Initialization & Detection
 * =============================================================================
 */

bool vga_detect(void) {
    /* Assume VGA available by default for early kernel */
    /* Real detection requires BIOS access which may not be available */

    /* Safe read/write test on VGA buffer */
    volatile vga_cell_t* test_ptr = &vga_buffer[0];
    uint8_t saved_char = test_ptr->character;
    uint8_t saved_color = test_ptr->color;

    /* Write test pattern */
    test_ptr->character = 'X';
    test_ptr->color = 0x07;
    mb();

    /* Verify write succeeded */
    bool success = (test_ptr->character == 'X') && (test_ptr->color == 0x07);

    /* Restore original values */
    test_ptr->character = saved_char;
    test_ptr->color = saved_color;
    mb();

    vga_detected = success;
    mb();

    return vga_detected;
}

bool vga_init(void) {
    if (!vga_detected && !vga_detect()) {
        return false;
    }

    /* Initialize default color (white on black) */
    current_color = vga_make_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    /* Initialize cursor position */
    cursor_col = 0;
    cursor_row = 0;
    mb();

    vga_initialized = true;
    mb();

    return true;
}

bool vga_is_initialized(void) {
    mb();
    return vga_initialized;
}

void vga_clear(void) {
    vga_begin_atomic();

    if (!vga_initialized) {
        vga_end_atomic();
        return;
    }

    for (size_t row = 0; row < VGA_ROWS; row++) {
        vga_clear_row_internal(row);
    }

    cursor_col = 0;
    cursor_row = 0;
    mb();

    vga_end_atomic();
}

void vga_clear_row(size_t row) {
    vga_begin_atomic();

    if (vga_initialized) {
        vga_clear_row_internal(row);
    }

    vga_end_atomic();
}

/* =============================================================================
 * Color Control
 * =============================================================================
 */

uint8_t vga_make_color(uint8_t fg, uint8_t bg) {
    /* Clamp invalid colors to valid range (0-15)
     * This prevents color overflow into higher bits
     * Invalid colors are silently clamped (no error returned)
     */
    return (fg & 0x0F) | ((bg & 0x0F) << 4);
}

uint8_t vga_set_color(uint8_t foreground, uint8_t background) {
    vga_begin_atomic();

    if (!is_valid_color(foreground) || !is_valid_color(background)) {
        current_color = vga_make_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        mb();
        vga_end_atomic();
        return current_color;
    }

    current_color = vga_make_color(foreground, background);
    mb();

    vga_end_atomic();
    return current_color;
}

uint8_t vga_get_color(void) {
    mb();
    return current_color;
}

/* =============================================================================
 * Cursor Control
 * =============================================================================
 */

/* VGA-LOW-001: Cursor getters are intentionally unlocked.
 *
 * On x86_64 a single aligned load of a `volatile size_t` is hardware-atomic
 * (64-bit aligned word read).  The compiler barrier `mb()` prevents the
 * compiler from hoisting the read above any preceding stores.  Adding the
 * spinlock here would risk deadlock for callers that already hold vga_lock
 * (e.g. inside a vga_begin_atomic() section). */
size_t vga_get_cursor_col(void) {
    mb();
    return cursor_col;
}

size_t vga_get_cursor_row(void) {
    mb();
    return cursor_row;
}

void vga_set_cursor(vga_pos_t pos) {
    vga_begin_atomic();

    /* Clamp values to valid range */
    if (pos.col.value >= VGA_COLS) {
        pos.col.value = VGA_COLS - 1;
    }
    if (pos.row.value >= VGA_ROWS) {
        pos.row.value = VGA_ROWS - 1;
    }

    cursor_col = pos.col.value;
    cursor_row = pos.row.value;
    mb();

    vga_end_atomic();
}

bool vga_advance_cursor(void) {
    vga_begin_atomic();

    bool scrolled = false;

    cursor_col++;

    if (cursor_col >= VGA_COLS) {
        vga_newline_internal();
        scrolled = true;
    }

    mb();
    vga_end_atomic();

    return scrolled;
}

/* =============================================================================
 * Low-Level Character Output
 * =============================================================================
 *
 * INTERRUPT SAFETY: [INTERRUPT-UNSAFE]
 * These functions acquire vga_lock and MUST NOT be called from:
 *   - Interrupt handlers
 *   - Exception handlers
 *   - NMI handlers
 *
 * For interrupt-safe output, use:
 *   - vga_put_char_early() - Direct MMIO, no lock
 *   - vga_put_string_early() - Direct MMIO, no lock
 *   - serial output (has separate lock)
 *
 * Tracking issue: #SMP-003 (VGA interrupt safety)
 * =============================================================================
 */

/**
 * @brief Write character at specific position with color
 * @note Using type-safe wrappers prevents parameter swapping bugs
 */
void vga_put_char_at(char character, vga_col_t col, vga_row_t row, vga_colors_t colors) {
    vga_begin_atomic();

    if (!is_valid_position(row.value, col.value)) {
        vga_end_atomic();
        return;
    }

    /* Validate colors before use
     * Invalid colors are clamped to safe defaults (white on black)
     * This prevents display corruption from invalid color values
     */
    uint8_t fg = colors.fg;
    uint8_t bg = colors.bg;
    if (!is_valid_color(fg)) {
        fg = VGA_COLOR_WHITE;
    }
    if (!is_valid_color(bg)) {
        bg = VGA_COLOR_BLACK;
    }

    size_t idx = vga_index(row.value, col.value);
    uint8_t color_attr = vga_make_color(fg, bg);

    /* Atomic 16-bit write */
    volatile uint16_t* cell_ptr = reinterpret_cast<volatile uint16_t*>(&vga_buffer[idx]);
    *cell_ptr = static_cast<uint16_t>(static_cast<uint8_t>(character)) |
                (static_cast<uint16_t>(color_attr) << 8);

    mb();
    vga_end_atomic();
}

void vga_put_char(char character) {
    vga_begin_atomic();

    if (!vga_initialized) {
        vga_end_atomic();
        return;
    }

    switch (character) {
    case '\n':
        vga_newline_internal();
        vga_end_atomic();
        return;

    case '\r':
        cursor_col = 0;
        mb();
        vga_end_atomic();
        return;

    case '\t':
        cursor_col = (cursor_col + 8) & ~7;
        if (cursor_col >= VGA_COLS) {
            vga_newline_internal();
        }
        mb();
        vga_end_atomic();
        return;

    default:
        break;
    }

    if (cursor_col >= VGA_COLS) {
        vga_newline_internal();
    }

    if (is_valid_position(cursor_row, cursor_col)) {
        size_t idx = vga_index(cursor_row, cursor_col);

        /* Atomic 16-bit write */
        volatile uint16_t* cell_ptr = reinterpret_cast<volatile uint16_t*>(&vga_buffer[idx]);
        *cell_ptr = static_cast<uint16_t>(static_cast<uint8_t>(character)) |
                    (static_cast<uint16_t>(current_color) << 8);

        cursor_col++;
        mb();
    }

    vga_end_atomic();
}

void vga_put_string(const char* str) {
    if (str == nullptr) {
        return;
    }

    vga_begin_atomic();

    if (!vga_initialized) {
        vga_end_atomic();
        return;
    }

    /* MED-004 FIX: Limit to VGA_ROWS * VGA_COLS (screen capacity).
     *
     * Previous limit was an arbitrary 256 which silently truncated any
     * string longer than two screen rows.  The natural hard ceiling is the
     * total number of display cells (25 × 80 = 2000): writing more
     * characters than fit on screen is pointless and the lock-hold-time
     * is still bounded.
     *
     * This matches vga_put_string_early() which uses the same limit.
     */
    constexpr size_t MAX_STRING_LEN = VGA_ROWS * VGA_COLS;

    for (size_t i = 0; i < MAX_STRING_LEN && str[i] != '\0'; i++) {
        switch (str[i]) {
        case '\n':
            vga_newline_internal();
            break;

        case '\r':
            cursor_col = 0;
            mb();
            break;

        case '\t':
            cursor_col = (cursor_col + 8) & ~7;
            if (cursor_col >= VGA_COLS) {
                vga_newline_internal();
            }
            mb();
            break;

        default:
            if (cursor_col >= VGA_COLS) {
                vga_newline_internal();
            }

            if (is_valid_position(cursor_row, cursor_col)) {
                size_t idx = vga_index(cursor_row, cursor_col);
                volatile uint16_t* cell_ptr =
                    reinterpret_cast<volatile uint16_t*>(&vga_buffer[idx]);
                *cell_ptr = static_cast<uint16_t>(static_cast<uint8_t>(str[i])) |
                            (static_cast<uint16_t>(current_color) << 8);
                cursor_col++;
                mb();
            }
            break;
        }
    }

    vga_end_atomic();
}

void vga_newline(void) {
    vga_begin_atomic();

    if (vga_initialized) {
        vga_newline_internal();
    }

    vga_end_atomic();
}

/* =============================================================================
 * Atomic Operations (for SMP)
 * =============================================================================
 */

void vga_begin_atomic(void) {
    g_vga_atomic_tok = vga_lock();
}

void vga_end_atomic(void) {
    vga_unlock(g_vga_atomic_tok);
}

bool vga_try_begin_atomic(void) {
    return vga_try_lock(&g_vga_atomic_tok);
}

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

void vga_put_char_early(char character, vga_pos_t pos, uint8_t color) {
    /* Direct MMIO write - no lock, no bounds checking beyond screen */
    if (pos.row.value >= VGA_ROWS || pos.col.value >= VGA_COLS) {
        return;
    }

    size_t idx = pos.row.value * VGA_COLS + pos.col.value;
    volatile uint16_t* cell_ptr = reinterpret_cast<volatile uint16_t*>(&vga_buffer[idx]);

    /* Atomic 16-bit write: character + color attribute */
    *cell_ptr = static_cast<uint16_t>(static_cast<uint8_t>(character)) |
                (static_cast<uint16_t>(color) << 8);

    mb(); /* Ensure write is visible */
}

/**
 * @brief Write string directly to VGA buffer without locking
 * [INTERRUPT-SAFE] - Does not acquire spinlock
 *
 * @param str Null-terminated string to write
 * @param pos Position struct with starting column (0-79) and row (0-24)
 * @param color Color attribute byte
 *
 *   Both vga_put_string() and this function cap at VGA_ROWS * VGA_COLS.
 *   vga_put_string() previously capped at 256 (MED-004 fix); this function had no limit. A very
 * long string could wrap around the screen multiple times, overwriting its own panic message and
 * producing confusing output.
 *
 *   Now limits output to one full screen (2000 characters = 80x25).
 *   This ensures panic messages remain readable and don't self-overwrite.
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
void vga_put_string_early(const char* str, vga_pos_t pos, uint8_t color) {
    if (str == nullptr) {
        return;
    }

    /* Direct MMIO writes - no lock */
    size_t current_col = pos.col.value;
    size_t current_row = pos.row.value;
    size_t start_col = pos.col.value;

    /* Limit maximum characters to prevent screen wraparound.
     * One full screen = 80 cols * 25 rows = 2000 characters.
     * This ensures panic messages don't overwrite themselves.
     */
    constexpr size_t MAX_EARLY_STRING_LEN = VGA_ROWS * VGA_COLS;
    size_t char_count = 0;

    for (size_t i = 0; str[i] != '\0' && char_count < MAX_EARLY_STRING_LEN; i++) {
        /* Handle basic control characters */
        if (str[i] == '\n') {
            current_col = start_col; /* Return to start of line */
            current_row++;
            if (current_row >= VGA_ROWS) {
                current_row = VGA_ROWS - 1; /* Clamp to last row */
            }
            char_count++;
            continue;
        }

        if (str[i] == '\r') {
            current_col = start_col; /* Return to start of line */
            char_count++;
            continue;
        }

        /* Stop at end of row */
        if (current_col >= VGA_COLS) {
            break;
        }

        /* Stop at end of screen */
        if (current_row >= VGA_ROWS) {
            break;
        }

        vga_put_char_early(str[i], vga_make_pos(vga_col(current_col), vga_row(current_row)), color);
        current_col++;
        char_count++;
    }

    mb(); /* Ensure all writes are visible */
}
