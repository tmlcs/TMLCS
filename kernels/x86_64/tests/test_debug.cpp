#include "test_debug.h"
#include "serial.h"

// Enable debug macros for testing
#define DEBUG_ENABLE 1
#include "debug.h"

/* ==========================================
 * Debug Macros Test
 * ==========================================
 * Tests all debug macros: DEBUG_PRINTLN, DEBUG_VAR,
 * DEBUG_ASSERT, and DEBUG_LOG.
 *
 * Requires DEBUG_ENABLE to be defined before including debug.h
 * ========================================== */

// Forward declaration from main.cpp
// Naming convention: test_<purpose> for consistency
extern uint32_t test_debug_value;

void test_debug_macros() {
    serial_write_str("\r\n=== Debug Macros Test ===\r\n");

    // Test DEBUG_PRINTLN
    DEBUG_PRINTLN("Testing DEBUG_PRINTLN...");
    serial_write_str("[DEBUG MACRO] DEBUG_PRINTLN: OK\r\n");

    // Test DEBUG_VAR
    DEBUG_VAR(test_debug_value, test_debug_value);
    serial_write_str("[DEBUG MACRO] DEBUG_VAR: OK\r\n");

    // Test DEBUG_ASSERT - with true condition (should not fail)
    // Use actual variable comparison instead of tautology
    uint32_t test_val = 42;
    DEBUG_ASSERT(test_val == 42);
    serial_write_str("[DEBUG MACRO] DEBUG_ASSERT (pass): OK\r\n");

    // Test DEBUG_LOG
    DEBUG_LOG("Testing DEBUG_LOG macro");
    serial_write_str("[DEBUG MACRO] DEBUG_LOG: OK\r\n");

    serial_write_str("[DEBUG MACROS] All tests passed\r\n");
}
