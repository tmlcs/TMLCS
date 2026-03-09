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
 * Assert Macro
 * ==========================================
 * Simple assertion for tests. Does not halt on failure,
 * just logs the result. Kernel tests continue on failure.
 * ========================================== */
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        TEST_FAIL(msg); \
    } else { \
        TEST_PASS(msg); \
    } \
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* TEST_FRAMEWORK_H */
