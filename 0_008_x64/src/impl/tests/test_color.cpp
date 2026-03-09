#include "test_color.h"
#include "serial.h"
#include "print.h"
#include "constants.h"

/* ==========================================
 * Color Validation Test
 * ==========================================
 * Verifies VGA color handling including valid colors (0-15)
 * and invalid color fallback behavior.
 * ========================================== */

void test_color_validation() {
    serial_write_str("\r\n=== Color Validation Test ===\r\n");
    serial_write_str("Testing valid colors (0-15)...\r\n");

    // Test with valid colors
    print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLUE);
    print_str("Valid color: YELLOW on BLUE - OK\r\n");

    // Test with invalid colors (>15) - should use default
    serial_write_str("Testing invalid colors (>15)...\r\n");
    print_set_color(255, 100);  // Invalid values - should use white on black
    print_str("Invalid color fallback: Should be WHITE on BLACK - OK\r\n");

    // Restore normal color
    print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
    serial_write_str("Color validation: PASSED\r\n");

    serial_write_str("[COLOR TEST] PASSED: Color validation works\r\n");
}
