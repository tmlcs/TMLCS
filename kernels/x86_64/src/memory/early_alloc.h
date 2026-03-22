#ifndef EARLY_ALLOC_H
#define EARLY_ALLOC_H

#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * GLOBEX_OS Early Boot Memory Allocator
 * =============================================================================
 *
 * Simple bump allocator for early boot memory allocation.
 * Used before the main heap is initialized.
 *
 * Features:
 *   - Simple bump pointer (no free)
 *   - Pre-allocated memory pool
 *   - Fast O(1) allocation
 *   - Alignment support
 *
 * Usage:
 *   @code
 *   early_alloc_init(pool, size);  // Set up memory pool
 *
 *   void* ptr = early_alloc(256);  // Allocate 256 bytes
 *   void* aligned = early_alloc_align(4096, 4096);  // 4KB aligned
 *   @endcode
 *
 * @note This is a ONE-WAY allocator - no free() function
 * @note Use only during early boot, before main heap is ready
 * @note After heap_init(), use kmalloc/kfree instead
 *
 * =============================================================================
 */

/* =============================================================================
 * Configuration
 * =============================================================================
 */

/** Default early allocation pool size (1MB) */
#define EARLY_ALLOC_POOL_SIZE (1024 * 1024)

/** Default alignment for early allocations */
#define EARLY_ALLOC_ALIGNMENT 8

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * @brief Initialize early allocator with memory pool
 * @param pool Pointer to memory pool (must be aligned)
 * @param size Size of pool in bytes
 * @return 1 on success, 0 on failure
 *
 * Must be called before any early_alloc() calls.
 * Pool should be in BSS or statically allocated memory.
 */
int early_alloc_init(void* pool, size_t size);

/**
 * @brief Initialize with default BSS pool
 * @return 1 on success, 0 on failure
 *
 * Uses internal BSS pool of EARLY_ALLOC_POOL_SIZE bytes.
 */
int early_alloc_init_default(void);

/**
 * @brief Check if early allocator is initialized
 * @return 1 if initialized, 0 otherwise
 */
int early_alloc_is_initialized(void);

/* =============================================================================
 * Allocation Functions
 * =============================================================================
 */

/**
 * @brief Allocate memory from early pool
 * @param size Size in bytes
 * @return Pointer to allocated memory, or NULL on failure
 *
 * Simple bump allocation - no free().
 * Memory is aligned to EARLY_ALLOC_ALIGNMENT.
 */
void* early_alloc(size_t size);

/**
 * @brief Allocate aligned memory from early pool
 * @param size Size in bytes
 * @param alignment Required alignment (must be power of 2)
 * @return Pointer to allocated memory, or NULL on failure
 *
 * Returns memory aligned to specified alignment.
 * Useful for page tables, DMA buffers, etc.
 */
void* early_alloc_align(size_t size, size_t alignment);

/**
 * @brief Get total allocated bytes
 * @return Number of bytes allocated so far
 */
size_t early_alloc_get_used(void);

/**
 * @brief Get remaining bytes in pool
 * @return Number of bytes remaining
 */
size_t early_alloc_get_free(void);

/**
 * @brief Get total pool size
 * @return Total pool size in bytes
 */
size_t early_alloc_get_pool_size(void);

/* =============================================================================
 * Debug Functions
 * =============================================================================
 */

/**
 * @brief Print early allocator statistics
 *
 * Outputs via serial:
 *   - Pool size
 *   - Used bytes
 *   - Free bytes
 *   - Utilization percentage
 */
void early_alloc_print_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* EARLY_ALLOC_H */
