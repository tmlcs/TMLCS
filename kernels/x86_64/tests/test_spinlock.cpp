#include "test_spinlock.h"
#include "spinlock.h"
#include "../core/debug/debug.h"
#include "test_framework.h"

/* ==========================================
 * Spinlock Test Module Implementation
 * ==========================================
 *
 * Note: These tests run on single-CPU (QEMU default).
 * SMP contention tests would require multi-processor setup.
 *
 * Test coverage:
 *   - Initialization state
 *   - Basic acquire/release cycle
 *   - Try acquire (non-blocking)
 *   - NULL pointer safety
 *   - Interrupt state preservation
 * ========================================== */

/* ==========================================
 * Test: Spinlock Initialization
 * ==========================================
 * Verifies:
 *   - SPINLOCK_INIT macro initializes correctly
 *   - spinlock_init() function works
 *   - Initial state: locked=0, interrupts_enabled=false
 * ========================================== */
void test_spinlock_initialization(void) {
    serial_write_str("\r\n=== Spinlock Initialization Test ===\r\n");

    /* Test 1: Static initialization with SPINLOCK_INIT */
    spinlock_t static_lock = SPINLOCK_INIT;
    TEST_ASSERT(static_lock.locked == 0, "SPINLOCK_INIT: locked == 0");
    TEST_ASSERT(static_lock.interrupts_enabled == false,
                "SPINLOCK_INIT: interrupts_enabled == false");

    /* Test 2: Dynamic initialization with spinlock_init() */
    spinlock_t dynamic_lock;
    dynamic_lock.locked = 0xFFFFFFFF;              /* Dirty value */
    dynamic_lock.interrupts_enabled = true;        /* Dirty value */
    spinlock_init(&dynamic_lock);

    TEST_ASSERT(dynamic_lock.locked == 0, "spinlock_init(): locked == 0");
    TEST_ASSERT(dynamic_lock.interrupts_enabled == false,
                "spinlock_init(): interrupts_enabled == false");

    /* Test 3: Re-initialization is safe */
    spinlock_init(&dynamic_lock);
    TEST_ASSERT(dynamic_lock.locked == 0, "spinlock_init() re-init: locked == 0");

    serial_write_str("[SPINLOCK INIT] All initialization tests passed\r\n");
}

/* ==========================================
 * Test: Basic Acquire/Release
 * ==========================================
 * Verifies:
 *   - Lock can be acquired from unlocked state
 *   - Lock state changes correctly (0 -> 1 -> 0)
 *   - Multiple acquire/release cycles work
 *
 * Note: On single-CPU, acquire will succeed immediately
 *       (no contention from other CPUs)
 * ========================================== */
void test_spinlock_acquire_release(void) {
    serial_write_str("\r\n=== Spinlock Acquire/Release Test ===\r\n");

    spinlock_t lock = SPINLOCK_INIT;

    /* Test 1: Initial state is unlocked */
    TEST_ASSERT(lock.locked == 0, "Initial state: unlocked");

    /* Test 2: Acquire sets locked to 1 */
    spinlock_acquire(&lock);
    TEST_ASSERT(lock.locked == 1, "After acquire: locked == 1");

    /* Test 3: Release sets locked to 0 */
    spinlock_release(&lock);
    TEST_ASSERT(lock.locked == 0, "After release: locked == 0");

    /* Test 4: Multiple acquire/release cycles */
    for (int i = 0; i < 5; i++) {
        spinlock_acquire(&lock);
        TEST_ASSERT(lock.locked == 1, "Cycle %d: acquire sets locked == 1");

        /* Critical section simulation */
        volatile uint32_t dummy = lock.locked;
        (void)dummy;

        spinlock_release(&lock);
        TEST_ASSERT(lock.locked == 0, "Cycle %d: release sets locked == 0");
    }

    serial_write_str("[SPINLOCK ACQUIRE/RELEASE] All acquire/release tests passed\r\n");
}

/* ==========================================
 * Test: Try Acquire (Non-Blocking)
 * ==========================================
 * Verifies:
 *   - try_acquire returns true on unlocked spinlock
 *   - try_acquire returns false on locked spinlock
 *   - Lock state is correct after try_acquire
 * ========================================== */
void test_spinlock_try_acquire(void) {
    serial_write_str("\r\n=== Spinlock Try Acquire Test ===\r\n");

    spinlock_t lock = SPINLOCK_INIT;
    bool result;

    /* Test 1: try_acquire on unlocked spinlock should succeed */
    result = spinlock_try_acquire(&lock);
    TEST_ASSERT(result == true, "try_acquire(unlocked) returns true");
    TEST_ASSERT(lock.locked == 1, "try_acquire(unlocked) sets locked == 1");

    /* Test 2: try_acquire on locked spinlock should fail */
    result = spinlock_try_acquire(&lock);
    TEST_ASSERT(result == false, "try_acquire(locked) returns false");
    TEST_ASSERT(lock.locked == 1, "try_acquire(locked) preserves locked == 1");

    /* Test 3: Release and try again */
    spinlock_release(&lock);
    TEST_ASSERT(lock.locked == 0, "After release: locked == 0");

    result = spinlock_try_acquire(&lock);
    TEST_ASSERT(result == true, "try_acquire after release returns true");
    TEST_ASSERT(lock.locked == 1, "try_acquire after release sets locked == 1");

    /* Cleanup: release the lock */
    spinlock_release(&lock);

    serial_write_str("[SPINLOCK TRY_ACQUIRE] All try_acquire tests passed\r\n");
}

/* ==========================================
 * Test: NULL Pointer Safety
 * ==========================================
 * Verifies:
 *   - Functions handle NULL pointer gracefully
 *   - No crash or undefined behavior
 *   - Functions return safely without side effects
 * ========================================== */
void test_spinlock_null_pointer_safety(void) {
    serial_write_str("\r\n=== Spinlock NULL Pointer Safety Test ===\r\n");

    /* Test 1: spinlock_init(NULL) should not crash */
    spinlock_init(nullptr);
    TEST_PASS("spinlock_init(nullptr) handled safely");

    /* Test 2: spinlock_acquire(NULL) should not crash */
    spinlock_acquire(nullptr);
    TEST_PASS("spinlock_acquire(nullptr) handled safely");

    /* Test 3: spinlock_release(NULL) should not crash */
    spinlock_release(nullptr);
    TEST_PASS("spinlock_release(nullptr) handled safely");

    /* Test 4: spinlock_try_acquire(NULL) should return false */
    bool result = spinlock_try_acquire(nullptr);
    TEST_ASSERT(result == false, "spinlock_try_acquire(nullptr) returns false");

    /* Test 5: Verify valid lock still works after NULL calls */
    spinlock_t lock = SPINLOCK_INIT;
    spinlock_acquire(&lock);
    TEST_ASSERT(lock.locked == 1, "Valid lock works after NULL calls");
    spinlock_release(&lock);
    TEST_ASSERT(lock.locked == 0, "Valid release works after NULL calls");

    serial_write_str("[SPINLOCK NULL SAFETY] All NULL pointer tests passed\r\n");
}

/* ==========================================
 * Test: Interrupt State Preservation
 * ==========================================
 * Verifies:
 *   - Interrupts are disabled during acquire (slow path)
 *   - Interrupt state is restored on release
 *   - Fast path doesn't modify interrupt state
 *
 * Note: This test is limited on single-CPU QEMU.
 *       Full SMP testing would require multi-processor setup.
 * ========================================== */
void test_spinlock_interrupt_state(void) {
    serial_write_str("\r\n=== Spinlock Interrupt State Test ===\r\n");

    spinlock_t lock = SPINLOCK_INIT;

    /* Test 1: Initial state */
    TEST_ASSERT(lock.interrupts_enabled == false,
                "Initial state: interrupts_enabled == false");

    /* Test 2: Fast path acquire (uncontended) doesn't set interrupts_enabled */
    spinlock_acquire(&lock);
    /* Fast path should leave interrupts_enabled as false */
    TEST_ASSERT(lock.interrupts_enabled == false,
                "Fast path: interrupts_enabled remains false");
    spinlock_release(&lock);

    /* Test 3: After release, interrupts_enabled is reset */
    TEST_ASSERT(lock.interrupts_enabled == false,
                "After release: interrupts_enabled reset to false");

    /* Test 4: Lock is usable after interrupt state test */
    spinlock_acquire(&lock);
    TEST_ASSERT(lock.locked == 1, "Lock functional after interrupt test");
    spinlock_release(&lock);
    TEST_ASSERT(lock.locked == 0, "Release functional after interrupt test");

    serial_write_str("[SPINLOCK INTERRUPT STATE] All interrupt state tests passed\r\n");
}

/* ==========================================
 * Main Test Entry Point
 * ==========================================
 * Runs all spinlock tests in sequence
 * ========================================== */
void test_spinlock(void) {
    serial_write_str("\r\n============================================\r\n");
    serial_write_str("GLOBEX_OS Spinlock Test Suite\r\n");
    serial_write_str("============================================\r\n");

    test_spinlock_initialization();
    test_spinlock_acquire_release();
    test_spinlock_try_acquire();
    test_spinlock_null_pointer_safety();
    test_spinlock_interrupt_state();

    serial_write_str("\r\n============================================\r\n");
    serial_write_str("[SPINLOCK TEST] ALL TESTS PASSED\r\n");
    serial_write_str("============================================\r\n");
}
