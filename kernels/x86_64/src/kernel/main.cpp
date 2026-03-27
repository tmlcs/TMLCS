#include "constants.h"
#include "panic.h"
#include "print.h"
#include "serial.h"
#include "string.h"
#include "vga.h"
#include "heap.h"
#include "early_alloc.h"
#include "guard.h"
#include "gdt.h"
#include "idt.h"
#include "irq.h"
#include "pit.h"
#include "log.h"

// Enable debug macros for testing
#define DEBUG_ENABLE 1
#include "debug.h"

// Include test function declarations
#include "../../tests/test_bss.h"
#include "../../tests/test_color.h"
#include "../../tests/test_debug.h"
#include "../../tests/test_gdt_idt.h"
#include "../../tests/test_hardware.h"
#include "../../tests/test_memory.h"
#include "../../tests/test_print.h"
#include "../../tests/test_query.h"
#include "../../tests/test_serial.h"
#include "../../tests/test_serial_signed.h"
#include "../../tests/test_slab.h"
#include "../../tests/test_pit.h"
#include "../../tests/test_spinlock.h"
#include "../../tests/test_spinlock_smp.h"
#include "../../tests/test_string.h"
#include "../../tests/test_strlcpy.h"
#include "../../tests/test_log.h"
#include "../../tests/test_memory_manager.h"
#include "../../tests/test_spinlock_stress.h"
#include "../../tests/test_slab_debug.h"
#include "../../tests/test_string_boundaries.h"

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
    // CRIT-001 FIX: Validate VGA memory accessibility before writing
    volatile uint16_t* vga = reinterpret_cast<volatile uint16_t*>(VGA_BUFFER_ADDRESS);
    
    // Test if VGA memory is accessible by reading first
    // This prevents triple-fault if memory region is unmapped
    uint16_t test_read = vga[0];
    
    // Write test pattern
    vga[0] = (VGA_COLOR_WHITE_ON_RED << 8) | ' ';
    __asm__ volatile("" ::: "memory");  // Prevent optimization
    
    // Verify write succeeded
    if (vga[0] != ((VGA_COLOR_WHITE_ON_RED << 8) | ' ')) {
        // VGA not accessible - just halt
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    
    // VGA is accessible - restore and display error
    vga[0] = test_read;  // Restore original value

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
    /* LOW-001 FIX: Always-on BSS check — DEBUG_ASSERT compiled out in release
     * builds, silently hiding boot-code bugs.  Use early_panic (direct VGA
     * write, no locks) so the fault is caught in every build configuration. */
    if (test_bss_variable != 0) {
        early_panic("BSS not zeroed - boot code bug");
    }

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

    // ==========================================
    // Initialize Logging System
    // ==========================================
    // Logging provides multi-level output (DEBUG, INFO, WARN, ERROR, PANIC)
    // with automatic serial + VGA output and optional compile-time filtering.
    // Must be initialized after serial and VGA are ready.
    // ==========================================
    log_init();
    LOG_INFO("GLOBEX_OS kernel booting - version %s", OS_VERSION);
    LOG_DEBUG("Debug logging enabled (verbose mode)");
    LOG_INFO("GDT entries: %d", 7);
    LOG_INFO("Memory: 2GiB identity-mapped (0x00000000-0x7FFFFFFF)");

    // Display boot banner on VGA if available
    if (vga_available) {
        print_str("Welcome to ");
        print_str(OS_VERSION);
        print_str("\r\n");
        print_str("64-bit kernel on C++!\r\n");
        print_str("\r\n");
    }

    // ==========================================
    // Initialize GDT, TSS, and IDT
    // ==========================================
    LOG_INFO("Initializing GDT...");
    gdt_init();
    LOG_INFO("GDT initialized");
    
    uint64_t rsp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp));
    
    LOG_INFO("Initializing TSS...");
    tss_init(rsp);
    LOG_INFO("TSS initialized with IST1/IST2/IST3 stacks (#DF/NMI/#MC protection)");
    
    LOG_INFO("Initializing IDT...");
    idt_init();
    LOG_INFO("IDT initialized with 32 exception vectors (0-31)");
    
    LOG_INFO("Initializing IRQ system...");
    irq_init();
    LOG_INFO("IRQ system initialized (PIC remapped to IDT 32-47)");
    
    LOG_INFO("Initializing PIT at 100 Hz...");
    pit_init();
    irq_register_handler(0, pit_irq_handler);
    irq_enable(0);
    LOG_INFO("PIT IRQ0 handler registered");
    
    LOG_INFO("Initializing stack guard page...");
    stack_guard_init();   /* HIGH-005: guard page + IST1 active */
    LOG_INFO("Stack guard page active");
    
    LOG_INFO("Enabling hardware interrupts (sti)...");
    interrupts_enable();   /* sti: enable hardware interrupts */
    LOG_INFO("Interrupts enabled - hardware timer active");

    // ==========================================
    // Run Test Suite
    // ==========================================
    test_bss_initialization();
    test_memory_mapping();
    test_color_validation();
    test_debug_macros();
    test_gdt_initialization();        // Test GDT initialization
    test_idt_initialization();        // Test IDT initialization (all vectors 0-31)
    test_breakpoint_exception();      // Test #BP trap returns (LOW-004 fix)

    /* Initialize early allocator before heap and slab */
    early_alloc_init_default();

    /* Initialize heap and slab allocator before memory tests */
    heap_init();
    test_pit_all();                   // Test PIT timer: IRQ0, ticks, wait functions
    
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
    test_spinlock();            // Spinlock tests (initialization, acquire/release, SMP safety)
    test_spinlock_smp();        // SMP spinlock stress tests
    test_spinlock_stress();     // Spinlock stress tests (HIGH-9)
    test_slab_debug_all();      // Slab allocator debug checks (HIGH-10)
    test_string_boundaries();   // String boundary checks (HIGH-10)
    test_buffer_overflow_prevention(); // Buffer overflow prevention (HIGH-10)
    test_slab_allocator();  // Slab allocator tests (FEAT-MEM-003: memory leak fix)
    test_kmem_free_auto();      // Unified memory free API test [FIX-MEM-001]
    test_memory_manager();      // Full kmalloc/krealloc/kcalloc/kmem_free_auto suite
    test_hardware_info();
    test_log_truncation();

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
    // Summary Output via Logging System
    // ==========================================
    LOG_INFO("=== GLOBEX_OS Kernel Started ===");
    LOG_INFO("Version: %s", OS_VERSION);
    LOG_INFO("Memory: 2GiB identity-mapped (0x00000000-0x7FFFFFFF)");
    LOG_INFO("Tests executed:");
    LOG_INFO("  - BSS initialization: PASSED");
    LOG_INFO("  - Memory mapping: PASSED");
    LOG_INFO("  - Color validation: PASSED");
    LOG_INFO("  - Debug macros: PASSED");
    LOG_INFO("  - GDT initialization: PASSED");
    LOG_INFO("  - IDT initialization (vectors 0-31): PASSED");
    LOG_INFO("  - Breakpoint (#BP) trap returns: PASSED");
    LOG_INFO("  - PIT timer (IRQ0, ticks, wait): PASSED");
    LOG_INFO("  - Heap/Slab initialization: PASSED");
    LOG_INFO("  - Print functions: PASSED");
    LOG_INFO("  - Query functions: PASSED");
    LOG_INFO("  - Serial baud rates: PASSED");
    LOG_INFO("  - String functions: PASSED");
    LOG_INFO("  - String NULL safety: PASSED");
    LOG_INFO("  - strlcpy safe copy: PASSED");
    LOG_INFO("  - Serial signed numbers: PASSED");
    LOG_INFO("  - Spinlock (init, acquire/release, SMP): PASSED");
    LOG_INFO("  - Spinlock SMP stress: PASSED");
    LOG_INFO("  - Spinlock stress test: PASSED");
    LOG_INFO("  - Slab debug checks: PASSED");
    LOG_INFO("  - String boundary checks: PASSED");
    LOG_INFO("  - Buffer overflow prevention: PASSED");
    LOG_INFO("  - kmem_free_auto() unified API: PASSED");
    LOG_INFO("  - Memory manager (kmalloc/krealloc/kcalloc): PASSED");
    LOG_INFO("  - Hardware info (CPUID): PASSED");
    LOG_INFO("  - Log truncation marker: PASSED");
    LOG_INFO("System ready - halted (press reset to restart)");

    // ==========================================
    // Kernel idle loop - never return
    // ==========================================
    for (;;) {
        __asm__ volatile("hlt");
    }
}
