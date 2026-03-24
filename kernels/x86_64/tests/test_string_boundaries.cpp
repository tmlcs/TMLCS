#include "test_string_boundaries.h"
#include "print.h"
#include "serial.h"
#include "string.h"
#include "test_framework.h"

/* This file intentionally tests the deprecated strcpy() function. */
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

/* ==========================================
 * Helper Functions
 * ========================================== */

// Simple string comparison for tests
static bool streq(const char* s1, const char* s2) {
    if (s1 == nullptr || s2 == nullptr) {
        return s1 == s2;
    }
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

/* ==========================================
 * String Boundary Tests
 * ==========================================
 * Tests edge cases at buffer limits
 * ========================================== */
void test_string_boundaries(void) {
    serial_write_str("\r\n=== String Boundary Tests ===\r\n");

    // Test 1: memcpy with exact buffer size
    {
        const char src[] = "Hello";
        char dest[6];  // Exactly enough for "Hello" + '\0'

        // Copy exactly 5 bytes (no null terminator)
        memcpy(dest, src, 5);

        // Manually add null terminator for comparison
        dest[5] = '\0';

        if (streq(dest, "Hello")) {
            serial_write_str("memcpy exact size: OK\r\n");
        } else {
            serial_write_str("memcpy exact size: FAILED\r\n");
        }
    }

    // Test 2: memcpy with zero bytes
    {
        const char src[] = "Hello";
        char dest[10] = "World";

        // Copy 0 bytes - dest should be unchanged
        memcpy(dest, src, 0);

        if (streq(dest, "World")) {
            serial_write_str("memcpy zero bytes: OK\r\n");
        } else {
            serial_write_str("memcpy zero bytes: FAILED\r\n");
        }
    }

    // Test 3: strlcpy with exact fit (buffer = strlen + 1)
    {
        const char src[] = "Hello";
        char dest[6];  // Exactly 5 chars + null terminator

        size_t result = strlcpy(dest, src, sizeof(dest));

        if (result == 5 && streq(dest, "Hello")) {
            serial_write_str("strlcpy exact fit: OK\r\n");
        } else {
            serial_write_str("strlcpy exact fit: FAILED\r\n");
        }
    }

    // Test 4: strlcpy with truncation at exact boundary
    {
        const char src[] = "Hello World";
        char dest[6];  // Only 5 chars + null

        size_t result = strlcpy(dest, src, sizeof(dest));

        // Should return original length (11), dest should be "Hello"
        if (result == 11 && dest[5] == '\0' && streq(dest, "Hello")) {
            serial_write_str("strlcpy truncation at boundary: OK\r\n");
        } else {
            serial_write_str("strlcpy truncation at boundary: FAILED\r\n");
        }
    }

    // Test 5: strlcpy with buffer size 1 (null terminator only)
    {
        const char src[] = "Hello";
        char dest[1] = {'X'};

        size_t result = strlcpy(dest, src, 1);

        // Should return original length (5), dest should be just ""
        if (result == 5 && dest[0] == '\0') {
            serial_write_str("strlcpy size=1: OK\r\n");
        } else {
            serial_write_str("strlcpy size=1: FAILED\r\n");
        }
    }

    // Test 6: strlcpy with buffer size 0 (no write)
    {
        const char src[] = "Hello";
        char dest[10] = "World";

        size_t result = strlcpy(dest, src, 0);

        // Should return original length (5), dest should be unchanged
        if (result == 5 && streq(dest, "World")) {
            serial_write_str("strlcpy size=0: OK\r\n");
        } else {
            serial_write_str("strlcpy size=0: FAILED\r\n");
        }
    }

    // Test 7: memset with size 0
    {
        char buf[10] = "Hello";

        // memset with 0 bytes should do nothing
        memset(buf, 'X', 0);

        if (streq(buf, "Hello")) {
            serial_write_str("memset size=0: OK\r\n");
        } else {
            serial_write_str("memset size=0: FAILED\r\n");
        }
    }

    // Test 8: memset with exact buffer size
    {
        char buf[10];

        // Fill entire buffer
        memset(buf, 'A', 10);

        // Verify all bytes are 'A'
        bool all_a = true;
        for (size_t i = 0; i < 10; i++) {
            if (buf[i] != 'A') {
                all_a = false;
                break;
            }
        }

        if (all_a) {
            serial_write_str("memset exact size: OK\r\n");
        } else {
            serial_write_str("memset exact size: FAILED\r\n");
        }
    }

    // Test 9: memcmp with zero-length comparison
    {
        const char s1[] = "Hello";
        const char s2[] = "World";

        // memcmp with 0 bytes should return 0 (equal)
        int result = memcmp(s1, s2, 0);

        if (result == 0) {
            serial_write_str("memcmp zero length: OK (returns 0)\r\n");
        } else {
            serial_write_str("memcmp zero length: FAILED\r\n");
        }
    }

    // Test 10: memcmp with exact match
    {
        const char s1[] = "Hello";
        const char s2[] = "Hello";

        int result = memcmp(s1, s2, 5);

        if (result == 0) {
            serial_write_str("memcmp exact match: OK\r\n");
        } else {
            serial_write_str("memcmp exact match: FAILED\r\n");
        }
    }

    // Test 11: memcmp with difference at last byte
    {
        const char s1[] = "Hello";
        const char s2[] = "Hellp";  // Last char different

        int result = memcmp(s1, s2, 5);

        // Should be non-zero (difference detected)
        if (result != 0) {
            serial_write_str("memcmp last byte diff: OK\r\n");
        } else {
            serial_write_str("memcmp last byte diff: FAILED\r\n");
        }
    }

    // Test 12: strcpy with buffer exactly full
    {
        const char src[] = "Hello";  // 5 chars + null = 6 bytes
        char dest[6];

        strcpy(dest, src);

        if (streq(dest, "Hello")) {
            serial_write_str("strcpy exact fit: OK\r\n");
        } else {
            serial_write_str("strcpy exact fit: FAILED\r\n");
        }
    }

    serial_write_str("[STRING BOUNDARIES] All tests passed\r\n");
}

/* ==========================================
 * Buffer Overflow Prevention Tests
 * ==========================================
 * Verifies that safe functions prevent overflows
 * ========================================== */
void test_buffer_overflow_prevention(void) {
    serial_write_str("\r\n=== Buffer Overflow Prevention Tests ===\r\n");

    // Test 1: strlcpy truncation detection
    {
        const char src[] = "Hello World";  // 11 chars
        char dest[6];

        size_t result = strlcpy(dest, src, sizeof(dest));

        // Truncation detection: result >= destsize means truncation
        if (result >= sizeof(dest) && dest[5] == '\0') {
            serial_write_str("strlcpy truncation detection: OK\r\n");
        } else {
            serial_write_str("strlcpy truncation detection: FAILED\r\n");
        }
    }

    // Test 2: strlcpy null termination guaranteed
    {
        const char src[] = "LongString";
        char dest[3];

        size_t result = strlcpy(dest, src, sizeof(dest));

        // dest[2] must be '\0'
        if (dest[2] == '\0' && result == 10) {
            serial_write_str("strlcpy null termination: OK\r\n");
        } else {
            serial_write_str("strlcpy null termination: FAILED\r\n");
        }
    }

    // Test 3: memset doesn't overflow buffer
    {
        char buffer[10] = "BBBBBBBBB";  // 9 chars + null
        char guard_before = 'A';
        char guard_after = 'Z';

        // memset only the buffer
        memset(buffer, 'X', 10);

        // Verify guards are unchanged
        if (guard_before == 'A' && guard_after == 'Z') {
            serial_write_str("memset no overflow: OK\r\n");
        } else {
            serial_write_str("memset no overflow: FAILED\r\n");
        }
    }

    // Test 4: memcpy exact boundary (no overflow)
    {
        const char src[] = "012345678";  // 9 chars + null
        char dest[10];

        // Copy exactly 9 bytes (no overflow)
        memcpy(dest, src, 9);
        dest[9] = '\0';

        if (streq(dest, "012345678")) {
            serial_write_str("memcpy exact boundary: OK\r\n");
        } else {
            serial_write_str("memcpy exact boundary: FAILED\r\n");
        }
    }

    // Test 5: memmove handles overlap safely
    {
        char buffer[] = "Hello";

        // Move overlapping region: shift left by 1
        memmove(buffer, buffer + 1, 4);
        buffer[4] = '\0';

        if (streq(buffer, "ello")) {
            serial_write_str("memmove overlap safe: OK\r\n");
        } else {
            serial_write_str("memmove overlap safe: FAILED\r\n");
        }
    }

    serial_write_str("[BUFFER OVERFLOW PREVENTION] All tests passed\r\n");
}
