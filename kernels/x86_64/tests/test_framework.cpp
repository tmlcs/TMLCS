/* ==========================================
 * Test Framework Implementation
 * ==========================================
 * Global variables and implementation for test framework.
 *
 * Added g_test_failed to track test failure state.
 * This allows tests to abort early when a critical assertion fails,
 * preventing cascading failures from invalid state.
 * ========================================== */

#include "test_framework.h"

/* ==========================================
 * Global Test Failure State
 * ==========================================
 * Track failure state across TEST_ASSERT calls.
 * 
 * Usage:
 *   - Initialized to false at kernel startup (BSS)
 *   - Each test function should call TEST_RESET_FAILURE() at start
 *   - TEST_ASSERT sets to true on failure
 *   - TEST_ABORT_IF_FAILED() checks and returns early if true
 * ========================================== */
bool g_test_failed = false;
