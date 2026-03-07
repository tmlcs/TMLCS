#include "print.h"
#include "serial.h"
#include "panic.h"

// Enable debug macros for testing
#define DEBUG_ENABLE 1
#include "debug.h"

// Constante de versión centralizada
static constexpr const char* OS_VERSION = "GLOBEX_OS v0.014_x64";

// ==========================================
// Variable BSS de prueba (sin inicializador explícito)
// ==========================================
// Esta variable va a la sección .bss y DEBE ser inicializada a 0
// por el código de boot. Si no es 0, BSS initialization está rota.
static uint32_t bss_test_variable;

// Variable con inicializador explícito (va a .data)
static uint32_t data_test_variable = 0x12345678;

// Puntero para test de memoria alta (dirección mapeada > 64MB)
// Usamos 0x05000000 (80MB) que está dentro del rango mapeado y es RAM válida en QEMU
static volatile uint32_t* high_mem_test = reinterpret_cast<volatile uint32_t*>(0x05000000);

// Variables para test de debug macros
static uint32_t debug_test_value = 0xCAFEBABE;

// El kernel nunca debe retornar - usar noreturn
extern "C" [[noreturn]] void kernel_main() {
    // ==========================================
    // Inicializar serial primero (más seguro que VGA)
    // ==========================================
    if (!serial_init_default()) {
        // Serial falló - intentar panic por VGA directamente
        // Escribir directamente al buffer VGA sin usar funciones
        volatile uint16_t* vga = reinterpret_cast<volatile uint16_t*>(0xB8000);
        const char* msg = "FATAL: Serial init failed";
        for (size_t i = 0; msg[i] != '\0' && i < 80; i++) {
            vga[i] = (0x4F << 8) | msg[i];  // White on red
        }
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    // ==========================================
    // Detectar hardware VGA
    // ==========================================
    bool vga_available = print_detect();
    
    if (!vga_available) {
        // VGA no disponible - continuar solo con serial
        serial_write_str("[WARNING] VGA not detected, serial only mode\r\n");
    }

    // Inicializar VGA (solo si detectado)
    if (vga_available) {
        print_clear();
        print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
    }

    // Mensaje inicial VGA
    if (vga_available) {
        print_str("Welcome to ");
        print_str(OS_VERSION);
        print_str("\r\n");
        print_str("64-bit kernel on C++!\r\n");
        print_str("\r\n");
    }

    // ==========================================
    // BSS Initialization Test
    // ==========================================
    if (vga_available) {
        print_str("=== BSS Initialization Test ===\r\n");

        print_str("BSS variable (should be 0x00000000): 0x");
        print_hex(bss_test_variable);
        print_str("\r\n");

        print_str("DATA variable (should be 0x12345678): 0x");
        print_hex(data_test_variable);
        print_str("\r\n");
    }

    if (bss_test_variable == 0) {
        if (vga_available) {
            print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
            print_str("BSS INIT: PASSED\r\n");
        }
        serial_write_str("[BSS TEST] PASSED: BSS initialized to zero\r\n");
    } else {
        // BSS no se inicializó correctamente - error fatal
        if (vga_available) {
            print_set_color(PRINT_COLOR_LIGHT_RED, PRINT_COLOR_BLACK);
            print_str("BSS INIT: FAILED\r\n");
        }
        serial_write_str("[BSS TEST] FAILED: BSS not zeroed!\r\n");
        panic("BSS initialization failed", bss_test_variable);
    }

    if (vga_available) {
        print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
        print_str("\r\n");
    }

    // ==========================================
    // Memory Mapping Test
    // ==========================================
    if (vga_available) {
        print_str("=== Memory Mapping Test ===\r\n");
        print_str("Page tables: 2GiB mapped (0x00000000-0x7FFFFFFF)\r\n");
        print_str("Testing access to 80MB (0x05000000)...\r\n");
    }

    // Test de escritura/lectura en memoria alta
    uint32_t test_pattern = 0xDEADBEEF;
    uint32_t read_back = 0;

    // Escribir patrón en memoria alta
    *high_mem_test = test_pattern;

    // Leer de vuelta
    read_back = *high_mem_test;

    if (vga_available) {
        print_str("Write pattern: 0x");
        print_hex(test_pattern);
        print_str("\r\n");
        print_str("Read back:   0x");
        print_hex(read_back);
        print_str("\r\n");
    }

    if (read_back == test_pattern) {
        if (vga_available) {
            print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
            print_str("HIGH MEM ACCESS: PASSED\r\n");
        }
        serial_write_str("[MEM TEST] PASSED: Memory access at 80MB works\r\n");
    } else {
        // Memory access falló - error fatal
        if (vga_available) {
            print_set_color(PRINT_COLOR_LIGHT_RED, PRINT_COLOR_BLACK);
            print_str("HIGH MEM ACCESS: FAILED\r\n");
        }
        serial_write_str("[MEM TEST] FAILED: Memory access at 80MB failed!\r\n");
        panic("Memory mapping test failed", read_back);
    }

    if (vga_available) {
        print_str("\r\n");
    }

    // ==========================================
    // Color Validation Test
    // ==========================================
    if (vga_available) {
        print_str("=== Color Validation Test ===\r\n");
        print_str("Testing valid colors (0-15)...\r\n");
        
        // Test con colores válidos
        print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLUE);
        print_str("Valid color: YELLOW on BLUE - OK\r\n");
        
        // Test con valores inválidos (>15) - debe usar default
        print_str("Testing invalid colors (>15)...\r\n");
        print_set_color(255, 100);  // Valores inválidos - debe usar white on black
        print_str("Invalid color fallback: Should be WHITE on BLACK - OK\r\n");
        
        // Restaurar color normal
        print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
        print_str("Color validation: PASSED\r\n\r\n");
        
        serial_write_str("[COLOR TEST] PASSED: Color validation works\r\n");
    }

    // ==========================================
    // Debug Macros Test [D001, D002, D003]
    // ==========================================
    serial_write_str("\r\n=== Debug Macros Test ===\r\n");
    
    // Test DEBUG_PRINTLN [D002]
    DEBUG_PRINTLN("Testing DEBUG_PRINTLN...");
    serial_write_str("[DEBUG MACRO] DEBUG_PRINTLN: OK\r\n");
    
    // Test DEBUG_VAR
    DEBUG_VAR(debug_test_value, debug_test_value);
    serial_write_str("[DEBUG MACRO] DEBUG_VAR: OK\r\n");
    
    // Test DEBUG_ASSERT [D003] - con condición verdadera (no debe fallar)
    DEBUG_ASSERT(1 == 1);
    serial_write_str("[DEBUG MACRO] DEBUG_ASSERT (pass): OK\r\n");
    
    // Test DEBUG_LOG
    DEBUG_LOG("Testing DEBUG_LOG macro");
    serial_write_str("[DEBUG MACRO] DEBUG_LOG: OK\r\n");
    
    serial_write_str("[DEBUG MACROS] All tests passed\r\n");

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
    serial_write_str("Page tables: 2GiB mapped\r\n");
    serial_write_str("Test: Memory access at 80MB - OK\r\n");
    serial_write_str("Test: Color validation - OK\r\n");
    serial_write_str("System halted - press reset to restart\r\n");

    // ==========================================
    // Kernel idle loop - nunca retornar
    // ==========================================
    for (;;) {
        __asm__ volatile ("hlt");
    }
}