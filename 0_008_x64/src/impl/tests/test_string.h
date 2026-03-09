#ifndef TEST_STRING_H
#define TEST_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

void test_string_functions(void);

/**
 * @brief Test memcpy overlap detection (CRIT-005)
 * Verifies that overlapping regions are detected in DEBUG mode
 */
void test_memcpy_overlap_detection(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_STRING_H */
