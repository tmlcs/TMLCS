#include "test_query.h"
#include "serial.h"
#include "print.h"

/* ==========================================
 * Query Functions Test
 * ==========================================
 * Tests cursor and color query/set functions.
 * Verifies print_get_cursor, print_set_cursor, print_get_color,
 * boundary clamping, and print_is_initialized.
 * ========================================== */

// Forward declarations from main.cpp
extern size_t test_cursor_col;
extern size_t test_cursor_row;
extern uint8_t test_fg;
extern uint8_t test_bg;

void test_query_functions() {
    serial_write_str("\r\n=== Query Functions Test ===\r\n");

    // Test print_get_cursor() - get current cursor position
    print_get_cursor(&test_cursor_col, &test_cursor_row);
    serial_write_str("Current cursor: col=");
    serial_write_dec((uint32_t)test_cursor_col);
    serial_write_str(", row=");
    serial_write_dec((uint32_t)test_cursor_row);
    serial_write_str("\r\n");
    serial_write_str("print_get_cursor: OK\r\n");

    // Test print_set_cursor() - set cursor to specific position
    serial_write_str("Setting cursor to (40, 12)...\r\n");
    print_set_cursor(40, 12);

    // Verify new position
    print_get_cursor(&test_cursor_col, &test_cursor_row);
    if (test_cursor_col == 40 && test_cursor_row == 12) {
        serial_write_str("Cursor set successfully: col=");
        serial_write_dec((uint32_t)test_cursor_col);
        serial_write_str(", row=");
        serial_write_dec((uint32_t)test_cursor_row);
        serial_write_str("\r\n");
        serial_write_str("print_set_cursor: OK\r\n");
    } else {
        serial_write_str("print_set_cursor: FAILED\r\n");
    }

    // Test print_set_cursor() boundary clamping
    // Test 1: Column > 79 should clamp to 79
    print_set_cursor(100, 10);
    print_get_cursor(&test_cursor_col, &test_cursor_row);
    if (test_cursor_col == 79 && test_cursor_row == 10) {
        serial_write_str("print_set_cursor clamp col: OK\r\n");
    } else {
        serial_write_str("print_set_cursor clamp col: FAILED\r\n");
    }

    // Test 2: Row > 24 should clamp to 24 (column also clamped from 100 to 79)
    print_set_cursor(100, 50);
    print_get_cursor(&test_cursor_col, &test_cursor_row);
    if (test_cursor_col == 79 && test_cursor_row == 24) {
        serial_write_str("print_set_cursor clamp row: OK\r\n");
    } else {
        serial_write_str("print_set_cursor clamp row: FAILED\r\n");
    }

    // Test 3: Both at max boundary (79, 24)
    print_set_cursor(79, 24);
    print_get_cursor(&test_cursor_col, &test_cursor_row);
    if (test_cursor_col == 79 && test_cursor_row == 24) {
        serial_write_str("print_set_cursor max boundary: OK\r\n");
    } else {
        serial_write_str("print_set_cursor max boundary: FAILED\r\n");
    }

    // Test 4: Zero position (0, 0)
    print_set_cursor(0, 0);
    print_get_cursor(&test_cursor_col, &test_cursor_row);
    if (test_cursor_col == 0 && test_cursor_row == 0) {
        serial_write_str("print_set_cursor zero: OK\r\n");
    } else {
        serial_write_str("print_set_cursor zero: FAILED\r\n");
    }

    // Test print_get_color() - get current color
    // Returns combined color byte: (bg << 4) | fg
    uint8_t color = print_get_color();
    test_fg = color & 0x0F;  // Lower 4 bits = foreground
    test_bg = (color >> 4) & 0x0F;  // Upper 4 bits = background
    serial_write_str("Current color: fg=");
    serial_write_dec((uint32_t)test_fg);
    serial_write_str(", bg=");
    serial_write_dec((uint32_t)test_bg);
    serial_write_str("\r\n");
    serial_write_str("print_get_color: OK\r\n");

    // Test print_is_initialized() [HIGH-007]
    if (print_is_initialized()) {
        serial_write_str("print_is_initialized: OK (VGA ready)\r\n");
    } else {
        serial_write_str("print_is_initialized: VGA not initialized\r\n");
    }

    // Restore cursor to normal position
    print_set_cursor(0, 13);

    serial_write_str("[QUERY FUNCTIONS] All tests passed\r\n");
}
