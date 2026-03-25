#include "test_pit.h"
#include "pit.h"
#include "irq.h"
#include "serial.h"
#include "print.h"

/* =============================================================================
 * Test Framework Helpers
 * =============================================================================
 */

static int tests_passed = 0;
static int tests_failed = 0;

static void print_test_header(const char* name) {
    serial_write_str("\r\n--- ");
    serial_write_str(name);
    serial_write_str(" ---\r\n");
}

static void test_pass(const char* msg) {
    serial_write_str("  [PASS] ");
    serial_write_str(msg);
    serial_write_str("\r\n");
    tests_passed++;
}

static void test_fail(const char* msg) {
    serial_write_str("  [FAIL] ");
    serial_write_str(msg);
    serial_write_str("\r\n");
    tests_failed++;
}

/* =============================================================================
 * Test: PIT Initialization
 * =============================================================================
 */

void test_pit_init(void) {
    print_test_header("PIT Initialization");

    /* Test 1: Initialize with default frequency */
    int result = pit_init();
    if (result == 1) {
        test_pass("pit_init() succeeded");
    } else {
        test_fail("pit_init() failed");
        return;
    }

    /* Test 2: Check initialized flag */
    if (pit_is_initialized()) {
        test_pass("pit_is_initialized() returned true");
    } else {
        test_fail("pit_is_initialized() returned false");
    }

    /* Test 3: Initial tick count should be 0 */
    uint64_t ticks = pit_get_ticks();
    if (ticks == 0) {
        test_pass("Initial tick count is 0");
    } else {
        test_fail("Initial tick count should be 0");
    }
}

/* =============================================================================
 * Test: Frequency Changes
 * =============================================================================
 */

void test_pit_frequency(void) {
    print_test_header("Frequency Changes");

    /* Test 1: Change to 50 Hz */
    int result = pit_set_frequency(50);
    if (result == 1) {
        test_pass("pit_set_frequency(50) succeeded");
    } else {
        test_fail("pit_set_frequency(50) failed");
    }

    uint32_t freq = pit_get_frequency();
    if (freq == 50) {
        test_pass("Frequency changed to 50 Hz");
    } else {
        test_fail("Frequency not updated");
    }

    /* Test 2: Change to 200 Hz */
    result = pit_set_frequency(200);
    if (result == 1) {
        test_pass("pit_set_frequency(200) succeeded");
    } else {
        test_fail("pit_set_frequency(200) failed");
    }

    freq = pit_get_frequency();
    if (freq == 200) {
        test_pass("Frequency changed to 200 Hz");
    } else {
        test_fail("Frequency not updated");
    }

    /* Test 3: Invalid frequency (too high) */
    result = pit_set_frequency(2000);
    if (result == 0) {
        test_pass("Invalid frequency (2000 Hz) rejected");
    } else {
        test_fail("Invalid frequency should be rejected");
    }

    /* Test 4: Invalid frequency (zero) */
    result = pit_set_frequency(0);
    if (result == 0) {
        test_pass("Zero frequency rejected");
    } else {
        test_fail("Zero frequency should be rejected");
    }

    /* Restore default frequency */
    pit_set_frequency(100);
}

/* =============================================================================
 * Test: Tick Counting
 * =============================================================================
 */

void test_pit_ticks(void) {
    print_test_header("Tick Counting");

    /* Reset ticks */
    pit_reset_ticks();

    uint64_t initial_ticks = pit_get_ticks();
    if (initial_ticks == 0) {
        test_pass("Ticks reset to 0");
    } else {
        test_fail("Ticks not reset properly");
    }

    /* Wait for a few ticks */
    serial_write_str("  Waiting for ticks...\r\n");
    
    uint64_t timeout = 1000000;  /* Prevent infinite loop */
    uint64_t start_ticks = pit_get_ticks();
    
    while (pit_get_ticks() == start_ticks && timeout-- > 0) {
        __asm__ volatile("pause");
    }

    uint64_t current_ticks = pit_get_ticks();
    if (current_ticks > start_ticks) {
        test_pass("Ticks are incrementing");
        serial_write_str("    Start: ");
        serial_write_dec64(start_ticks);
        serial_write_str(", Current: ");
        serial_write_dec64(current_ticks);
        serial_write_str("\r\n");
    } else {
        test_fail("Ticks not incrementing");
    }

    /* Test milliseconds tracking */
    uint64_t ms = pit_get_milliseconds();
    if (ms > 0) {
        test_pass("Milliseconds tracking works");
        serial_write_str("    Milliseconds: ");
        serial_write_dec64(ms);
        serial_write_str("\r\n");
    } else {
        test_fail("Milliseconds not tracking");
    }
}

/* =============================================================================
 * Test: IRQ Registration
 * =============================================================================
 */

void test_pit_irq(void) {
    print_test_header("IRQ Registration");

    /* Test 1: Check IRQ system initialized */
    if (irq_is_initialized()) {
        test_pass("IRQ system is initialized");
    } else {
        test_fail("IRQ system not initialized");
    }

    /* Test 2: PIT handler registered (pit_irq_handler) */
    irq_handler_t handler = irq_get_handler(0);
    if (handler != nullptr) {
        test_pass("IRQ0 handler registered");
    } else {
        test_fail("IRQ0 handler not registered");
    }

    /* Test 3: Enable IRQ0 */
    irq_enable(0);
    if (irq_is_enabled(0)) {
        test_pass("IRQ0 enabled");
    } else {
        test_fail("IRQ0 not enabled");
    }

    /* Test 4: Check IRQ count increments */
    uint32_t initial_count = irq_get_count(0);
    
    /* Wait for a few ticks */
    pit_wait_ms(50);  /* Wait 50ms = ~5 ticks at 100Hz */
    
    uint32_t current_count = irq_get_count(0);
    if (current_count > initial_count) {
        test_pass("IRQ0 count incrementing");
        serial_write_str("    Initial: ");
        serial_write_dec(initial_count);
        serial_write_str(", Current: ");
        serial_write_dec(current_count);
        serial_write_str("\r\n");
    } else {
        test_fail("IRQ0 count not incrementing");
    }
}

/* =============================================================================
 * Test: Wait Functions
 * =============================================================================
 */

void test_pit_wait(void) {
    print_test_header("Wait Functions");

    /* Test 1: pit_wait_ms */
    uint64_t start_ms = pit_get_milliseconds();
    pit_wait_ms(10);
    uint64_t end_ms = pit_get_milliseconds();
    uint64_t elapsed = end_ms - start_ms;

    if (elapsed >= 8 && elapsed <= 20) {  /* Allow some tolerance */
        test_pass("pit_wait_ms(10) accurate");
        serial_write_str("    Elapsed: ");
        serial_write_dec64(elapsed);
        serial_write_str(" ms\r\n");
    } else {
        test_fail("pit_wait_ms(10) inaccurate");
    }

    /* Test 2: pit_wait_us (basic test) */
    /* Note: This is a rough test as us accuracy depends on calibration */
    uint32_t start_us_count = 0;  /* We don't have microsecond counter */
    (void)start_us_count;
    
    pit_wait_us(100);
    test_pass("pit_wait_us(100) completed");
}

/* =============================================================================
 * Test: PIT Statistics
 * =============================================================================
 */

void test_pit_stats(void) {
    print_test_header("PIT Statistics");

    /* Print statistics */
    pit_print_stats();

    /* Verify state structure */
    volatile pit_state_t* state = pit_get_state();
    if (state != nullptr) {
        test_pass("pit_get_state() returned valid pointer");
    } else {
        test_fail("pit_get_state() returned NULL");
    }

    if (state->initialized == 1) {
        test_pass("State shows initialized");
    } else {
        test_fail("State shows not initialized");
    }
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */

void test_pit_all(void) {
    serial_write_str("\r\n");
    serial_write_str("============================================\r\n");
    serial_write_str("GLOBEX_OS PIT (Timer) Test Suite\r\n");
    serial_write_str("============================================\r\n");

    /* Reset PIT state (IRQ system and handler already set up by kernel_main) */
    serial_write_str("Initializing PIT...\r\n");
    if (!pit_init()) {
        serial_write_str("[FAIL] pit_init() failed!\r\n");
        return;
    }
    serial_write_str("IRQ0 (timer) handler active\r\n");

    /* Run tests */
    test_pit_init();
    test_pit_frequency();
    test_pit_ticks();
    test_pit_irq();
    test_pit_wait();
    test_pit_stats();

    /* Summary */
    serial_write_str("\r\n============================================\r\n");
    serial_write_str("PIT Test Summary\r\n");
    serial_write_str("============================================\r\n");
    serial_write_str("Passed: ");
    serial_write_dec(tests_passed);
    serial_write_str("\r\n");
    serial_write_str("Failed: ");
    serial_write_dec(tests_failed);
    serial_write_str("\r\n");

    if (tests_failed == 0) {
        serial_write_str("\r\n[PIT] All tests PASSED!\r\n");
    } else {
        serial_write_str("\r\n[PIT] Some tests FAILED!\r\n");
    }
    serial_write_str("============================================\r\n");
}
