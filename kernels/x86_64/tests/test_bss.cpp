#include "test_bss.h"
#include "serial.h"
#include "panic.h"
#include "constants.h"

/* ==========================================
 * BSS Initialization Test
 * ==========================================
 * Verifies that BSS variables are zero-initialized by boot code.
 * This is a critical test that validates the boot process.
 * ========================================== */

// Forward declarations for variables from main.cpp
// Naming convention: test_<purpose> for consistency
extern uint32_t test_bss_variable;
extern uint32_t test_data_variable;

void test_bss_initialization() {
    serial_write_str("\r\n=== BSS Initialization Test ===\r\n");

    serial_write_str("BSS variable (should be 0x00000000): 0x");
    serial_write_hex(test_bss_variable);
    serial_write_str("\r\n");

    serial_write_str("DATA variable (should be 0x12345678): 0x");
    serial_write_hex(test_data_variable);
    serial_write_str("\r\n");

    if (test_bss_variable == 0) {
        serial_write_str("[BSS TEST] PASSED: BSS initialized to zero\r\n");
    } else {
        serial_write_str("[BSS TEST] FAILED: BSS not zeroed!\r\n");
        panic("BSS initialization failed", test_bss_variable);
    }
}
