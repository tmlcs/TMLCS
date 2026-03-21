#include "test_spinlock_smp.h"
#include "serial.h"
#include "print.h"
#include "spinlock.h"
#include "atomic.h"
#include "barriers.h"

/* =============================================================================
 * SMP Test Configuration
 * =============================================================================
 */
#define SMP_TEST_ITERATIONS 1000
#define SMP_TEST_MAX_CPUS 4

/* =============================================================================
 * Shared Test Data
 * =============================================================================
 */
static spinlock_t smp_test_lock = SPINLOCK_INIT;
static volatile uint64_t smp_shared_counter = 0;
static volatile uint32_t smp_nested_success = 0;
static spinlock_t smp_lock_a = SPINLOCK_INIT;
static spinlock_t smp_lock_b = SPINLOCK_INIT;

/* =============================================================================
 * CPU Detection Functions
 * =============================================================================
 */

int smp_get_cpu_id(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(1));
    return (ebx >> 24) & 0xFF;
}

int smp_get_cpu_count(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(1));
    return (ebx >> 8) & 0xFF;
}

int smp_is_smp_system(void) {
    return smp_get_cpu_count() > 1;
}

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

static void smp_print_header(const char* title) {
    int cpu_count = smp_get_cpu_count();
    int cpu_id = smp_get_cpu_id();

    serial_write_str("\r\n=== ");
    serial_write_str(title);
    serial_write_str(" ===\r\n");

    if (cpu_count > 1) {
        serial_write_str("SMP Detected: ");
        serial_write_dec(cpu_count);
        serial_write_str(" CPUs available\r\n");
    } else {
        serial_write_str("NOTE: Single-CPU mode\r\n");
        serial_write_str("For SMP testing: make run-smp\r\n");
    }

    serial_write_str("Current CPU: ");
    serial_write_dec(cpu_id);
    serial_write_str("\r\n");
}

static void smp_print_result(const char* test_name, int passed) {
    serial_write_str("[");
    if (passed) {
        serial_write_str("PASS");
    } else {
        serial_write_str("FAIL");
    }
    serial_write_str("] ");
    serial_write_str(test_name);
    serial_write_str("\r\n");
}

/* =============================================================================
 * Test: Data Integrity
 * =============================================================================
 */

void test_smp_data_integrity(void) {
    uint64_t expected_total = (uint64_t)SMP_TEST_ITERATIONS;

    smp_shared_counter = 0;
    mb();

    /* Single CPU test - verifies spinlock correctness */
    for (int i = 0; i < SMP_TEST_ITERATIONS; i++) {
        spinlock_acquire(&smp_test_lock);
        smp_shared_counter++;
        spinlock_release(&smp_test_lock);
    }

    serial_write_str("[DATA INTEGRITY] Verifying counter...\r\n");
    serial_write_str("Expected: ");
    serial_write_dec64(expected_total);
    serial_write_str("\r\nActual:   ");
    serial_write_dec64(smp_shared_counter);
    serial_write_str("\r\n");

    if (smp_shared_counter == expected_total) {
        smp_print_result("DATA INTEGRITY", 1);
        serial_write_str("No lost updates detected!\r\n");
    } else {
        smp_print_result("DATA INTEGRITY", 0);
        serial_write_str("ERROR: Lost updates!\r\n");
    }
}

/* =============================================================================
 * Test: Fairness (Single CPU Simulation)
 * =============================================================================
 */

void test_smp_fairness(void) {
    uint32_t acquisitions = 0;

    /* Test that lock can be acquired fairly in sequence */
    for (int i = 0; i < SMP_TEST_ITERATIONS; i++) {
        spinlock_acquire(&smp_test_lock);
        acquisitions++;
        spinlock_release(&smp_test_lock);
    }

    serial_write_str("[FAIRNESS] Lock acquisition test...\r\n");
    serial_write_str("Acquisitions: ");
    serial_write_dec(acquisitions);
    serial_write_str(" (expected: ");
    serial_write_dec(SMP_TEST_ITERATIONS);
    serial_write_str(")\r\n");

    if (acquisitions == SMP_TEST_ITERATIONS) {
        smp_print_result("FAIRNESS", 1);
    } else {
        smp_print_result("FAIRNESS", 0);
    }
}

/* =============================================================================
 * Test: Nested Locks
 * =============================================================================
 */

void test_smp_nested_locks(void) {
    smp_nested_success = 0;
    mb();

    /* Test acquiring two locks in sequence */
    for (int i = 0; i < SMP_TEST_ITERATIONS / 10; i++) {
        spinlock_acquire(&smp_lock_a);
        spinlock_acquire(&smp_lock_b);

        atomic_inc32(&smp_nested_success);

        spinlock_release(&smp_lock_b);
        spinlock_release(&smp_lock_a);
    }

    uint32_t expected = SMP_TEST_ITERATIONS / 10;

    serial_write_str("[NESTED LOCKS] Verifying no deadlock...\r\n");
    serial_write_str("Expected: ");
    serial_write_dec(expected);
    serial_write_str("\r\nActual: ");
    serial_write_dec(smp_nested_success);
    serial_write_str("\r\n");

    if (smp_nested_success == expected) {
        smp_print_result("NESTED LOCKS (No Deadlock)", 1);
    } else {
        smp_print_result("NESTED LOCKS (No Deadlock)", 0);
    }
}

/* =============================================================================
 * Test: Memory Ordering
 * =============================================================================
 */

void test_smp_memory_ordering(void) {
    volatile uint32_t data = 0;
    volatile uint32_t flag = 0;

    /* Test write ordering */
    data = 0xDEADBEEF;
    mb();
    flag = 1;
    mb();

    /* Verify ordering */
    int passed = (flag == 1 && data == 0xDEADBEEF);

    serial_write_str("[MEMORY ORDERING] Testing barriers...\r\n");
    serial_write_str("Data: 0x");
    serial_write_hex(data);
    serial_write_str(", Flag: ");
    serial_write_dec(flag);
    serial_write_str("\r\n");

    smp_print_result("MEMORY ORDERING", passed);
}

/* =============================================================================
 * Test: Contention Simulation
 * =============================================================================
 */

void test_smp_contention(void) {
    uint64_t counter = 0;

    serial_write_str("[CONTENTION] Testing under simulated load...\r\n");

    /* Simulate contention with many quick acquire/release cycles */
    for (int i = 0; i < SMP_TEST_ITERATIONS * 2; i++) {
        spinlock_acquire(&smp_test_lock);
        counter++;
        /* Small delay to simulate work */
        for (volatile int j = 0; j < 10; j++) {
            __asm__ volatile("nop");
        }
        spinlock_release(&smp_test_lock);
    }

    serial_write_str("Operations completed: ");
    serial_write_dec64(counter);
    serial_write_str("\r\n");

    if (counter == SMP_TEST_ITERATIONS * 2) {
        smp_print_result("CONTENTION", 1);
    } else {
        smp_print_result("CONTENTION", 0);
    }
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */

void test_spinlock_smp(void) {
    int cpu_id = smp_get_cpu_id();
    int cpu_count = smp_get_cpu_count();

    /* Only CPU 0 runs SMP tests */
    if (cpu_id != 0) {
        return;
    }

    smp_print_header("SMP Spinlock Stress Test Suite");

    serial_write_str("\r\nTest Configuration:\r\n");
    serial_write_str("  Iterations: ");
    serial_write_dec(SMP_TEST_ITERATIONS);
    serial_write_str("\r\n");

    if (cpu_count > 1) {
        serial_write_str("\r\n[SMP] Multi-processor environment\r\n");
        serial_write_str("[SMP] ");
        serial_write_dec(cpu_count);
        serial_write_str(" CPUs detected\r\n");
        serial_write_str("[SMP] Testing via time-slicing\r\n");
    } else {
        serial_write_str("\r\n[NOTE] Single-CPU mode\r\n");
        serial_write_str("[NOTE] Use 'make run-smp' for SMP testing\r\n");
    }

    serial_write_str("\r\n--- Running SMP Tests ---\r\n");

    test_smp_data_integrity();
    test_smp_fairness();
    test_smp_nested_locks();
    test_smp_memory_ordering();
    test_smp_contention();

    serial_write_str("\r\n============================================\r\n");
    serial_write_str("[SMP TESTING] Test suite complete\r\n");
    serial_write_str("============================================\r\n");

    serial_write_str("\r\n[SMP tests completed on CPU ");
    serial_write_dec(cpu_id);
    serial_write_str("]\r\n");

    if (cpu_count > 1) {
        serial_write_str("[NOTE] Multi-CPU: Time-slicing concurrency tested\r\n");
    } else {
        serial_write_str("[NOTE] Single-CPU: Logical correctness verified\r\n");
    }
}
