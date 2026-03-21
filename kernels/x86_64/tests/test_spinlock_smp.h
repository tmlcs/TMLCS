#ifndef TEST_SPINLOCK_SMP_H
#define TEST_SPINLOCK_SMP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Spinlock SMP Stress Test Module
 * =============================================================================
 * Tests for spinlock implementation under REAL SMP (multi-processor) conditions.
 *
 * IMPORTANT: These tests require QEMU with -smp 4 (or similar multi-CPU config).
 * Running on single-CPU will NOT test actual concurrency, only logical correctness.
 *
 * Tests cover:
 *   - Concurrent lock acquisition from multiple CPUs
 *   - Data integrity under contention
 *   - Lock fairness (no starvation)
 *   - Performance under load
 *   - Memory ordering correctness
 *
 * Test Scenarios:
 *   1. Multiple CPUs simultaneously acquiring same lock
 *   2. Counter increment from all CPUs (verify no lost updates)
 *   3. Per-CPU data with shared lock
 *   4. Nested lock acquisition patterns
 *   5. Lock contention with mixed read/write
 *
 * Expected Behavior:
 *   - All tests should pass on both single-CPU and multi-CPU
 *   - Multi-CPU tests will take longer due to contention
 *   - Counter tests should show NO lost updates (atomicity guaranteed)
 *
 * @note Requires: QEMU with -smp 4 flag
 * @note Uses: serial output for progress reporting
 * @note Thread-safe: All tests use proper synchronization
 * =============================================================================
 */

/**
 * Run all SMP spinlock tests
 * Called from kernel_main() test suite
 *
 * @note This function will detect if running on SMP or single-CPU
 *       and adjust test expectations accordingly
 */
void test_spinlock_smp(void);

/**
 * Test concurrent lock acquisition from multiple CPUs
 * Verifies that multiple CPUs can safely contend for the same lock
 *
 * Test Design:
 *   - All CPUs attempt to acquire lock simultaneously
 *   - Each CPU increments a counter when it holds the lock
 *   - Final counter value should equal total iterations × CPUs
 *
 * @note Requires SMP to fully test contention
 */
void test_smp_concurrent_acquire(void);

/**
 * Test data integrity under SMP contention
 * Verifies no lost updates when multiple CPUs modify shared data
 *
 * Test Design:
 *   - Shared counter initially zero
 *   - Each CPU increments counter 1000 times
 *   - Final value should be exactly CPUs × 1000
 *
 * Success Criteria:
 *   - counter == expected_value (no lost updates)
 *   - All CPUs completed their iterations
 *
 * @note This is the PRIMARY test for SMP correctness
 */
void test_smp_data_integrity(void);

/**
 * Test per-CPU data with shared lock
 * Verifies each CPU can safely access its own data while contending
 *
 * Test Design:
 *   - Array of per-CPU counters
 *   - Each CPU increments only its own counter
 *   - Lock protects the array access
 *   - All counters should be correct at end
 *
 * @note Tests both locking and per-CPU data isolation
 */
void test_smp_per_cpu_data(void);

/**
 * Test lock fairness (no starvation)
 * Verifies all CPUs eventually acquire the lock
 *
 * Test Design:
 *   - Track which CPU acquired lock each time
 *   - Run many iterations
 *   - Verify all CPUs got the lock at least once
 *
 * Success Criteria:
 *   - All CPUs acquired lock at least N times
 *   - No CPU starved (minimum acquisitions threshold)
 *
 * @note Ticket lock should provide good fairness
 */
void test_smp_fairness(void);

/**
 * Test nested lock acquisition patterns
 * Verifies no deadlock with multiple locks
 *
 * Test Design:
 *   - Two locks: lock_a and lock_b
 *   - CPUs acquire in different orders
 *   - Verify no deadlock occurs
 *
 * @note Tests for potential deadlock scenarios
 */
void test_smp_nested_locks(void);

/**
 * Test memory ordering under SMP
 * Verifies that memory operations are properly ordered across CPUs
 *
 * Test Design:
 *   - CPU 0: Write data, then set flag
 *   - CPU 1: Wait for flag, then read data
 *   - Verify data is correctly visible
 *
 * @note Tests memory barrier effectiveness
 */
void test_smp_memory_ordering(void);

/**
 * Detect if running on SMP system
 * @return 1 if SMP detected (multiple CPUs), 0 if single-CPU
 *
 * Uses CPUID to detect number of logical processors
 */
int smp_is_smp_system(void);

/**
 * Get current CPU ID (APIC ID)
 * @return CPU ID (0 to N-1)
 *
 * Uses CPUID leaf 1 to get APIC ID
 */
int smp_get_cpu_id(void);

/**
 * Get number of logical processors
 * @return Number of logical processors detected
 *
 * Uses CPUID to detect processor count
 */
int smp_get_cpu_count(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_SPINLOCK_SMP_H */
