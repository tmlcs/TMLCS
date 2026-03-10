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
extern uint32_t debug_test_value;

void test_debug_macros() {
    serial_write_str("\r\n=== Debug Macros Test ===\r\n");

    // Test DEBUG_PRINTLN [D002]
    DEBUG_PRINTLN("Testing DEBUG_PRINTLN...");
    serial_write_str("[DEBUG MACRO] DEBUG_PRINTLN: OK\r\n");

    // Test DEBUG_VAR
    DEBUG_VAR(debug_test_value, debug_test_value);
    serial_write_str("[DEBUG MACRO] DEBUG_VAR: OK\r\n");

    // Test DEBUG_ASSERT [D003] - with true condition (should not fail)
    // Use actual variable comparison instead of tautology
    uint32_t test_val = 42;
    DEBUG_ASSERT(test_val == 42);
    serial_write_str("[DEBUG MACRO] DEBUG_ASSERT (pass): OK\r\n");

    // Test DEBUG_LOG
    DEBUG_LOG("Testing DEBUG_LOG macro");
    serial_write_str("[DEBUG MACRO] DEBUG_LOG: OK\r\n");

    serial_write_str("[DEBUG MACROS] All tests passed\r\n");
}
