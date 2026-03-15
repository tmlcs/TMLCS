#include "constants.h"
#include "panic.h"
#include "print.h"
#include "serial.h"
#include "string.h"
#include "vga.h"

// Enable debug macros for testing
#define DEBUG_ENABLE 1
#include "debug.h"

// Include test function declarations
#include "../../tests/test_bss.h"
#include "../../tests/test_color.h"
#include "../../tests/test_debug.h"
#include "../../tests/test_hardware.h"
#include "../../tests/test_memory.h"
#include "../../tests/test_print.h"
#include "../../tests/test_query.h"
#include "../../tests/test_serial.h"
#include "../../tests/test_serial_signed.h"
#include "../../tests/test_spinlock.h"
#include "../../tests/test_string.h"
#include "../../tests/test_strlcpy.h"

// Centralized version constant
static constexpr const char* OS_VERSION = "GLOBEX_OS v0.015_x64";

// ==========================================
// Global Variables for Tests
// ==========================================
// These variables are shared between main.cpp and test modules.
// They are declared extern in test files and defined here.
// Naming convention: test_<purpose> for consistency
// ==========================================

// BSS test variable (no explicit initializer - MUST be zeroed by boot code)
uint32_t test_bss_variable;

// Data section test variable
uint32_t test_data_variable = 0x12345678;

// High memory test pointer (80MB - within 2GiB mapped region)
volatile uint32_t* test_high_mem_ptr = reinterpret_cast<volatile uint32_t*>(0x05000000);

// Debug macros test variable
uint32_t test_debug_value = 0xCAFEBABE;

// Print functions test variables
uint64_t test_u64_value = 0x123456789ABCDEF0ULL;
int32_t test_signed_pos = 12345;
int32_t test_signed_neg = -9876;
int64_t test_signed64 = -123456789012345LL;

// Query functions test variables
size_t test_cursor_col, test_cursor_row;
uint8_t test_fg, test_bg;

// ==========================================
// Early Panic Function (pre-driver initialization)
// ==========================================
// Displays error directly to VGA buffer without using print_* functions.
// Falls back to CPU halt if VGA is not available.
// Used when both serial and VGA initialization fail.
//
// INTERRUPT-SAFE: Does not acquire any locks.
// Uses vga_put_string_early() for direct MMIO writes.
// ==========================================
static void early_panic(const char* msg) {
    // Try VGA buffer directly (no driver initialization)
    volatile uint16_t* vga = reinterpret_cast<volatile uint16_t*>(VGA_BUFFER_ADDRESS);

    // Test if VGA memory is writable
    uint16_t saved = vga[0];
    vga[0] = (VGA_COLOR_WHITE_ON_RED << 8) | ' ';  // Space with white on red

    if (vga[0] == ((VGA_COLOR_WHITE_ON_RED << 8) | ' ')) {
        // VGA is available - display error
        vga[0] = saved;  // Restore

        // Use vga_put_string_early for interrupt-safe output
        // This function does NOT acquire locks, preventing deadlock
        vga_put_string_early("ERROR: ", vga_make_pos(vga_col(0), vga_row(0)),
                             VGA_COLOR_WHITE_ON_RED);
        vga_put_string_early(msg, vga_make_pos(vga_col(7), vga_row(0)), VGA_COLOR_WHITE_ON_RED);

        // Fill rest of first row with spaces for clarity
        for (size_t pos = 7 + strlen(msg); pos < VGA_COLS; pos++) {
            vga[pos] = (VGA_COLOR_WHITE_ON_RED << 8) | ' ';
        }

        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    // VGA not available - just halt (no output possible)
    // This is the absolute worst case - system is dead silent
    for (;;) {
        __asm__ volatile("hlt");
    }
}

// ==========================================
// Kernel Entry Point
// ==========================================
// The kernel never returns - use noreturn attribute.
// This function initializes hardware, runs tests, and halts.
// ==========================================
extern "C" [[noreturn]] void kernel_main() {
    // ==========================================
    // CRITICAL: BSS Initialization Verification
    // ==========================================
    // This ASSERT runs BEFORE any other initialization to catch
    // boot code bugs early. If BSS is not zeroed, the boot code
    // has a critical bug and we fail immediately.
    //
    // test_bss_variable is a uint32_t without explicit initializer,
    // so it MUST be in .bss section and MUST be zeroed by boot code.
    // ==========================================
    DEBUG_ASSERT(test_bss_variable == 0);

    // ==========================================
    // Initialize Hardware with Robust Fallback
    // ==========================================
    // Priority: Serial → VGA → Early Panic
    //
    // This ensures you always have at least one output method:
    // - Serial: Best for automated testing and remote debugging
    // - VGA: Direct display for local debugging
    // - Early Panic: Last resort direct VGA write
    // ==========================================

    bool serial_ok = serial_init_default();
    bool vga_detected_raw = false;

    // Quick VGA detection test (without full initialization)
    {
        volatile uint16_t* vga = reinterpret_cast<volatile uint16_t*>(VGA_BUFFER_ADDRESS);
        uint16_t saved = vga[0];
        vga[0] = (VGA_COLOR_LIGHT_GREEN_ON_BLACK << 8) | ' ';  // Test write
        vga_detected_raw = (vga[0] == ((VGA_COLOR_LIGHT_GREEN_ON_BLACK << 8) | ' '));
        vga[0] = saved;
    }

    // Case 1: Both serial and VGA failed - use early panic
    if (!serial_ok && !vga_detected_raw) {
        early_panic("No serial or VGA available");
        // Never returns
    }

    // Case 2: Serial failed but VGA works - VGA only mode
    if (!serial_ok && vga_detected_raw) {
        // Initialize VGA and report error
        print_detect();
        print_clear();
        print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
        print_str("[FATAL] Serial initialization failed\r\n");
        print_str("[FATAL] Output will be VGA only\r\n");
        print_str("[FATAL] System may be unstable\r\n");
        print_set_color(PRINT_COLOR_LIGHT_RED, PRINT_COLOR_BLACK);

        // Display error code for diagnostics
        print_str("\r\nSerial error code: 0x");
        print_hex(serial_get_error_code());
        print_str("\r\nTimeout count: ");
        print_dec(serial_get_timeout_count());
        print_str("\r\n");

        // Halt - system cannot continue safely without serial
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    // Case 3: Serial works - continue with normal initialization
    if (serial_ok) {
        serial_write_str("[BOOT] Serial initialized successfully\r\n");
        serial_write_str("[BOOT] Baud rate: 115200\r\n");
        serial_write_str("[BOOT] Port: COM1 (0x3F8)\r\n");

        // Report any initialization warnings
        if (serial_has_failed()) {
            serial_write_str("[WARN] Serial reported non-fatal errors\r\n");
        }
    }

    // Initialize VGA if available
    bool vga_available = print_detect();

    if (!vga_available) {
        // VGA not available - log warning to serial only
        serial_write_str("[WARN] VGA not detected, serial only mode\r\n");
    } else {
        // VGA available - full initialization
        // CRITICAL: Must call print_init() to set vga_initialized = true
        print_init();
        print_clear();
        print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
        serial_write_str("[BOOT] VGA initialized successfully\r\n");
    }

    // Display boot banner on VGA if available
    if (vga_available) {
        print_str("Welcome to ");
        print_str(OS_VERSION);
        print_str("\r\n");
        print_str("64-bit kernel on C++!\r\n");
        print_str("\r\n");
    }

    // ==========================================
    // Run Test Suite
    // ==========================================
    test_bss_initialization();
    test_memory_mapping();
    test_color_validation();
    test_debug_macros();
    test_print_functions();
    test_decimal_boundary_values();  // Buffer overflow tests
    test_query_functions();
    test_serial_baud_rates();
    test_serial_null_pointer_handling();  // Null pointer should not mark hardware failed
    test_string_functions();
    test_string_null_pointer_safety();  // NULL pointer validation in string functions
    test_strlcpy_safe_copy();           // Safe bounded string copy
    test_strlcpy_vs_strcpy_overflow();  // Overflow prevention demo
    test_memcpy_overlap_detection();    // Overlap detection in DEBUG mode
    test_serial_signed_numbers();
    test_spinlock();  // Spinlock tests (initialization, acquire/release, SMP safety)
    test_hardware_info();

    // ==========================================
    // Final Status Display
    // ==========================================
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
    // Summary Output to Serial
    // ==========================================
    serial_write_str("\r\n=== GLOBEX_OS Kernel Started ===\r\n");
    serial_write_str(OS_VERSION);
    serial_write_str("\r\n");
    serial_write_str("Page tables: 2GiB mapped\r\n");
    serial_write_str("Test: BSS initialization - OK\r\n");
    serial_write_str("Test: Memory mapping - OK\r\n");
    serial_write_str("Test: Color validation - OK\r\n");
    serial_write_str("Test: Debug macros - OK\r\n");
    serial_write_str("Test: Print functions (64-bit, signed) - OK\r\n");
    serial_write_str("Test: Query functions (cursor, color) - OK\r\n");
    serial_write_str("Test: Serial baud rates - OK\r\n");
    serial_write_str("Test: String functions - OK\r\n");
    serial_write_str("Test: String NULL safety - OK\r\n");
    serial_write_str("Test: strlcpy safe copy - OK\r\n");
    serial_write_str("Test: Serial signed numbers - OK\r\n");
    serial_write_str("Test: Spinlock (init, acquire/release, SMP) - OK\r\n");
    serial_write_str("Test: Hardware info (CPUID) - OK\r\n");
    serial_write_str("System halted - press reset to restart\r\n");

    // ==========================================
    // Kernel idle loop - never return
    // ==========================================
    for (;;) {
        __asm__ volatile("hlt");
    }
}
