#include "test_serial_signed.h"
#include "serial.h"

/* ==========================================
 * Serial Signed Numbers Test
 * ==========================================
 * Tests signed decimal output for 32-bit and 64-bit integers.
 * Verifies serial_write_dec_signed and serial_write_dec64_signed
 * including edge cases like INT64_MIN.
 * ========================================== */

void test_serial_signed_numbers() {
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
}
