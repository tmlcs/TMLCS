#ifndef TEST_SPINLOCK_H
#define TEST_SPINLOCK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================
 * Spinlock Test Module
 * ==========================================
 * Tests for spinlock implementation in src/lib/spinlock/
 *
 * Tests cover:
 *   - Initial state validation
 *   - Basic acquire/release
 *   - Try acquire (non-blocking)
 *   - NULL pointer handling
 *   - Interrupt state preservation
 *
 * @note These tests run on single-CPU (QEMU default)
 *       SMP contention tests require multi-processor setup
 * ========================================== */

/**
 * Run all spinlock tests
 * Called from kernel_main() test suite
 */
void test_spinlock(void);

/**
 * Test spinlock initialization
 * Verifies SPINLOCK_INIT macro and spinlock_init()
 */
void test_spinlock_initialization(void);

/**
 * Test basic acquire and release
 * Verifies lock can be acquired and released
 */
void test_spinlock_acquire_release(void);

/**
 * Test try_acquire functionality
 * Verifies non-blocking acquire attempt
 */
void test_spinlock_try_acquire(void);

/**
 * Test NULL pointer handling
 * Verifies functions handle NULL gracefully
 */
void test_spinlock_null_pointer_safety(void);

/**
 * Test interrupt state preservation
 * Verifies interrupts are restored correctly after release
 */
void test_spinlock_interrupt_state(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_SPINLOCK_H */
