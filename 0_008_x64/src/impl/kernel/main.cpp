#include "print.h"
#include "serial.h"
#include "debug.h"

// Constante de versión centralizada
static constexpr const char* OS_VERSION = "GLOBEX_OS v0.009_x64";

// El kernel nunca debe retornar - usar noreturn
extern "C" [[noreturn]] void kernel_main() {
    // ==========================================
    // Inicializar serial primero (más seguro que VGA)
    // ==========================================
    serial_init_default();

    // ==========================================
    // Detectar hardware VGA
    // ==========================================
    print_detect();
    
    // Inicializar VGA (solo si detectado)
    print_clear();
    print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);

    // Mensaje inicial VGA
    print_str("Welcome to ");
    print_str(OS_VERSION);
    print_str("\r\n");
    print_str("64-bit kernel on C++!\r\n");
    print_str("\r\n");

    // Estado del serial
    print_str("Serial console: ");
    if (serial_is_initialized()) {
        print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
        print_str("ENABLED\r\n");
    } else {
        print_set_color(PRINT_COLOR_LIGHT_RED, PRINT_COLOR_BLACK);
        print_str("NOT AVAILABLE\r\n");
    }

    // Debug mode status
    print_str("Debug mode: ");
    #if DEBUG_ENABLE
    print_str("ON\r\n");
    #else
    print_str("OFF\r\n");
    #endif

    print_str("\r\n");
    print_str("System ready.\r\n");

    // ==========================================
    // Mensajes por serial (siempre se escriben)
    // ==========================================
    serial_write_str("\r\n=== GLOBEX_OS Kernel Started ===\r\n");
    serial_write_str(OS_VERSION);
    serial_write_str("\r\n");
    serial_write_str("System halted - press reset to restart\r\n");

    // ==========================================
    // Kernel idle loop - nunca retornar
    // ==========================================
    for (;;) {
        __asm__ volatile ("hlt");
    }
}