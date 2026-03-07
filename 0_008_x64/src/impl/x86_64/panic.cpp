#include "panic.h"
#include "print.h"
#include "serial.h"

/* ==========================================
 * panic() - Kernel panic con código de error
 * ========================================== */
void panic(const char* message, uint32_t error_code) {
    /* ==========================================
     * Deshabilitar interrupciones
     * ========================================== */
    __asm__ volatile ("cli");

    /* ==========================================
     * Intentar inicializar serial si no está listo
     * ========================================== */
    if (!serial_is_initialized()) {
        serial_init_default();
    }

    /* ==========================================
     * Mensaje de error por serial (siempre disponible)
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
     * Mensaje de error por VGA (si está disponible)
     * ========================================== */
    if (print_detect()) {
        /* Fondo rojo, texto blanco brillante para máximo contraste */
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
     * Loop infinito con HLT - nunca retornar
     * ========================================== */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/* ==========================================
 * panic_simple() - Kernel panic sin código de error
 * ========================================== */
void panic_simple(const char* message) {
    panic(message, 0);
}
