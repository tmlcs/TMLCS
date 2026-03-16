#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

/* ==========================================
 * Test Framework Header
 * ==========================================
 * Internal test framework for GLOBEX_OS kernel tests.
 * Provides common utilities and declarations for test modules.
 *
 * @note All test functions use serial output for logging
 * @note Tests are designed to run during kernel initialization
 * ========================================== */

#include "serial.h"
#include "print.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================
 * Test Failure State
 * ==========================================
 * Track test failure state to allow early abort.
 * This prevents cascading failures when a test continues after
 * an assertion failure.
 *
 * Usage:
 *   - Set to false at start of each test function
 *   - TEST_ASSERT sets to true on failure
 *   - TEST_ABORT_IF_FAILED() returns early if true
 * ========================================== */
extern bool g_test_failed;

/* ==========================================
 * Test Function Declaration Macro
 * ==========================================
 * Use this macro to declare test functions in headers.
 * Example: TEST_FUNC(my_test);
 * ========================================== */
#define TEST_FUNC(name) void name(void)

/* ==========================================
 * Test Result Macros
 * ==========================================
 * Use these macros to report test results.
 * ========================================== */
#define TEST_PASS(msg) serial_write_str("[PASS] "); serial_write_str(msg); serial_write_str("\r\n")
#define TEST_FAIL(msg) serial_write_str("[FAIL] "); serial_write_str(msg); serial_write_str("\r\n")

/* ==========================================
 * Test Control Macros
 * ==========================================
 * Macros to control test execution flow.
 *
 * TEST_RESET_FAILURE(): Reset failure state at start of test
 * TEST_ABORT_IF_FAILED(): Return early if test already failed
 * TEST_ABORT_IF_FALSE(cond): Return if condition is false (guard pattern)
 * ========================================== */

/**
 * @brief Reset test failure state at start of test
 * @note Call this at the beginning of each test function
 */
#define TEST_RESET_FAILURE() do { g_test_failed = false; } while(0)

/**
 * @brief Abort test early if a failure has already occurred
 * @note Prevents cascading failures from invalid state
 *
 * Usage:
 *   @code
 *   void test_something(void) {
 *       TEST_RESET_FAILURE();
 *       serial_write_str("=== Test Name ===\r\n");
 *
 *       TEST_ASSERT(condition1, "First check");
 *       TEST_ABORT_IF_FAILED();  // Stop if first check failed
 *
 *       TEST_ASSERT(condition2, "Second check");  // Won't run if first failed
 *       // ...
 *   }
 *   @endcode
 */
#define TEST_ABORT_IF_FAILED() do { \
    if (g_test_failed) { \
        serial_write_str("[ABORT] Test aborted due to prior failure\r\n"); \
        return; \
    } \
} while(0)

/**
 * @brief Guard pattern - abort if condition is false
 * @param cond Condition to check
 * @note Convenience wrapper for common guard pattern
 *
 * Usage:
 *   @code
 *   void test_ptr_safety(void) {
 *       TEST_RESET_FAILURE();
 *       void* ptr = get_resource();
 *       TEST_ABORT_IF_FALSE(ptr != NULL);  // Abort if NULL
 *       // ... rest of test assumes ptr is valid
 *   }
 *   @endcode
 */
#define TEST_ABORT_IF_FALSE(cond) do { \
    if (!(cond)) { \
        serial_write_str("[ABORT] Test aborted - required condition not met\r\n"); \
        return; \
    } \
} while(0)

/* ==========================================
 * Assert Macro
 * ==========================================
 * Assertion for tests that tracks failure state.
 * 
 * Changes from original:
 *   - Sets g_test_failed = true on failure
 *   - Allows caller to detect and abort on failure
 *   - Does NOT halt execution (kernel tests continue)
 *
 * Usage:
 *   @code
 *   void test_example(void) {
 *       TEST_RESET_FAILURE();
 *       
 *       TEST_ASSERT(ptr != NULL, "Pointer should not be NULL");
 *       TEST_ABORT_IF_FAILED();  // Optional: stop on first failure
 *       
 *       TEST_ASSERT(value == 42, "Value should be 42");
 *       // ... more tests
 *   }
 *   @endcode
 *
 * @note For backward compatibility, existing tests without
 *       TEST_ABORT_IF_FAILED() will continue to work as before
 *       (all assertions run, failures logged)
 * ========================================== */
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        TEST_FAIL(msg); \
        g_test_failed = true;  /* Track failure */ \
    } else { \
        TEST_PASS(msg); \
    } \
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* TEST_FRAMEWORK_H */
