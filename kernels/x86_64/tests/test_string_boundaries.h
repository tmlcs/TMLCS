#ifndef TEST_STRING_BOUNDARIES_H
#define TEST_STRING_BOUNDARIES_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Test boundary conditions in string functions
 * Verifies correct behavior at buffer limits and edge cases
 *
 * Tests include:
 *   - memcpy with exact buffer size
 *   - strlcpy with truncation at exact boundary
 *   - memset with size 0 and SIZE_MAX
 *   - strcpy with buffer exactly full
 *   - memcmp with zero-length comparison
 */
void test_string_boundaries(void);

/**
 * @brief Test buffer overflow prevention
 * Verifies that safe functions prevent buffer overflows
 *
 * Tests include:
 *   - strlcpy vs strcpy with small buffer
 *   - memcpy with size larger than destination
 *   - memset with size larger than buffer
 */
void test_buffer_overflow_prevention(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_STRING_BOUNDARIES_H */
