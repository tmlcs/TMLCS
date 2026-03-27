#include "test_log.h"
#include "serial.h"
#include "log.h"

/* ==========================================
 * Log Truncation Test
 * ==========================================
 * Uses log_format_to_buf() to programmatically verify that build_message()
 * appends "..." when the format string is not fully consumed.
 *
 * A 16-byte buffer with a 36-character literal format string triggers
 * truncation: 15 chars written, last 3 replaced with "...".
 *
 * Both truncated and non-truncated cases are verified. PASSED is only
 * printed if both programmatic assertions hold.
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
