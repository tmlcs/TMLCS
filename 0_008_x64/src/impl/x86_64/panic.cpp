#include "panic.h"
#include "print.h"
#include "serial.h"

/* ==========================================
 * panic() - Kernel panic with error code
 * ========================================== */
void panic(const char* message, uint32_t error_code) {
    /* ==========================================
     * Disable interrupts
     * ========================================== */
    __asm__ volatile ("cli");

    /* ==========================================
     * Try to initialize serial if not ready
     * ========================================== */
    if (!serial_is_initialized()) {
        serial_init_default();
    }

    /* ==========================================
     * Error message via serial (always available)
     * ========================================== */
    serial_write_str("\r\n\r\n");
    serial_write_str("!!! KERNEL PANIC !!!\r\n");
    serial_write_str("\r\n");
    
    if (message != nullptr) {
        serial_write_str("Error: ");
        serial_write_str(message);
        serial_write_str("\r\n");
    }
    
    serial_write_str("Error Code: 0x");
    serial_write_hex(error_code);
    serial_write_str("\r\n");
    serial_write_str("\r\nSystem halted.\r\n");

    /* ==========================================
     * Error message via VGA (if initialized)
     * ==========================================
     * HIGH-007: Use print_is_initialized() instead of print_detect()
     * to ensure VGA is fully initialized before using print_clear().
     * print_detect() only tests hardware presence, but print_clear()
     * requires full initialization.
     * ========================================== */
    if (print_is_initialized()) {
        /* Red background, bright white text for maximum contrast */
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_RED);
        print_clear();

        print_str("!!! KERNEL PANIC !!!\r\n");
        print_str("\r\n");

        if (message != nullptr) {
            print_str("Error: ");
            print_str(message);
            print_str("\r\n");
        }

        print_str("Error Code: 0x");
        print_hex(error_code);
        print_str("\r\n");
        print_str("\r\nSystem halted.\r\n");
    }

    /* ==========================================
     * Infinite loop with HLT - never return
     * ========================================== */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/* ==========================================
 * panic_simple() - Kernel panic without error code
 * ========================================== */
void panic_simple(const char* message) {
    panic(message, 0);
}
