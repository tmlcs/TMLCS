#ifndef TEST_STRLCPY_H
#define TEST_STRLCPY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Test strlcpy safe copy functionality
 * Verifies buffer overflow prevention and truncation detection
 */
void test_strlcpy_safe_copy(void);

/**
 * @brief Demonstrate strlcpy vs strcpy buffer overflow prevention
 * Shows how strlcpy safely handles small buffers with large sources
 */
void test_strlcpy_vs_strcpy_overflow(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_STRLCPY_H */
