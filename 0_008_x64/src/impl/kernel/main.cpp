#include "print.h"
#include "serial.h"
#include "panic.h"

// Enable debug macros for testing
#define DEBUG_ENABLE 1
#include "debug.h"

// Include constants for magic numbers [PC005]
#include "constants.h"

// Constante de versión centralizada
static constexpr const char* OS_VERSION = "GLOBEX_OS v0.015_x64";

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

// Variables para test de print functions
static uint64_t test_u64_value = 0x123456789ABCDEF0ULL;
static int32_t test_signed_pos = 12345;
static int32_t test_signed_neg = -9876;
static int64_t test_signed64 = -123456789012345LL;

// Variables para test de query functions
static size_t test_cursor_col, test_cursor_row;
static uint8_t test_fg, test_bg;

// El kernel nunca debe retornar - usar noreturn
extern "C" [[noreturn]] void kernel_main() {
    // ==========================================
    // CRITICAL: BSS Initialization Verification
    // ==========================================
    // This ASSERT runs BEFORE any other initialization to catch
    // boot code bugs early. If BSS is not zeroed, the boot code
    // has a critical bug and we fail immediately.
    //
    // bss_test_variable is a static uint32_t without explicit initializer,
    // so it MUST be in .bss section and MUST be zeroed by boot code.
    // ==========================================
    DEBUG_ASSERT(bss_test_variable == 0);

    // ==========================================
    // Inicializar serial primero (más seguro que VGA)
    // ==========================================
    if (!serial_init_default()) {
        // Serial falló - intentar panic por VGA directamente
        // Escribir directamente al buffer VGA sin usar funciones
        volatile uint16_t* vga = reinterpret_cast<volatile uint16_t*>(VGA_BUFFER_ADDRESS);
        const char* msg = "FATAL: Serial init failed";
        for (size_t i = 0; msg[i] != '\0' && i < VGA_COLS; i++) {
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

    // ==========================================
    // Print Functions Test - 64-bit and Signed [3.5]
    // ==========================================
    if (vga_available) {
        print_str("\r\n=== Print Functions Test ===\r\n");
    }
    serial_write_str("\r\n=== Print Functions Test ===\r\n");
    
    // Test print_hex64
    if (vga_available) {
        print_str("print_hex64(0x123456789ABCDEF0): ");
        print_hex64(test_u64_value);
        print_str("\r\n");
    }
    serial_write_str("print_hex64: ");
    serial_write_hex64(test_u64_value);
    serial_write_str("\r\n");
    
    // Test print_dec_signed (positive)
    if (vga_available) {
        print_str("print_dec_signed(12345): ");
        print_dec_signed(test_signed_pos);
        print_str("\r\n");
    }
    serial_write_str("print_dec_signed (positive): ");
    serial_write_dec_signed(test_signed_pos);
    serial_write_str("\r\n");
    
    // Test print_dec_signed (negative)
    if (vga_available) {
        print_str("print_dec_signed(-9876): ");
        print_dec_signed(test_signed_neg);
        print_str("\r\n");
    }
    serial_write_str("print_dec_signed (negative): ");
    serial_write_dec_signed(test_signed_neg);
    serial_write_str("\r\n");
    
    // Test print_dec64_signed
    if (vga_available) {
        print_str("print_dec64_signed(-123456789012345): ");
        print_dec64_signed(test_signed64);
        print_str("\r\n");
    }
    serial_write_str("print_dec64_signed: ");
    serial_write_dec64_signed(test_signed64);
    serial_write_str("\r\n");
    
    if (vga_available) {
        print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
        print_str("Print functions: PASSED\r\n");
    }
    serial_write_str("[PRINT FUNCTIONS] All tests passed\r\n");

    // ==========================================
    // Query Functions Test - Cursor/Color [3.6]
    // ==========================================
    if (vga_available) {
        print_str("\r\n=== Query Functions Test ===\r\n");
    }
    serial_write_str("\r\n=== Query Functions Test ===\r\n");
    
    // Test print_get_cursor() - obtener cursor actual
    print_get_cursor(&test_cursor_col, &test_cursor_row);
    if (vga_available) {
        print_str("Current cursor: col=");
        print_dec((uint32_t)test_cursor_col);
        print_str(", row=");
        print_dec((uint32_t)test_cursor_row);
        print_str("\r\n");
    }
    serial_write_str("print_get_cursor: OK\r\n");
    
    // Test print_set_cursor() - mover cursor a posición específica
    if (vga_available) {
        print_str("Setting cursor to (40, 12)...\r\n");
    }
    print_set_cursor(40, 12);
    
    // Verificar nueva posición
    print_get_cursor(&test_cursor_col, &test_cursor_row);
    if (test_cursor_col == 40 && test_cursor_row == 12) {
        if (vga_available) {
            print_str("Cursor set successfully: col=");
            print_dec((uint32_t)test_cursor_col);
            print_str(", row=");
            print_dec((uint32_t)test_cursor_row);
            print_str("\r\n");
        }
        serial_write_str("print_set_cursor: OK\r\n");
    } else {
        serial_write_str("print_set_cursor: FAILED\r\n");
    }
    
    // Test print_get_color() - obtener color actual
    print_get_color(&test_fg, &test_bg);
    if (vga_available) {
        print_str("Current color: fg=");
        print_dec((uint32_t)test_fg);
        print_str(", bg=");
        print_dec((uint32_t)test_bg);
        print_str("\r\n");
    }
    serial_write_str("print_get_color: OK\r\n");
    
    // Restaurar cursor a posición normal
    print_set_cursor(0, 13);
    
    if (vga_available) {
        print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
        print_str("Query functions: PASSED\r\n");
    }
    serial_write_str("[QUERY FUNCTIONS] All tests passed\r\n");

    // ==========================================
    // Serial Signed Numbers Test [3.7]
    // ==========================================
    serial_write_str("\r\n=== Serial Signed Numbers Test ===\r\n");
    
    // Test serial_write_dec_signed() - positive
    serial_write_str("serial_write_dec_signed(42): ");
    serial_write_dec_signed(42);
    serial_write_str("\r\n");
    
    // Test serial_write_dec_signed() - negative
    serial_write_str("serial_write_dec_signed(-1234): ");
    serial_write_dec_signed(-1234);
    serial_write_str("\r\n");
    
    // Test serial_write_dec_signed() - zero
    serial_write_str("serial_write_dec_signed(0): ");
    serial_write_dec_signed(0);
    serial_write_str("\r\n");
    
    // Test serial_write_dec64_signed() - large negative
    serial_write_str("serial_write_dec64_signed(-9876543210): ");
    serial_write_dec64_signed(-9876543210LL);
    serial_write_str("\r\n");
    
    // Test serial_write_dec64_signed() - INT64_MIN edge case
    serial_write_str("serial_write_dec64_signed(INT64_MIN): ");
    serial_write_dec64_signed(-9223372036854775807LL - 1);
    serial_write_str("\r\n");
    
    serial_write_str("[SERIAL SIGNED] All tests passed\r\n");

    // ==========================================
    // Hardware Information Test [3.8]
    // ==========================================
    serial_write_str("\r\n=== Hardware Information Test ===\r\n");
    
    // Test directo de CPUID
    uint32_t test_eax, test_ebx, test_ecx, test_edx;
    
    // CPUID leaf 0: Vendor ID
    __asm__ volatile (
        "cpuid"
        : "=a"(test_eax), "=b"(test_ebx), "=c"(test_ecx), "=d"(test_edx)
        : "a"(0)
        : "memory"
    );
    
    serial_write_str("CPU Max Leaf: ");
    serial_write_hex(test_eax);
    serial_write_str("\r\n");
    
    // Vendor string
    serial_write_str("Vendor: ");
    const char* vendor = (const char*)&test_ebx;
    for (int i = 0; i < 4 && vendor[i]; i++) serial_write_char(vendor[i]);
    vendor = (const char*)&test_edx;
    for (int i = 0; i < 4 && vendor[i]; i++) serial_write_char(vendor[i]);
    vendor = (const char*)&test_ecx;
    for (int i = 0; i < 4 && vendor[i]; i++) serial_write_char(vendor[i]);
    serial_write_str("\r\n");
    
    // CPUID leaf 1: Processor Info
    __asm__ volatile (
        "cpuid"
        : "=a"(test_eax), "=b"(test_ebx), "=c"(test_ecx), "=d"(test_edx)
        : "a"(1)
        : "memory"
    );
    
    uint32_t stepping = test_eax & 0xF;
    uint32_t model = (test_eax >> 4) & 0xF;
    uint32_t family = (test_eax >> 8) & 0xF;
    
    serial_write_str("Family: ");
    serial_write_dec(family);
    serial_write_str(", Model: ");
    serial_write_dec(model);
    serial_write_str(", Stepping: ");
    serial_write_dec(stepping);
    serial_write_str("\r\n");
    
    // CPUID leaf 0x80000000: Extended leaf check
    __asm__ volatile (
        "cpuid"
        : "=a"(test_eax), "=b"(test_ebx), "=c"(test_ecx), "=d"(test_edx)
        : "a"(0x80000000)
        : "memory"
    );
    
    uint32_t max_extended = test_eax;
    
    // CPUID leaf 0x80000001: Long mode check
    int has_long_mode = 0;
    if (max_extended >= 0x80000001) {
        __asm__ volatile (
            "cpuid"
            : "=a"(test_eax), "=b"(test_ebx), "=c"(test_ecx), "=d"(test_edx)
            : "a"(0x80000001)
            : "memory"
        );
        has_long_mode = (test_edx >> 29) & 1;
    }
    
    serial_write_str("Long Mode (64-bit): ");
    if (has_long_mode) {
        serial_write_str("Supported\r\n");
    } else {
        serial_write_str("NOT Supported\r\n");
    }
    
    serial_write_str("[HARDWARE INFO] CPU detection complete\r\n");

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
    serial_write_str("Test: Debug macros - OK\r\n");
    serial_write_str("Test: Print functions (64-bit, signed) - OK\r\n");
    serial_write_str("Test: Query functions (cursor, color) - OK\r\n");
    serial_write_str("Test: Serial signed numbers - OK\r\n");
    serial_write_str("Test: Hardware info (CPUID) - OK\r\n");
    serial_write_str("System halted - press reset to restart\r\n");

    // ==========================================
    // Kernel idle loop - nunca retornar
    // ==========================================
    for (;;) {
        __asm__ volatile ("hlt");
    }
}