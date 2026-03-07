#include "print.h"
#include "serial.h"
#include "debug.h"

// Constante de versión centralizada
static constexpr const char* OS_VERSION = "GLOBEX_OS v0.010_x64";

// ==========================================
// Variable BSS de prueba (sin inicializador explícito)
// ==========================================
// Esta variable va a la sección .bss y DEBE ser inicializada a 0
// por el código de boot. Si no es 0, BSS initialization está rota.
static uint32_t bss_test_variable;

// Variable con inicializador explícito (va a .data)
static uint32_t data_test_variable = 0x12345678;

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

    // ==========================================
    // BSS Initialization Test
    // ==========================================
    print_str("=== BSS Initialization Test ===\r\n");
    
    print_str("BSS variable (should be 0x00000000): 0x");
    print_hex(bss_test_variable);
    print_str("\r\n");
    
    print_str("DATA variable (should be 0x12345678): 0x");
    print_hex(data_test_variable);
    print_str("\r\n");
    
    if (bss_test_variable == 0) {
        print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
        print_str("BSS INIT: PASSED\r\n");
        serial_write_str("[BSS TEST] PASSED: BSS initialized to zero\r\n");
    } else {
        print_set_color(PRINT_COLOR_LIGHT_RED, PRINT_COLOR_BLACK);
        print_str("BSS INIT: FAILED\r\n");
        serial_write_str("[BSS TEST] FAILED: BSS not zeroed!\r\n");
    }
    
    print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
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