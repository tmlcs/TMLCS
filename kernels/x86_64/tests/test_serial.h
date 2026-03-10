#ifndef TEST_SERIAL_H
#define TEST_SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

void test_serial_baud_rates(void);

/**
 * @brief Test null pointer handling
 * Verifies that null pointers do NOT mark serial as hardware failed
 */
void test_serial_null_pointer_handling(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_SERIAL_H */
