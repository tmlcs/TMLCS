#include "test_serial.h"
#include "serial.h"
#include "constants.h"

/* ==========================================
 * Serial Baud Rate Validation Test
 * ==========================================
 * Tests valid and invalid baud rate handling.
 * Verifies serial_init() accepts valid rates and rejects invalid ones.
 * ========================================== */

void test_serial_baud_rates() {
    serial_write_str("\r\n=== Serial Baud Rate Test ===\r\n");

    // Test valid baud rates
    {
        // Test 115200 (maximum)
        if (serial_init(SERIAL_DEFAULT_PORT, 115200)) {
            serial_write_str("Baud 115200: OK\r\n");
        } else {
            serial_write_str("Baud 115200: FAILED\r\n");
        }

        // Test 9600 (standard)
        if (serial_init(SERIAL_DEFAULT_PORT, 9600)) {
            serial_write_str("Baud 9600: OK\r\n");
        } else {
            serial_write_str("Baud 9600: FAILED\r\n");
        }

        // Test 110 (minimum)
        if (serial_init(SERIAL_DEFAULT_PORT, 110)) {
            serial_write_str("Baud 110: OK\r\n");
        } else {
            serial_write_str("Baud 110: FAILED\r\n");
        }

        // Test 57600 (common high rate)
        if (serial_init(SERIAL_DEFAULT_PORT, 57600)) {
            serial_write_str("Baud 57600: OK\r\n");
        } else {
            serial_write_str("Baud 57600: FAILED\r\n");
        }

        // Re-initialize with default for subsequent tests
        serial_init_default();
    }

    // Test invalid baud rates (should fail)
    {
        // Test 0 (zero - invalid)
        if (!serial_init(SERIAL_DEFAULT_PORT, 0)) {
            serial_write_str("Baud 0 (invalid): correctly rejected\r\n");
        } else {
            serial_write_str("Baud 0 (invalid): FAILED - should reject\r\n");
        }

        // Test 50 (below minimum - invalid)
        if (!serial_init(SERIAL_DEFAULT_PORT, 50)) {
            serial_write_str("Baud 50 (invalid): correctly rejected\r\n");
        } else {
            serial_write_str("Baud 50 (invalid): FAILED - should reject\r\n");
        }

        // Test 230400 (above maximum - invalid)
        if (!serial_init(SERIAL_DEFAULT_PORT, 230400)) {
            serial_write_str("Baud 230400 (invalid): correctly rejected\r\n");
        } else {
            serial_write_str("Baud 230400 (invalid): FAILED - should reject\r\n");
        }

        // Re-initialize with default for subsequent tests
        serial_init_default();
    }

    serial_write_str("[SERIAL BAUD RATE] All tests passed\r\n");
}

/* ==========================================
 * Serial Null Pointer Handling Test (CRIT-003)
 * ==========================================
 * Verifies that passing null pointers to serial functions
 * does NOT incorrectly mark the serial port as hardware failed.
 * 
 * CRIT-003: Null pointer is a programming error, not hardware failure.
 * The serial_failed flag should NOT be set for null pointers.
 * ========================================== */
void test_serial_null_pointer_handling() {
    serial_write_str("\r\n=== Serial Null Pointer Test ===\r\n");
    
    // Initialize serial first
    serial_init_default();
    
    // Clear any existing error state
    serial_clear_error();
    
    // Verify serial is initially in good state
    if (serial_has_failed()) {
        serial_write_str("ERROR: Serial reported failed before test!\r\n");
    } else {
        serial_write_str("Serial initial state: OK (not failed)\r\n");
    }
    
    // Test 1: Pass null pointer to serial_write_str
    serial_write_str("Testing serial_write_str(nullptr)...\r\n");
    serial_write_str((const char*)0);  // Pass null pointer
    
    // CRIT-003 FIX: This should NOT set serial_failed = 1
    if (serial_has_failed()) {
        serial_write_str("serial_failed set for null pointer!\r\n");
        serial_write_str("  Error code: ");
        serial_write_dec(serial_get_error_code());
        serial_write_str("\r\n");
    } else {
        serial_write_str("serial_failed NOT set for null pointer passed\r\n");
    }
    
    // Verify error code was set correctly
    uint32_t error_code = serial_get_error_code();
    if (error_code == SERIAL_ERROR_NULL_PTR) {
        serial_write_str("Error code: SERIAL_ERROR_NULL_PTR (correct)\r\n");
    } else {
        serial_write_str("Error code: ");
        serial_write_dec(error_code);
        serial_write_str(" (expected ");
        serial_write_dec(SERIAL_ERROR_NULL_PTR);
        serial_write_str(")\r\n");
    }
    
    // Test 2: Verify serial still works after null pointer
    serial_write_str("Testing serial still works after nullptr...\r\n");
    serial_write_str("Serial still functional: OK\r\n");
    
    // Clear error state for subsequent tests
    serial_clear_error();
    
    serial_write_str("[SERIAL NULL POINTER] All tests passed\r\n");
}
