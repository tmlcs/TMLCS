#include "vga.h"
#include "spinlock.h"
#include "constants.h"

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
 * =============================================================================
 */

/* =============================================================================
 * Memory Barrier Macro
 * =============================================================================
 * Prevents compiler reordering of memory operations.
 * On x86_64, hardware has strong ordering, but compiler barriers are still
 * needed to prevent the compiler from reordering volatile accesses.
 *
 * @assembly
 *   Instrucción: "" (empty inline assembly)
 *   Clobbers: "memory" - tells compiler that memory may be modified
 *   Propósito: Prevenir reordenamiento de instrucciones por el compilador
 *   Efectos:
 *     - mb()  : Barrera completa (lectura + escritura)
 *     - rmb() : Barrera de lectura (ordenamiento de loads)
 *     - wmb() : Barrera de escritura (ordenamiento de stores)
 *   Ciclos: 0 (es una directiva al compilador, no genera código)
 *   Barreras: Explícita vía clobber "memory"
 * 
 * @note En x86_64, el hardware tiene ordenamiento fuerte, pero el
 *       compilador puede reordenar instrucciones. Esta barrera lo previene.
 * @note El clobber "memory" le dice al compilador que cualquier acceso
 *       a memoria debe completarse antes de continuar.
 * 
 * @see mb(), rmb(), wmb() macros
 * =============================================================================
 */
#define mb()  __asm__ volatile ("" ::: "memory")
#define rmb() __asm__ volatile ("" ::: "memory")
#define wmb() __asm__ volatile ("" ::: "memory")

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
static volatile vga_cell_t* vga_buffer =
    reinterpret_cast<volatile vga_cell_t*>(VGA_BUFFER_ADDRESS);
static volatile bool vga_detected = false;
static volatile bool vga_initialized = false;

/* Cursor state */
static volatile size_t cursor_col = 0;
static volatile size_t cursor_row = 0;

/* Current color attribute */
static uint8_t current_color = 0;

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
        volatile uint16_t* dst = reinterpret_cast<volatile uint16_t*>(
            &vga_buffer[vga_index(r, 0)]);
        const volatile uint16_t* src = reinterpret_cast<const volatile uint16_t*>(
            &vga_buffer[vga_index(r + 1, 0)]);

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

size_t vga_get_cursor_col(void) {
    mb();
    return cursor_col;
}

size_t vga_get_cursor_row(void) {
    mb();
    return cursor_row;
}

void vga_set_cursor(size_t col, size_t row) {
    vga_begin_atomic();
    
    /* Clamp values to valid range */
    if (col >= VGA_COLS) {
        col = VGA_COLS - 1;
    }
    if (row >= VGA_ROWS) {
        row = VGA_ROWS - 1;
    }
    
    cursor_col = col;
    cursor_row = row;
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
 */

void vga_put_char_at(char character, size_t col, size_t row,
                     uint8_t fg, uint8_t bg) {
    vga_begin_atomic();
    
    if (!is_valid_position(row, col)) {
        vga_end_atomic();
        return;
    }
    
    size_t idx = vga_index(row, col);
    uint8_t color_attr = vga_make_color(fg, bg);
    
    /* Atomic 16-bit write */
    volatile uint16_t* cell_ptr = reinterpret_cast<volatile uint16_t*>(
        &vga_buffer[idx]);
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
        volatile uint16_t* cell_ptr = reinterpret_cast<volatile uint16_t*>(
            &vga_buffer[idx]);
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
    
    /* Limit maximum length */
    constexpr size_t MAX_STRING_LEN = 4096;
    
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
    vga_lock();
}

void vga_end_atomic(void) {
    vga_unlock();
}

bool vga_try_begin_atomic(void) {
    return vga_try_lock();
}
