#ifndef TEST_MEMORY_MANAGER_H
#define TEST_MEMORY_MANAGER_H

#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Memory Manager Test Module
 * =============================================================================
 * Tests for kernel heap memory allocation (kmalloc/kfree)
 * and bitmap page allocator.
 *
 * Tests cover:
 *   - Basic allocation/deallocation
 *   - Multiple allocations
 *   - Boundary conditions
 *   - NULL pointer handling
 *   - Memory corruption detection
 *   - Fragmentation handling
 *   - Large allocations
 *
 * @note Requires heap_init() to be called before tests
 * @note Tests may allocate significant memory - ensure enough free pages
 * =============================================================================
 */

/**
 * Run all memory manager tests
 * Called from kernel_main() test suite
 */
void test_memory_manager(void);

/**
 * Test basic kmalloc/kfree functionality
 * Verifies simple allocation and deallocation
 */
void test_kmalloc_basic(void);

/**
 * Test multiple allocations
 * Verifies multiple simultaneous allocations work correctly
 */
void test_kmalloc_multiple(void);

/**
 * Test kcalloc (zeroed allocation)
 * Verifies memory is properly zeroed
 */
void test_kcalloc_zeroed(void);

/**
 * Test krealloc (reallocation)
 * Verifies growing and shrinking allocations
 */
void test_krealloc(void);

/**
 * Test NULL pointer handling
 * Verifies functions handle NULL gracefully
 */
void test_kmalloc_null_handling(void);

/**
 * Test boundary conditions
 * Verifies edge cases (size 0, max size, etc.)
 */
void test_kmalloc_boundaries(void);

/**
 * Test memory integrity
 * Verifies allocated memory can be written/read correctly
 */
void test_memory_integrity(void);

/**
 * Test fragmentation handling
 * Verifies allocator handles fragmented memory
 */
void test_fragmentation(void);

/**
 * Test large allocations
 * Verifies multi-page allocations work correctly
 */
void test_large_allocations(void);

/**
 * Test bitmap allocator directly
 * Verifies low-level bitmap operations
 */
void test_bitmap_allocator(void);

/**
 * TEST-MEM-001: Stress test for memory allocator
 * Tests allocator under memory pressure with many alloc/free cycles
 */
void test_memory_stress(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_MEMORY_MANAGER_H */
