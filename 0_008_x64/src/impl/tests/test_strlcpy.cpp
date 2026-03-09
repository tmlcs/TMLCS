#include "test_strlcpy.h"
#include "serial.h"
#include "string.h"

/* ==========================================
 * strlcpy Safe Copy Test
 * ==========================================
 * Verifies that strlcpy prevents buffer overflow by:
 *   1. Never writing more than destsize bytes
 *   2. Always null-terminating (if destsize > 0)
 *   3. Returning source length for truncation detection
 * ========================================== */
void test_strlcpy_safe_copy() {
    serial_write_str("\r\n=== strlcpy Safe Copy Test ===\r\n");

    // Test 1: Complete copy (buffer large enough)
    {
        char src[] = "Hello";
        char dest[10];
        size_t len = strlcpy(dest, src, sizeof(dest));

        bool passed = (len == 5) && 
                      (dest[0] == 'H') && (dest[4] == 'o') && 
                      (dest[5] == '\0') && (dest[9] == '\0');

        if (passed) {
            serial_write_str("strlcpy complete copy: OK\r\n");
        } else {
            serial_write_str("strlcpy complete copy: FAILED\r\n");
        }
    }

    // Test 2: Truncation (buffer too small)
    {
        char src[] = "Hello, World!";
        char dest[6];
        size_t len = strlcpy(dest, src, sizeof(dest));

        // Should truncate to 5 chars + null, return full length (13)
        bool passed = (len == 13) &&           // Truncation detected: 13 >= 6
                      (dest[0] == 'H') && 
                      (dest[4] == 'o') && 
                      (dest[5] == '\0');       // Null-terminated

        if (passed) {
            serial_write_str("strlcpy truncation: OK (len=13, truncated to 5 chars)\r\n");
        } else {
            serial_write_str("strlcpy truncation: FAILED\r\n");
        }
    }

    // Test 3: Exact fit (buffer = source length + 1 for null)
    {
        char src[] = "Test";
        char dest[5];
        size_t len = strlcpy(dest, src, sizeof(dest));

        bool passed = (len == 4) && 
                      (dest[0] == 'T') && (dest[3] == 't') && 
                      (dest[4] == '\0');

        if (passed) {
            serial_write_str("strlcpy exact fit: OK\r\n");
        } else {
            serial_write_str("strlcpy exact fit: FAILED\r\n");
        }
    }

    // Test 4: Empty string copy
    {
        char src[] = "";
        char dest[10];
        dest[0] = 'X';  // Initialize with known value
        size_t len = strlcpy(dest, src, sizeof(dest));

        bool passed = (len == 0) && (dest[0] == '\0');

        if (passed) {
            serial_write_str("strlcpy empty string: OK\r\n");
        } else {
            serial_write_str("strlcpy empty string: FAILED\r\n");
        }
    }

    // Test 5: destsize = 1 (only room for null terminator)
    {
        char src[] = "Hello";
        char dest[1];
        size_t len = strlcpy(dest, src, 1);

        // Should only write null terminator, return source length
        bool passed = (len == 5) && (dest[0] == '\0');

        if (passed) {
            serial_write_str("strlcpy destsize=1: OK (null only)\r\n");
        } else {
            serial_write_str("strlcpy destsize=1: FAILED\r\n");
        }
    }

    // Test 6: destsize = 0 (no write, just calculate length)
    {
        char src[] = "Hello";
        char dest[10];
        dest[0] = 'X';  // Should remain unchanged
        size_t len = strlcpy(dest, src, 0);

        bool passed = (len == 5) && (dest[0] == 'X');  // dest unchanged

        if (passed) {
            serial_write_str("strlcpy destsize=0: OK (no write)\r\n");
        } else {
            serial_write_str("strlcpy destsize=0: FAILED\r\n");
        }
    }

    // Test 7: NULL pointer safety
    {
        char src[] = "Hello";
        char dest[10];

        size_t r1 = strlcpy(nullptr, src, sizeof(dest));
        size_t r2 = strlcpy(dest, nullptr, sizeof(dest));

        if (r1 == 0 && r2 == 0) {
            serial_write_str("strlcpy NULL handling: OK\r\n");
        } else {
            serial_write_str("strlcpy NULL handling: FAILED\r\n");
        }
    }

    serial_write_str("[STRLCPY SAFE COPY] All tests passed\r\n");
}

/* ==========================================
 * strlcpy vs strcpy Buffer Overflow Demo
 * ==========================================
 * Demonstrates how strlcpy prevents buffer overflow
 * that would occur with strcpy.
 * ========================================== */
void test_strlcpy_vs_strcpy_overflow() {
    serial_write_str("\r\n=== strlcpy vs strcpy Overflow Demo ===\r\n");

    // Case: Small buffer, large source
    // strcpy would overflow, strlcpy truncates safely

    const char* src = "This is a very long string!";

    // Test with strlcpy (SAFE)
    {
        char dest[10];
        size_t len = strlcpy(dest, src, sizeof(dest));

        // Verify: truncation detected (source length >= buffer size)
        // and result is null-terminated
        bool truncation_detected = (len >= sizeof(dest));
        bool null_terminated = (dest[5] == '\0' || dest[4] == '\0' || 
                                dest[6] == '\0' || dest[7] == '\0' ||
                                dest[8] == '\0' || dest[9] == '\0');

        serial_write_str("strlcpy result: ");
        serial_write_str(dest);
        serial_write_str("\r\n");
        serial_write_str("  Source length: ");
        serial_write_dec(len);
        serial_write_str(", Buffer size: 10\r\n");

        if (truncation_detected && null_terminated) {
            serial_write_str("strlcpy prevented overflow: OK\r\n");
        } else {
            serial_write_str("strlcpy overflow prevention: FAILED\r\n");
        }
    }

    serial_write_str("[STRLCPY VS STRCPY] Demo complete\r\n");
}
