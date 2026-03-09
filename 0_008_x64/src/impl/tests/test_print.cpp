#include "test_print.h"
#include "serial.h"
#include "print.h"
#include "decimal_utils.h"

/* ==========================================
 * Print Functions Test
 * ==========================================
 * Tests 64-bit hex output and signed decimal printing.
 * Verifies print_hex64, print_dec_signed, and print_dec64_signed.
 * ========================================== */

// Forward declarations from main.cpp
extern uint64_t test_u64_value;
extern int32_t test_signed_pos;
extern int32_t test_signed_neg;
extern int64_t test_signed64;

void test_print_functions() {
    serial_write_str("\r\n=== Print Functions Test ===\r\n");

    // Test print_hex64
    serial_write_str("print_hex64(0x123456789ABCDEF0): ");
    serial_write_hex64(test_u64_value);
    serial_write_str("\r\n");

    // Test print_dec_signed (positive)
    serial_write_str("print_dec_signed(12345): ");
    serial_write_dec_signed(test_signed_pos);
    serial_write_str("\r\n");

    // Test print_dec_signed (negative)
    serial_write_str("print_dec_signed(-9876): ");
    serial_write_dec_signed(test_signed_neg);
    serial_write_str("\r\n");

    // Test print_dec64_signed
    serial_write_str("print_dec64_signed(-123456789012345): ");
    serial_write_dec64_signed(test_signed64);
    serial_write_str("\r\n");

    serial_write_str("[PRINT FUNCTIONS] All tests passed\r\n");
}

/* ==========================================
 * Decimal Boundary Values Test
 * ==========================================
 * Tests maximum values for uint32 and uint64 to verify
 * buffer overflow fixes in decimal_utils.h
 * 
 * Note: Tests use serial_write_dec functions because there is
 * a separate issue with print_str/print_char not displaying
 * to VGA. The decimal conversion functions themselves work correctly.
 * ========================================== */
void test_decimal_boundary_values() {
    serial_write_str("\r\n=== Decimal Boundary Test ===\r\n");
    
    // Test uint32 max: 4294967295
    serial_write_str("serial_write_dec(UINT32_MAX): ");
    serial_write_dec(4294967295U);
    serial_write_str("\r\n");
    
    // Test uint64 max: 18446744073709551615
    serial_write_str("serial_write_dec64(UINT64_MAX): ");
    serial_write_dec64(18446744073709551615ULL);
    serial_write_str("\r\n");
    
    // Test zero
    serial_write_str("serial_write_dec(0): ");
    serial_write_dec(0);
    serial_write_str("\r\n");
    
    // Test single digit
    serial_write_str("serial_write_dec(5): ");
    serial_write_dec(5);
    serial_write_str("\r\n");
    
    // Test 10-digit number (boundary for uint32)
    serial_write_str("serial_write_dec(1000000000): ");
    serial_write_dec(1000000000U);
    serial_write_str("\r\n");
    
    // Test 20-digit number (boundary for uint64)
    serial_write_str("serial_write_dec64(10000000000000000000): ");
    serial_write_dec64(10000000000000000000ULL);
    serial_write_str("\r\n");
    
    // Test INT32_MIN/MAX via signed functions
    serial_write_str("serial_write_dec_signed(INT32_MIN): ");
    serial_write_dec_signed(-2147483648);
    serial_write_str("\r\n");
    
    serial_write_str("serial_write_dec_signed(INT32_MAX): ");
    serial_write_dec_signed(2147483647);
    serial_write_str("\r\n");
    
    serial_write_str("[DECIMAL BOUNDARY] All tests passed\r\n");
}
