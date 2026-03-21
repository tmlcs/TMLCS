#ifndef TEST_PIT_H
#define TEST_PIT_H

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * PIT (Timer) Test Module
 * =============================================================================
 * Tests for PIT (Programmable Interval Timer) driver.
 *
 * Tests cover:
 *   - PIT initialization
 *   - Frequency configuration
 *   - Tick counting
 *   - IRQ registration and handling
 *   - Wait functions (ms, us)
 *   - Statistics tracking
 *
 * @note Requires PIT hardware (8253/8254)
 * @note Tests modify global timer state
 * =============================================================================
 */

/**
 * Run all PIT tests
 * Called from kernel_main() test suite
 */
void test_pit_all(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_PIT_H */
