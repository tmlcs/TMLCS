#ifndef TEST_STRING_H
#define TEST_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

void test_string_functions(void);

/**
 * @brief Test memcpy overlap detection
 * Verifies that overlapping regions are detected in DEBUG mode
 */
void test_memcpy_overlap_detection(void);

/**
 * @brief Test NULL pointer safety in string functions
 * Verifies that memcpy, memmove, strcpy, memset, memcmp handle NULL safely
 */
void test_string_null_pointer_safety(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_STRING_H */
