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

#if LOG_MAX_LEVEL <= LOG_LEVEL_INFO
    /* Verify truncation using log_format_to_buf: cap=16.
     * The "..." marker fires when the *format string* is not fully consumed.
     * Use a long literal format string (no %s) so the buffer fills with literal
     * chars and the loop exits mid-format, leaving *fmt != '\0'. */
    char buf[16];
    size_t written = log_format_to_buf(buf, sizeof(buf),
                                       "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

    /* written should be 15 (cap-1), last three chars should be "..." */
    bool truncated = (written == 15)
                  && (buf[12] == '.')
                  && (buf[13] == '.')
                  && (buf[14] == '.');

    if (truncated) {
        serial_write_str("  [PASS] log_format_to_buf truncation verified (ends with ...)\r\n");
    } else {
        serial_write_str("  [FAIL] log_format_to_buf did not truncate correctly\r\n");
    }

    /* Verify non-truncated case */
    char buf2[64];
    size_t written2 = log_format_to_buf(buf2, sizeof(buf2), "hello %s", "world");
    bool no_trunc = (written2 == 11) && (buf2[written2] == '\0');
    if (no_trunc) {
        serial_write_str("  [PASS] log_format_to_buf non-truncated case correct\r\n");
    } else {
        serial_write_str("  [FAIL] log_format_to_buf non-truncated case wrong\r\n");
    }

    if (truncated && no_trunc) {
        serial_write_str("[LOG TRUNCATION] PASSED\r\n");
    } else {
        serial_write_str("[LOG TRUNCATION] FAILED\r\n");
    }
#else
    serial_write_str("[LOG TRUNCATION] SKIPPED at this LOG_LEVEL\r\n");
#endif
}
