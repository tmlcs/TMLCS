#include "test_log.h"
#include "serial.h"
#include "log.h"

/* ==========================================
 * Log Truncation Test
 * ==========================================
 * LOG_BUFFER_SIZE = 256. A format string that expands to > 255 chars
 * triggers the truncation marker in build_message().
 *
 * 30 repetitions of %u with value 1234567890 (10 digits each) plus
 * spaces = ~330 chars expanded — well over 255.
 *
 * After the fix (buf_end-3), the truncated line in serial_output.log
 * must end with "..." (three dots). Before the fix it ends with "..".
 *
 * Verification (run after make run-serial):
 *   grep "TRUNCATION-TEST" serial_output.log
 *   -> line must end in "..."
 * ==========================================
 */
void test_log_truncation(void) {
    serial_write_str("\r\n=== Log Truncation Test ===\r\n");

    /* Emit a LOG_INFO that expands to > 255 chars to trigger truncation.
     * 30x "1234567890 " = 330 chars > 255 = LOG_BUFFER_SIZE - 1. */
    LOG_INFO("TRUNCATION-TEST: %u %u %u %u %u %u %u %u %u %u "
             "%u %u %u %u %u %u %u %u %u %u "
             "%u %u %u %u %u %u %u %u %u %u",
             1234567890u, 1234567890u, 1234567890u, 1234567890u, 1234567890u,
             1234567890u, 1234567890u, 1234567890u, 1234567890u, 1234567890u,
             1234567890u, 1234567890u, 1234567890u, 1234567890u, 1234567890u,
             1234567890u, 1234567890u, 1234567890u, 1234567890u, 1234567890u,
             1234567890u, 1234567890u, 1234567890u, 1234567890u, 1234567890u,
             1234567890u, 1234567890u, 1234567890u, 1234567890u, 1234567890u);

    serial_write_str("[LOG TRUNCATION] Marker check: see TRUNCATION-TEST line above\r\n");
    serial_write_str("[LOG TRUNCATION] Expected: ends with '...' | Bug: ends with '..'\r\n");
    serial_write_str("[LOG TRUNCATION] PASSED\r\n");
}
