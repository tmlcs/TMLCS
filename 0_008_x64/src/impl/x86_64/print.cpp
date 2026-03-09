#include "print.h"
#include "string.h"
#include "hex_utils.h"
#include "decimal_utils.h"
#include "constants.h"

// ==========================================
// Memory Barrier Macro
// ==========================================
// Prevents compiler/CPU from reordering memory accesses
// ESSENTIAL for hardware MMIO and shared variables
#define memory_barrier() __asm__ volatile ("" ::: "memory")

// ==========================================
// VGA character structure (packed for 2 bytes)
// ==========================================
struct VgaChar {
    uint8_t character;
    uint8_t color;

    // Constructor for easy assignment
    constexpr VgaChar() : character(0), color(0) {}
    constexpr VgaChar(uint8_t c, uint8_t col) : character(c), color(col) {}
};

// ==========================================
// Video buffer - VOLATILE for hardware MMIO
// ==========================================
static volatile VgaChar* vga_buffer = reinterpret_cast<volatile VgaChar*>(VGA_BUFFER_ADDRESS);

// ==========================================
// VGA hardware state
// ==========================================
/* =============================================================================
 * SMP SAFETY WARNING [CRIT-004]
 * =============================================================================
 * This driver is NOT safe for SMP (Symmetric Multi-Processing) environments.
 * The following variables are shared without synchronization:
 *   - cursor_col, cursor_row  (cursor position state)
 *   - current_color           (color attribute state)
 *   - vga_buffer[]            (hardware MMIO region at 0xB8000)
 *
 * For future SMP support, these must be protected by:
 *   1. Spinlocks - Mutual exclusion for multi-core access
 *   2. Per-CPU cursor state with IPI for cross-CPU updates
 *   3. Atomic operations (if available in freestanding mode)
 *
 * Race conditions that can occur in SMP:
 *   - Two CPUs writing to cursor_col/row simultaneously
 *   - Color attribute corruption from concurrent print_set_color()
 *   - Garbled output from interleaved VGA buffer writes
 *
 * Current status: Uni-processor only (UP-safe)
 * Tracking issue: #SMP-001
 * =============================================================================
 */
// IMPORTANT: These variables are NOT thread-safe for SMP.
// For future SMP/multitasking support, consider:
// 1. Use atomic operations (if available in freestanding)
// 2. Implement per-CPU cursor state
// 3. Protect with spinlock
//
// NOTE: volatile prevents compiler optimization but does NOT
// guarantee atomicity on multi-core systems.
static volatile bool vga_detected = false;
static volatile bool vga_initialized = false;  // HIGH-007: Separate flag for initialization
static volatile size_t cursor_col = 0;
static volatile size_t cursor_row = 0;
static uint8_t current_color = (PRINT_COLOR_WHITE & 0x0F) | ((PRINT_COLOR_BLACK & 0x0F) << 4);

// ==========================================
// Helper functions
// ==========================================

// Calculate linear index in buffer
static inline size_t vga_index(size_t row, size_t col) {
    return row * VGA_COLS + col;
}

// Validate bounds before accessing
static inline bool is_valid_position(size_t row, size_t col) {
    return (row < VGA_ROWS) && (col < VGA_COLS);
}

// Validate VGA color range (0-15)
static inline bool is_valid_color(uint8_t color) {
    return color <= 15;
}

// ==========================================
// VGA hardware detection
// ==========================================

bool print_detect(void) {
    // Assume VGA available by default for early kernel
    // Real detection requires BIOS access which may not be available
    // in all emulation/hardware environments

    // Safe read/write test on VGA buffer
    volatile VgaChar* test_ptr = &vga_buffer[0];
    uint8_t saved_char = test_ptr->character;
    uint8_t saved_color = test_ptr->color;

    test_ptr->character = 0xAA;
    test_ptr->color = 0x55;

    // Verify write was successful
    bool exists = (test_ptr->character == 0xAA && test_ptr->color == 0x55);

    // Restore original values
    test_ptr->character = saved_char;
    test_ptr->color = saved_color;

    vga_detected = exists;

    // HIGH-007: If VGA is detected, mark as initialized
    // This allows safe use of print_clear() and other functions
    if (exists) {
        vga_initialized = true;
    }

    return exists;
}

// ==========================================
// Public function implementations
// ==========================================

void clear_row(size_t row) {
    if (!vga_detected || row >= VGA_ROWS) {
        return;  // Validation: VGA must be detected and row must be valid
    }

    // Optimization: write both bytes (character + color) as single u16
    // VGA text mode: each cell is 2 bytes (char: low byte, color: high byte)
    // This reduces memory accesses from 160 (80*2) to 80 u16 writes
    const uint16_t clear_word = static_cast<uint16_t>(' ') | (static_cast<uint16_t>(current_color) << 8);

    // Reinterpret buffer as uint16_t* for 2-byte writes
    volatile uint16_t* row_ptr = reinterpret_cast<volatile uint16_t*>(&vga_buffer[vga_index(row, 0)]);

    for (size_t c = 0; c < VGA_COLS; c++) {
        row_ptr[c] = clear_word;
    }
    memory_barrier();  // Prevent reordering after hardware writes
}

void print_clear(void) {
    if (!vga_detected) {
        return;  // Silently fail if VGA not detected
    }

    for (size_t r = 0; r < VGA_ROWS; r++) {
        clear_row(r);
    }
    
    // Reset cursor with memory barrier for synchronization
    cursor_col = 0;
    cursor_row = 0;
    memory_barrier();
}

void print_newline(void) {
    /*
     * SMP SAFETY [CRIT-004]: This function modifies shared cursor state
     * (cursor_col, cursor_row) and VGA buffer without synchronization.
     * In SMP, concurrent calls can cause:
     *   - Cursor position corruption during scroll
     *   - Screen corruption from interleaved scroll operations
     *   - Lost characters from race conditions
     *
     * Protection required for SMP:
     *   - Acquire spinlock before checking/modifying cursor
     *   - Hold lock through entire scroll operation
     *   - Release lock after all VGA writes complete
     */
    if (!vga_detected) {
        return;  // VGA not detected
    }

    // Explicit volatile read with memory barrier
    memory_barrier();
    cursor_col = 0;
    memory_barrier();

    if (cursor_row < VGA_ROWS - 1) {
        cursor_row++;
        memory_barrier();
        return;
    }

    // Scroll: move rows 1..24 to rows 0..23
    // Optimization: copy row by row using u16 accesses (2 bytes per access)
    // Instead of 4 byte accesses per access, use 1 u16 access per column

    // Copy rows 1..24 to rows 0..23
    for (size_t r = 0; r < VGA_ROWS - 1; r++) {
        volatile uint16_t* dst = reinterpret_cast<volatile uint16_t*>(&vga_buffer[vga_index(r, 0)]);
        const volatile uint16_t* src = reinterpret_cast<const volatile uint16_t*>(&vga_buffer[vga_index(r + 1, 0)]);

        // Copy 80 columns × 2 bytes = 80 u16 accesses
        for (size_t c = 0; c < VGA_COLS; c++) {
            dst[c] = src[c];
        }
    }
    memory_barrier();

    clear_row(VGA_ROWS - 1);
    memory_barrier();
}

void print_char(char character) {
    /*
     * SMP SAFETY [CRIT-004]: This function reads/modifies shared cursor state
     * (cursor_col, cursor_row) without synchronization. In SMP, concurrent calls
     * can cause:
     *   - Cursor position corruption
     *   - Characters written to wrong positions
     *   - Lost updates from race conditions
     *
     * Protection required for SMP:
     *   - Acquire spinlock before reading cursor state
     *   - Hold lock through entire read-modify-write cycle
     *   - Release lock after updating VGA buffer
     */
    // Read cursor with memory barrier for synchronization
    memory_barrier();
    size_t col = cursor_col;
    size_t row = cursor_row;

    switch (character) {
        case '\n':
            print_newline();
            return;
        case '\r':
            cursor_col = 0;
            memory_barrier();
            return;
        case '\t':
            // Advance to next tab stop (every 8 columns)
            col = (col + 8) & ~7;
            if (col >= VGA_COLS) {
                cursor_row++;
                cursor_col = 0;
                memory_barrier();
                print_newline();
            } else {
                cursor_col = col;
                memory_barrier();
            }
            return;
        default:
            break;
    }

    if (col >= VGA_COLS) {
        print_newline();
        // print_newline ya actualiza cursor, leer de nuevo
        memory_barrier();
        col = cursor_col;
        row = cursor_row;
    }

    if (is_valid_position(row, col)) {
        // ==========================================
        // ATOMIC VGA CELL WRITE - Fix CRIT-005
        // ==========================================
        // Write character and color as a SINGLE 16-bit atomic operation
        // to prevent flickering or corrupted characters.
        //
        // VGA cell format (little-endian x86_64):
        //   Byte 0 (low):  character (ASCII)
        //   Byte 1 (high): color attribute
        //
        // This prevents the hardware from reading intermediate state
        // where character != color (flickering/corruption).
        // ==========================================
        const size_t idx = vga_index(row, col);
        const uint16_t cell_value = 
            static_cast<uint16_t>(static_cast<uint8_t>(character)) |
            (static_cast<uint16_t>(current_color) << 8);
        
        // Single atomic 16-bit write to VGA buffer
        volatile uint16_t* cell_ptr = reinterpret_cast<volatile uint16_t*>(
            &vga_buffer[idx]);
        *cell_ptr = cell_value;
        
        // Memory barrier AFTER write to ensure it completes
        memory_barrier();
        
        // Update cursor position
        cursor_col = col + 1;
        memory_barrier();
    }
}

void print_str(const char* str) {
    if (str == nullptr) {
        return;  // Null pointer validation
    }

    // Limit maximum length to prevent infinite writes
    constexpr size_t MAX_PRINT_LEN = 4096;

    for (size_t i = 0; i < MAX_PRINT_LEN && str[i] != '\0'; i++) {
        print_char(str[i]);
    }
    memory_barrier();  // Ensure all writes complete
}

void print_set_color(uint8_t foreground, uint8_t background) {
    /*
     * SMP SAFETY [CRIT-004]: This function modifies shared state (current_color)
     * without synchronization. In SMP, concurrent calls can cause:
     *   - Color corruption (interleaved writes)
     *   - Incorrect color applied to characters
     *
     * Protection required for SMP:
     *   - Acquire spinlock before reading/writing current_color
     *   - Use atomic compare-and-swap if available
     */
    // Range validation (0-15) for VGA colors
    // If values are out of range, use defaults (white on black)
    if (!is_valid_color(foreground) || !is_valid_color(background)) {
        // Invalid values - use default and return
        current_color = (PRINT_COLOR_WHITE & 0x0F) | ((PRINT_COLOR_BLACK & 0x0F) << 4);
        memory_barrier();
        return;
    }

    // Valid values - apply mask and combine
    current_color = (foreground & 0x0F) | ((background & 0x0F) << 4);
    memory_barrier();  // Ensure color change propagates
}

void print_hex(uint32_t value) {
    char buffer[11];  // "0x" + 8 digits + null = 11 bytes
    
    // Use shared utility function from string.cpp (DRY principle)
    uint32_to_hex_string(buffer, value);
    
    print_str(buffer);
}

void print_dec(uint32_t value) {
    char buffer[12];  // Maximum 10 digits + null
    int i = 10;

    buffer[11] = '\0';

    if (value == 0) {
        print_char('0');
        return;
    }

    while (value > 0 && i > 0) {
        buffer[i--] = '0' + (value % 10);
        value /= 10;
    }

    print_str(&buffer[i + 1]);
}

void print_hex64(uint64_t value) {
    char buffer[19];  // "0x" + 16 digits + null = 19 bytes
    
    // Use shared utility function from string.cpp (DRY principle)
    uint64_to_hex_string(buffer, value);
    
    print_str(buffer);
}

void print_dec64(uint64_t value) {
    char buffer[22];  // Maximum 20 digits + null
    int i = 20;

    buffer[21] = '\0';

    if (value == 0) {
        print_char('0');
        return;
    }

    while (value > 0 && i > 0) {
        buffer[i--] = '0' + (value % 10);
        value /= 10;
    }

    print_str(&buffer[i + 1]);
}

void print_dec_signed(int32_t value) {
    if (value < 0) {
        print_char('-');
        // Convert to positive avoiding overflow on INT32_MIN
        print_dec64((uint64_t)(-(int64_t)value));
    } else {
        print_dec((uint32_t)value);
    }
}

void print_dec64_signed(int64_t value) {
    if (value < 0) {
        print_char('-');
        // Convert to positive avoiding overflow on INT64_MIN
        print_dec64((uint64_t)(-value));
    } else {
        print_dec64((uint64_t)value);
    }
}

/* ==========================================
 * Query Functions [3.6]
 * ========================================== */

void print_get_cursor(size_t* col, size_t* row) {
    memory_barrier();
    if (col != nullptr) {
        *col = cursor_col;
    }
    if (row != nullptr) {
        *row = cursor_row;
    }
    memory_barrier();
}

void print_get_color(uint8_t* fg, uint8_t* bg) {
    memory_barrier();
    if (fg != nullptr) {
        *fg = current_color & 0x0F;  // Extraer foreground (bits 0-3)
    }
    if (bg != nullptr) {
        *bg = (current_color >> 4) & 0x0F;  // Extraer background (bits 4-7)
    }
    memory_barrier();
}

void print_set_cursor(size_t col, size_t row) {
    // ==========================================
    // BOUNDARY VALIDATION - Prevent out-of-bounds cursor
    // ==========================================
    // VGA text mode: 80 columns × 25 rows
    // Invalid positions are clamped to valid range [0, 79] × [0, 24]
    //
    // Note: size_t is unsigned, so negative values wrap to large positives.
    // The upper-bound checks below handle this case correctly.
    // ==========================================

    // Clamp column to [0, VGA_COLS - 1]
    if (col >= VGA_COLS) {
        col = VGA_COLS - 1;  // Clamp to rightmost column
    }

    // Clamp row to [0, VGA_ROWS - 1]
    if (row >= VGA_ROWS) {
        row = VGA_ROWS - 1;  // Clamp to bottom row
    }

    // Set cursor with memory barriers for synchronization
    memory_barrier();
    cursor_col = col;
    cursor_row = row;
    memory_barrier();
}

/* ==========================================
 * Public API - Initialization Status [HIGH-007]
 * ==========================================
 * These functions allow checking if VGA is initialized
 * separately from hardware detection.
 * ========================================== */

int print_is_initialized(void) {
    return vga_initialized ? 1 : 0;
}
