#ifndef TEST_SPINLOCK_STRESS_H
#define TEST_SPINLOCK_STRESS_H

/* =============================================================================
 * Spinlock Stress Test Header
 * =============================================================================
 *
 * Stress tests for spinlock under high contention scenarios.
 * Tests data integrity, rapid acquire/release, and edge cases.
 *
 * @note Tests run on single-CPU QEMU by default
 * @note SMP testing requires multi-processor setup
 * =============================================================================
 */

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Main Entry Point
 * =============================================================================
 * Runs all spinlock stress tests
 * =============================================================================
 */
void test_spinlock_stress(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_SPINLOCK_STRESS_H */
