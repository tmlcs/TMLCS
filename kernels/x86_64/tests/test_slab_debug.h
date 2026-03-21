#ifndef TEST_SLAB_DEBUG_H
#define TEST_SLAB_DEBUG_H

#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Slab Allocator Debug Test Module
 * =============================================================================
 * Diagnostic tests for slab allocator corruption issue.
 * 
 * These tests help identify:
 *   - Memory pool corruption
 *   - Buffer overflows
 *   - Stack corruption
 *   - Serial output timing issues
 *   - Allocation pattern problems
 * =============================================================================
 */

/**
 * Run all slab allocator debug tests
 * Called from kernel_main() for debugging
 */
void test_slab_debug_all(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_SLAB_DEBUG_H */
