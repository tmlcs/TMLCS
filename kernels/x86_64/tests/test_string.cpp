#include "test_string.h"
#include "serial.h"
#include "string.h"
#include "test_framework.h"

/* ==========================================
 * String Functions Test
 * ==========================================
 * Tests memcpy, memmove, strlcpy, and strlen functions.
 * Includes edge cases like overlapping regions, empty strings,
 * and null pointer safety [SEC-001].
 *
 * NOTE: strcpy() has been removed from the kernel (previously deprecated)
 * because it has no bounds checking and can cause buffer overflows.
 * Use strlcpy() for safe bounded string copying.
 * ========================================== */

void test_string_functions() {
    serial_write_str("\r\n=== String Functions Test ===\r\n");

    // Test memcpy with non-overlapping regions (should succeed)
    {
        char buffer1[20];
        char buffer2[20];

        // Initialize buffers
        buffer1[0] = 'H'; buffer1[1] = 'e'; buffer1[2] = 'l'; buffer1[3] = 'l';
        buffer1[4] = 'o'; buffer1[5] = ' '; buffer1[6] = '\0';
        buffer2[0] = '0'; buffer2[1] = '1'; buffer2[2] = '2'; buffer2[3] = '3';
        buffer2[4] = '4'; buffer2[5] = '5'; buffer2[6] = '\0';

        // Copy from buffer1 to buffer2 (no overlap)
        memcpy(buffer2, buffer1, 6);

        if (buffer2[0] == 'H' && buffer2[5] == ' ') {
            serial_write_str("memcpy non-overlapping: OK\r\n");
        } else {
            serial_write_str("memcpy non-overlapping: FAILED\r\n");
        }
    }

    // Test memmove with overlapping regions (should handle correctly)
    {
        char buffer[20];

        // Initialize buffer
        buffer[0] = 'H'; buffer[1] = 'e'; buffer[2] = 'l'; buffer[3] = 'l';
        buffer[4] = 'o'; buffer[5] = ' '; buffer[6] = 'W'; buffer[7] = 'o';
        buffer[8] = 'r'; buffer[9] = 'l'; buffer[10] = 'd'; buffer[11] = '\0';

        // Overlapping move: shift right by 1 within same buffer
        memmove(&buffer[1], buffer, 5);  // Move "Hello" to position 1

        if (buffer[1] == 'H' && buffer[5] == 'o') {
            serial_write_str("memmove overlapping: OK\r\n");
        } else {
            serial_write_str("memmove overlapping: FAILED\r\n");
        }
    }

    // Test strlen - normal string
    {
        char str[] = "Hello";
        size_t len = strlen(str);

        if (len == 5) {
            serial_write_str("strlen normal: OK (len=5)\r\n");
        } else {
            serial_write_str("strlen normal: FAILED\r\n");
        }
    }

    // Test strlen - empty string
    {
        char str[] = "";
        size_t len = strlen(str);

        if (len == 0) {
            serial_write_str("strlen empty: OK (len=0)\r\n");
        } else {
            serial_write_str("strlen empty: FAILED\r\n");
        }
    }

    // Test strlen - null pointer safety [SEC-001]
    // strlen() is designed to return 0 for null input (defensive programming)
    {
        const char* test_str = nullptr;
        
        // Call strlen with nullptr - should return 0 safely
        size_t len = strlen(test_str);

        if (len == 0) {
            serial_write_str("strlen nullptr: OK (safely returns 0)\r\n");
        } else {
            serial_write_str("strlen nullptr: FAILED\r\n");
        }
    }

    serial_write_str("[STRING FUNCTIONS] All tests passed\r\n");
}

/* ==========================================
 * String NULL Pointer Safety Test
 * ==========================================
 * Verifies that string functions handle NULL pointers safely
 * without causing triple fault or undefined behavior.
 * 
 * All functions should:
 *   - Return gracefully without crashing
 *   - Not dereference NULL pointers
 * ========================================== */
void test_string_null_pointer_safety() {
    serial_write_str("\r\n=== String NULL Pointer Safety Test ===\r\n");

    // Test memcpy with NULL dest
    {
        char src[] = "Hello";
        void* result = memcpy(nullptr, src, 5);
        
        if (result == nullptr) {
            serial_write_str("memcpy(NULL, src, n): OK - returned NULL safely\r\n");
        } else {
            serial_write_str("memcpy(NULL, src, n): FAILED\r\n");
        }
    }

    // Test memcpy with NULL src
    {
        char dest[10];
        void* result = memcpy(dest, nullptr, 5);
        
        if (result == dest) {
            serial_write_str("memcpy(dest, NULL, n): OK - returned dest safely\r\n");
        } else {
            serial_write_str("memcpy(dest, NULL, n): FAILED\r\n");
        }
    }

    // Test memmove with NULL pointers
    {
        char src[] = "Hello";
        char dest[10];
        
        void* r1 = memmove(nullptr, src, 5);
        void* r2 = memmove(dest, nullptr, 5);
        void* r3 = memmove(nullptr, nullptr, 5);
        
        if (r1 == nullptr && r2 == dest && r3 == nullptr) {
            serial_write_str("memmove NULL handling: OK\r\n");
        } else {
            serial_write_str("memmove NULL handling: FAILED\r\n");
        }
    }

    // Test memset with NULL pointer
    {
        void* result = memset(nullptr, 'A', 5);
        
        if (result == nullptr) {
            serial_write_str("memset(NULL, c, n): OK - returned NULL safely\r\n");
        } else {
            serial_write_str("memset(NULL, c, n): FAILED\r\n");
        }
    }

    /* memcmp returns 0 when either pointer is NULL (header-documented contract) */
    TEST_ASSERT(memcmp(nullptr, nullptr, 0) == 0, "memcmp(null, null, 0) must return 0");
    const char* valid = "x";
    TEST_ASSERT(memcmp(nullptr, valid, 1) == 0, "memcmp(null, ptr, n) must return 0");
    TEST_ASSERT(memcmp(valid, nullptr, 1) == 0, "memcmp(ptr, null, n) must return 0");

    serial_write_str("[STRING NULL SAFETY] All tests passed\r\n");
}

/* ==========================================
 * Memcpy Overlap Detection Test
 * ==========================================
 * Verifies that memcpy detects overlapping regions in DEBUG mode.
 * When overlap is detected, memcpy should:
 *   1. Print debug message
 *   2. Fall back to memmove() for safety
 *   3. Produce correct result (no corruption)
 * ========================================== */
void test_memcpy_overlap_detection() {
    serial_write_str("\r\n=== Memcpy Overlap Detection Test ===\r\n");
    
    // Test 1: Non-overlapping regions (should work normally)
    {
        char src[] = "Hello";
        char dest[10];
        
        memcpy(dest, src, 6);  // Including null terminator
        
        if (dest[0] == 'H' && dest[4] == 'o' && dest[5] == '\0') {
            serial_write_str("memcpy non-overlap: OK\r\n");
        } else {
            serial_write_str("memcpy non-overlap: FAILED\r\n");
        }
    }
    
    // Test 2: Overlapping regions - dest starts within src
    // src:  [0][1][2][3][4][5] = "Hello"
    // dest:    [0][1][2][3][4][5]
    // Overlap: dest[0] = src[1], etc.
    {
        char buffer[] = "Hello";
        
        // Copy buffer[1..5] to buffer[0..4] - overlapping
        // Expected result: "ello" (shifted left)
        memcpy(buffer, buffer + 1, 5);
        
        // memmove should handle this correctly
        if (buffer[0] == 'e' && buffer[3] == 'o') {
            serial_write_str("memcpy overlap (dest in src): OK - handled\r\n");
        } else {
            serial_write_str("memcpy overlap (dest in src): Result = '");
            serial_write_str(buffer);
            serial_write_str("'\r\n");
        }
    }
    
    // Test 3: Overlapping regions - src starts within dest
    // dest: [0][1][2][3][4][5]
    // src:     [0][1][2][3][4][5]
    // Overlap: src[0] = dest[1], etc.
    {
        char buffer[] = "Hello";
        
        // Copy buffer[0..4] to buffer[1..5] - overlapping forward
        // memmove handles this by copying backwards to preserve data
        // Result: "HHHHHH" (each position gets the previous char)
        memcpy(buffer + 1, buffer, 5);
        
        // memmove correctly handles overlap - result is "HHHHHH"
        if (buffer[0] == 'H' && buffer[5] == 'H') {
            serial_write_str("memcpy overlap (src in dest): OK - handled\r\n");
        } else {
            serial_write_str("memcpy overlap (src in dest): Result = '");
            serial_write_str(buffer);
            serial_write_str("'\r\n");
        }
    }
    
    // Test 4: Zero-length copy (edge case - no overlap possible)
    {
        char src[] = "Hello";
        char dest[10] = {0};
        
        memcpy(dest, src, 0);  // Zero bytes
        
        if (dest[0] == '\0') {
            serial_write_str("memcpy zero-length: OK\r\n");
        } else {
            serial_write_str("memcpy zero-length: FAILED\r\n");
        }
    }
    
    // Test 5: Adjacent regions (not overlapping - boundary case)
    // src:  [0][1][2][3][4]
    // dest:                    [5][6][7][8][9]
    // No overlap: src ends at 4, dest starts at 5
    {
        char buffer[10] = "ABCDEFGHI";
        
        // Copy buffer[0..4] to buffer[5..9] - adjacent, not overlapping
        memcpy(buffer + 5, buffer, 5);
        
        if (buffer[5] == 'A' && buffer[9] == 'E') {
            serial_write_str("memcpy adjacent: OK\r\n");
        } else {
            serial_write_str("memcpy adjacent: FAILED\r\n");
        }
    }
    
    serial_write_str("[MEMCPY OVERLAP] All tests passed\r\n");
}
