#ifndef TEST_PRINT_H
#define TEST_PRINT_H

#ifdef __cplusplus
extern "C" {
#endif

void test_print_functions(void);

/**
 * @brief Test decimal conversion boundary values
 * Tests uint32_max (4294967295), uint64_max (18446744073709551615), and zero
 */
void test_decimal_boundary_values(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_PRINT_H */
