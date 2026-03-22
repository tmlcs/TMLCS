#ifndef TEST_SLAB_H
#define TEST_SLAB_H

#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Slab Allocator Test Module
 * =============================================================================
 * Tests for slab allocator (small object allocation).
 *
 * Tests cover:
 *   - Basic allocation/deallocation
 *   - Multiple cache sizes
 *   - Memory efficiency vs kmalloc
 *   - Cross-cache allocation
 *   - Stress testing
 *   - Statistics validation
 *
 * @note Requires slab_init() to be called before tests
 * @note Tests allocate many small objects
 * =============================================================================
 */

/**
 * Run all slab allocator tests
 * Called from kernel_main() test suite
 */
void test_slab_allocator(void);

/**
 * Test basic slab allocation
 * Verifies simple kmem_alloc/kmem_free
 */
void test_slab_basic(void);

/**
 * Test multiple cache sizes
 * Verifies all cache sizes work correctly
 */
void test_slab_cache_sizes(void);

/**
 * Test memory efficiency
 * Compares slab vs kmalloc memory usage
 */
void test_slab_efficiency(void);

/**
 * Test direct cache API
 * Verifies slab_alloc_XX/slab_free_XX functions
 */
void test_slab_direct_api(void);

/**
 * Test stress allocation
 * Many allocations and frees
 * ENABLED: 2026-03-21 - Fixed with serial_write_str timing delays
 */
void test_slab_stress(void);

/**
 * Test memory leak fix (FEAT-MEM-003)
 * Verifies kmem_free() properly reclaims memory
 */
void test_slab_memory_leak_fix(void);

/**
 * Test statistics
 * Verifies slab statistics are accurate
 * DISABLED: Causes serial corruption
 */
/* void test_slab_statistics(void); */

/**
 * Test kmem_free_auto() unified free API
 * Verifies automatic detection of slab vs bitmap allocations
 */
void test_kmem_free_auto(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_SLAB_H */
