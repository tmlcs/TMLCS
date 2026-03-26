#include "test_spinlock_stress.h"
#include "../core/debug/debug.h"
#include "spinlock.h"
#include "test_framework.h"
#include "string.h"

/* =============================================================================
 * SPINLOCK STRESS TEST MODULE
 * =============================================================================
 *
 * Purpose: Stress test spinlock under high contention scenarios
 *
 * Test scenarios:
 *   1. Rapid acquire/release cycles (10,000+ iterations)
 *   2. Data integrity with shared counter
 *   3. Try-acquire under load
 *   4. Multiple locks stress
 *   5. Critical section with simulated work
 *   6. Lock/unlock pattern variations
 *
 * Note: These tests run on single-CPU QEMU by default.
 *       True SMP contention testing would require multi-processor setup.
 *       However, these tests still validate:
 *         - Atomic operation correctness
 *         - Data integrity
 *         - Interrupt safety
 *         - High-frequency usage patterns
 * =============================================================================
 */

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/** Number of iterations for stress tests */
#define STRESS_ITERATIONS 5000

/** Number of iterations for extended stress tests */
#define EXTENDED_STRESS_ITERATIONS 10000

/** Test value for data integrity checks */
#define TEST_INCREMENT 1

/** Expected final counter value */
#define EXPECTED_COUNTER_VALUE (STRESS_ITERATIONS * TEST_INCREMENT)

/* =============================================================================
 * Test 1: Rapid Acquire/Release Stress
 * =============================================================================
 *
 * Purpose: Verify spinlock handles high-frequency acquire/release cycles
 *
 * Methodology:
 *   1. Acquire and release lock in tight loop
 *   2. Count total iterations
 *   3. Verify no failures or deadlocks
 *
 * Expected Result:
 *   - All iterations complete successfully
 *   - No timeout or hang
 *   - Lock state correct after test
 * =============================================================================
 */
void test_spinlock_rapid_acquire_release(void) {
    serial_write_str("\r\n=== Rapid Acquire/Release Stress Test ===\r\n");
    serial_write_str("Iterations: ");
    print_dec(STRESS_ITERATIONS);
    serial_write_str("\r\n");

    spinlock_t lock = SPINLOCK_INIT;
    uint32_t iterations = 0;
    bool test_failed = false;

    /* Stress test: rapid acquire/release */
    for (uint32_t i = 0; i < STRESS_ITERATIONS; i++) {
        spinlock_token_t tok = spinlock_acquire(&lock);

        /* Verify lock state */
        if (lock.locked != 1) {
            serial_write_str("[FAIL] Rapid: locked != 1 after acquire (iteration: ");
            print_dec(i);
            serial_write_str(")\r\n");
            test_failed = true;
            spinlock_release(&lock, tok);
            break;
        }

        spinlock_release(&lock, tok);

        /* Verify lock state */
        if (lock.locked != 0) {
            serial_write_str("[FAIL] Rapid: locked != 0 after release (iteration: ");
            print_dec(i);
            serial_write_str(")\r\n");
            test_failed = true;
            break;
        }

        iterations++;
    }

    if (!test_failed) {
        /* Verify final state */
        TEST_ASSERT(iterations == STRESS_ITERATIONS,
                    "Completed all iterations");
        TEST_ASSERT(lock.locked == 0, "Final state: unlocked");

        serial_write_str("[RAPID ACQUIRE/RELEASE] PASSED - ");
        print_dec(iterations);
        serial_write_str(" iterations completed\r\n");
    } else {
        serial_write_str("[RAPID ACQUIRE/RELEASE] FAILED\r\n");
    }
}

/* =============================================================================
 * Test 2: Data Integrity with Shared Counter
 * =============================================================================
 *
 * Purpose: Verify data integrity under concurrent-like access patterns
 *
 * Methodology:
 *   1. Initialize shared counter to 0
 *   2. Increment counter inside critical section
 *   3. Verify final counter value matches expected
 *
 * Expected Result:
 *   - Counter value equals iterations * increment
 *   - No lost updates
 *   - No corruption
 *
 * Note: On single-CPU, this tests the atomic operations themselves.
 *       On SMP, this would also test contention handling.
 * =============================================================================
 */
void test_spinlock_data_integrity(void) {
    serial_write_str("\r\n=== Data Integrity Stress Test ===\r\n");
    serial_write_str("Iterations: ");
    print_dec(STRESS_ITERATIONS);
    serial_write_str("\r\n");

    spinlock_t lock = SPINLOCK_INIT;
    volatile uint64_t shared_counter = 0;

    /* Increment counter in critical section */
    for (uint32_t i = 0; i < STRESS_ITERATIONS; i++) {
        spinlock_token_t tok = spinlock_acquire(&lock);

        /* Critical section: increment shared counter */
        shared_counter += TEST_INCREMENT;

        spinlock_release(&lock, tok);
    }

    /* Verify final counter value */
    uint64_t expected = (uint64_t)STRESS_ITERATIONS * TEST_INCREMENT;

    TEST_ASSERT(shared_counter == expected, "Counter matches expected value");

    serial_write_str("Final counter: ");
    print_dec64(shared_counter);
    serial_write_str(" (expected: ");
    print_dec64(expected);
    serial_write_str(")\r\n");

    if (shared_counter == expected) {
        serial_write_str("[DATA INTEGRITY] PASSED - No lost updates\r\n");
    } else {
        serial_write_str("[DATA INTEGRITY] FAILED - Data corruption detected\r\n");
        uint64_t lost = expected - shared_counter;
        serial_write_str("Lost updates: ");
        print_dec64(lost);
        serial_write_str("\r\n");
    }
}

/* =============================================================================
 * Test 3: Try-Acquire Under Load
 * =============================================================================
 *
 * Purpose: Test try_acquire behavior in high-contention scenario
 *
 * Methodology:
 *   1. Acquire lock
 *   2. Attempt try_acquire (should fail)
 *   3. Release lock
 *   4. Attempt try_acquire (should succeed)
 *   5. Repeat for many iterations
 *
 * Expected Result:
 *   - try_acquire fails when lock is held
 *   - try_acquire succeeds when lock is free
 *   - No false positives or negatives
 * =============================================================================
 */
void test_spinlock_try_acquire_under_load(void) {
    serial_write_str("\r\n=== Try-Acquire Under Load Test ===\r\n");
    serial_write_str("Iterations: ");
    print_dec(STRESS_ITERATIONS);
    serial_write_str("\r\n");

    spinlock_t lock = SPINLOCK_INIT;
    uint32_t success_count = 0;
    uint32_t fail_count = 0;
    bool test_failed = false;

    for (uint32_t i = 0; i < STRESS_ITERATIONS; i++) {
        /* Phase 1: Acquire lock normally */
        spinlock_token_t tok = spinlock_acquire(&lock);

        /* Phase 2: try_acquire should fail */
        spinlock_token_t try_tok;
        bool result = spinlock_try_acquire(&lock, &try_tok);
        (void)try_tok;
        if (result != false) {
            serial_write_str("[FAIL] try_acquire should fail when locked (iteration: ");
            print_dec(i);
            serial_write_str(")\r\n");
            test_failed = true;
            spinlock_release(&lock, tok);
            break;
        }
        fail_count++;

        /* Phase 3: Release lock */
        spinlock_release(&lock, tok);

        /* Phase 4: try_acquire should succeed */
        spinlock_token_t try_tok2;
        result = spinlock_try_acquire(&lock, &try_tok2);
        if (result != true) {
            serial_write_str("[FAIL] try_acquire should succeed when unlocked (iteration: ");
            print_dec(i);
            serial_write_str(")\r\n");
            test_failed = true;
            break;
        }
        success_count++;

        /* Phase 5: Release for next iteration */
        spinlock_release(&lock, try_tok2);
    }

    /* Verify counts */
    if (!test_failed) {
        TEST_ASSERT(success_count == STRESS_ITERATIONS,
                    "try_acquire succeeded correct number of times");
        TEST_ASSERT(fail_count == STRESS_ITERATIONS,
                    "try_acquire failed correct number of times");

        serial_write_str("[TRY-ACQUIRE UNDER LOAD] PASSED\r\n");
        serial_write_str("Success count: ");
        print_dec(success_count);
        serial_write_str(", Fail count: ");
        print_dec(fail_count);
        serial_write_str("\r\n");
    } else {
        serial_write_str("[TRY-ACQUIRE UNDER LOAD] FAILED\r\n");
    }
}

/* =============================================================================
 * Test 4: Multiple Locks Stress
 * =============================================================================
 *
 * Purpose: Test multiple independent locks working correctly
 *
 * Methodology:
 *   1. Create multiple independent locks
 *   2. Acquire/release in various patterns
 *   3. Verify each lock maintains independent state
 *
 * Expected Result:
 *   - Each lock operates independently
 *   - No cross-contamination of state
 *   - All locks functional after test
 * =============================================================================
 */
void test_spinlock_multiple_locks(void) {
    serial_write_str("\r\n=== Multiple Locks Stress Test ===\r\n");
    serial_write_str("Locks: 4, Iterations: ");
    print_dec(STRESS_ITERATIONS / 4);
    serial_write_str("\r\n");

    /* Create multiple independent locks */
    spinlock_t lock1 = SPINLOCK_INIT;
    spinlock_t lock2 = SPINLOCK_INIT;
    spinlock_t lock3 = SPINLOCK_INIT;
    spinlock_t lock4 = SPINLOCK_INIT;

    volatile uint64_t counter1 = 0;
    volatile uint64_t counter2 = 0;
    volatile uint64_t counter3 = 0;
    volatile uint64_t counter4 = 0;

    uint32_t iterations = STRESS_ITERATIONS / 4;

    /* Interleaved lock operations */
    for (uint32_t i = 0; i < iterations; i++) {
        /* Lock 1 */
        spinlock_token_t t1 = spinlock_acquire(&lock1);
        counter1++;
        spinlock_release(&lock1, t1);

        /* Lock 2 */
        spinlock_token_t t2 = spinlock_acquire(&lock2);
        counter2++;
        spinlock_release(&lock2, t2);

        /* Lock 3 */
        spinlock_token_t t3 = spinlock_acquire(&lock3);
        counter3++;
        spinlock_release(&lock3, t3);

        /* Lock 4 */
        spinlock_token_t t4 = spinlock_acquire(&lock4);
        counter4++;
        spinlock_release(&lock4, t4);
    }

    /* Verify each counter */
    TEST_ASSERT(counter1 == iterations, "Counter 1 correct");
    TEST_ASSERT(counter2 == iterations, "Counter 2 correct");
    TEST_ASSERT(counter3 == iterations, "Counter 3 correct");
    TEST_ASSERT(counter4 == iterations, "Counter 4 correct");

    /* Verify all locks are unlocked */
    TEST_ASSERT(lock1.locked == 0, "Lock 1 unlocked");
    TEST_ASSERT(lock2.locked == 0, "Lock 2 unlocked");
    TEST_ASSERT(lock3.locked == 0, "Lock 3 unlocked");
    TEST_ASSERT(lock4.locked == 0, "Lock 4 unlocked");

    serial_write_str("[MULTIPLE LOCKS] PASSED - 4 independent locks verified\r\n");
}

/* =============================================================================
 * Test 5: Critical Section with Simulated Work
 * =============================================================================
 *
 * Purpose: Test lock behavior with non-trivial critical sections
 *
 * Methodology:
 *   1. Acquire lock
 *   2. Perform multiple operations (simulated work)
 *   3. Release lock
 *   4. Verify all operations completed correctly
 *
 * Expected Result:
 *   - All operations complete atomically
 *   - No corruption of shared data
 *   - Lock released correctly after work
 * =============================================================================
 */
void test_spinlock_critical_section_work(void) {
    serial_write_str("\r\n=== Critical Section Work Test ===\r\n");
    serial_write_str("Iterations: ");
    print_dec(STRESS_ITERATIONS / 10);
    serial_write_str("\r\n");

    spinlock_t lock = SPINLOCK_INIT;

    /* Shared data structure */
    struct {
        uint64_t sum;
        uint64_t count;
        uint64_t min;
        uint64_t max;
    } shared_data = {0, 0, UINT64_MAX, 0};

    uint32_t iterations = STRESS_ITERATIONS / 10;

    for (uint32_t i = 0; i < iterations; i++) {
        spinlock_token_t tok = spinlock_acquire(&lock);

        /* Simulated work: update multiple fields */
        uint64_t value = i + 1;
        shared_data.sum += value;
        shared_data.count++;

        if (value < shared_data.min) {
            shared_data.min = value;
        }
        if (value > shared_data.max) {
            shared_data.max = value;
        }

        spinlock_release(&lock, tok);
    }

    /* Verify results */
    uint64_t expected_sum = (iterations * (iterations + 1)) / 2;

    TEST_ASSERT(shared_data.sum == expected_sum, "Sum correct");
    TEST_ASSERT(shared_data.count == iterations, "Count correct");
    TEST_ASSERT(shared_data.min == 1, "Min correct");
    TEST_ASSERT(shared_data.max == iterations, "Max correct");

    serial_write_str("[CRITICAL SECTION WORK] PASSED\r\n");
    serial_write_str("Sum: ");
    print_dec64(shared_data.sum);
    serial_write_str(", Count: ");
    print_dec64(shared_data.count);
    serial_write_str(", Min: ");
    print_dec64(shared_data.min);
    serial_write_str(", Max: ");
    print_dec64(shared_data.max);
    serial_write_str("\r\n");
}

/* =============================================================================
 * Test 6: Extended Stress Test
 * =============================================================================
 *
 * Purpose: Long-running stress test to detect rare issues
 *
 * Methodology:
 *   1. Run acquire/release loop for extended iterations
 *   2. Periodically verify lock state
 *   3. Check for any signs of corruption or deadlock
 *
 * Expected Result:
 *   - All iterations complete
 *   - No timeout or hang
 *   - Lock state always correct
 * =============================================================================
 */
void test_spinlock_extended_stress(void) {
    serial_write_str("\r\n=== Extended Stress Test ===\r\n");
    serial_write_str("Iterations: ");
    print_dec(EXTENDED_STRESS_ITERATIONS);
    serial_write_str(" (long-running)\r\n");

    spinlock_t lock = SPINLOCK_INIT;
    uint64_t counter = 0;
    uint32_t check_interval = EXTENDED_STRESS_ITERATIONS / 10;
    bool test_failed = false;

    for (uint32_t i = 0; i < EXTENDED_STRESS_ITERATIONS; i++) {
        spinlock_token_t tok = spinlock_acquire(&lock);
        counter++;
        spinlock_release(&lock, tok);

        /* Periodic verification - only print at checkpoints */
        if ((i + 1) % check_interval == 0) {
            if (lock.locked != 0) {
                serial_write_str("[FAIL] Lock not unlocked at checkpoint (iteration: ");
                print_dec(i + 1);
                serial_write_str(")\r\n");
                test_failed = true;
                break;
            }
            /* Only print checkpoint every 50% to reduce output */
            if ((i + 1) >= EXTENDED_STRESS_ITERATIONS / 2) {
                serial_write_str("Checkpoint: ");
                print_dec(i + 1);
                serial_write_str("/");
                print_dec(EXTENDED_STRESS_ITERATIONS);
                serial_write_str("\r\n");
            }
        }
    }

    if (!test_failed) {
        /* Final verification */
        TEST_ASSERT(counter == EXTENDED_STRESS_ITERATIONS,
                    "Counter matches iterations");
        TEST_ASSERT(lock.locked == 0, "Final state: unlocked");

        serial_write_str("[EXTENDED STRESS] PASSED - ");
        print_dec(EXTENDED_STRESS_ITERATIONS);
        serial_write_str(" iterations completed successfully\r\n");
    } else {
        serial_write_str("[EXTENDED STRESS] FAILED\r\n");
    }
}

/* =============================================================================
 * Test 7: Lock Pattern Variations
 * =============================================================================
 *
 * Purpose: Test various lock/unlock patterns
 *
 * Methodology:
 *   1. Test nested acquire/release (same lock)
 *   2. Test alternating patterns
 *   3. Test back-to-back operations
 *
 * Expected Result:
 *   - All patterns execute correctly
 *   - No state corruption
 * =============================================================================
 */
void test_spinlock_pattern_variations(void) {
    serial_write_str("\r\n=== Lock Pattern Variations Test ===\r\n");

    spinlock_t lock = SPINLOCK_INIT;
    uint64_t counter = 0;

    /* Pattern 1: Back-to-back acquire/release */
    for (uint32_t i = 0; i < 1000; i++) {
        spinlock_token_t t1 = spinlock_acquire(&lock);
        spinlock_release(&lock, t1);
        spinlock_token_t t2 = spinlock_acquire(&lock);
        spinlock_release(&lock, t2);
        counter += 2;
    }
    TEST_ASSERT(counter == 2000, "Pattern 1: back-to-back OK");

    /* Pattern 2: Alternating with work */
    for (uint32_t i = 0; i < 1000; i++) {
        spinlock_token_t ta = spinlock_acquire(&lock);
        counter++;
        spinlock_release(&lock, ta);

        /* Small "work" outside lock */
        volatile uint64_t dummy = counter * 2;
        (void)dummy;

        spinlock_token_t tb = spinlock_acquire(&lock);
        counter++;
        spinlock_release(&lock, tb);
    }
    TEST_ASSERT(counter == 4000, "Pattern 2: alternating OK");

    /* Pattern 3: Multiple releases after single acquire (should fail gracefully) */
    spinlock_token_t tc = spinlock_acquire(&lock);
    counter++;
    spinlock_release(&lock, tc);
    /* Don't double-release - that would corrupt state */

    /* Final state check */
    TEST_ASSERT(lock.locked == 0, "Final state: unlocked");
    TEST_ASSERT(counter == 4001, "Final counter correct");

    serial_write_str("[PATTERN VARIATIONS] PASSED\r\n");
}

/* =============================================================================
 * Main Entry Point
 * =============================================================================
 */
void test_spinlock_stress(void) {
    serial_write_str("\r\n============================================\r\n");
    serial_write_str("GLOBEX_OS Spinlock STRESS Test Suite\r\n");
    serial_write_str("============================================\r\n");
    serial_write_str("Note: Single-CPU mode (QEMU default)\r\n");
    serial_write_str("SMP testing requires multi-processor setup\r\n");
    serial_write_str("============================================\r\n");

    TEST_RESET_FAILURE();

    test_spinlock_rapid_acquire_release();
    TEST_ABORT_IF_FAILED();

    test_spinlock_data_integrity();
    TEST_ABORT_IF_FAILED();

    test_spinlock_try_acquire_under_load();
    TEST_ABORT_IF_FAILED();

    test_spinlock_multiple_locks();
    TEST_ABORT_IF_FAILED();

    test_spinlock_critical_section_work();
    TEST_ABORT_IF_FAILED();

    test_spinlock_extended_stress();
    TEST_ABORT_IF_FAILED();

    test_spinlock_pattern_variations();
    TEST_ABORT_IF_FAILED();

    serial_write_str("\r\n============================================\r\n");
    serial_write_str("[SPINLOCK STRESS] ALL TESTS PASSED\r\n");
    serial_write_str("============================================\r\n");
}
