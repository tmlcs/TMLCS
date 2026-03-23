#ifndef HEAP_H
#define HEAP_H

#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * GLOBEX_OS Kernel Heap Memory Allocator
 * =============================================================================
 *
 * Kernel heap memory allocation using bitmap page allocator.
 * Provides malloc/free-style API for kernel memory management.
 *
 * Features:
 *   - Page-aligned allocation (4KB minimum)
 *   - Variable size allocation (automatically rounds up to pages)
 *   - NULL pointer safety
 *   - No fragmentation (uses contiguous pages)
 *   - Thread-safe via spinlock
 *
 * Memory Model:
 *   - All allocations are page-aligned (4KB)
 *   - Minimum allocation: 4KB (1 page)
 *   - Maximum allocation: Limited by largest contiguous region
 *   - No automatic zeroing (use memset if needed)
 *
 * Usage:
 *   @code
 *   heap_init();  // Call once at kernel startup
 *
 *   // Allocate 1KB (actually gets 4KB due to page alignment)
 *   void* ptr = kmalloc(1024);
 *   if (ptr != NULL) {
 *       // Use memory...
 *       kmem_free_auto(ptr);
 *   }
 *
 *   // Allocate zeroed memory
 *   void* zeroed = kcalloc(10, sizeof(int));  // 40 bytes -> 4KB
 *
 *   // Reallocate
 *   ptr = krealloc(ptr, 8192);  // Now 8KB
 *   @endcode
 *
 * =============================================================================
 */

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/** Minimum allocation size (1 page = 4KB) */
#define HEAP_MIN_ALLOC 0x1000

/** Maximum allocation size (128MB = 32768 pages) */
#define HEAP_MAX_ALLOC (128 * 1024 * 1024)

/** Default heap alignment (page size) */
#define HEAP_ALIGNMENT 0x1000

/* =============================================================================
 * Allocation Functions
 * =============================================================================
 */

/**
 * @brief Initialize the kernel heap
 * @return 1 on success, 0 on failure
 *
 * Must be called before any kmalloc/kmem_free_auto calls.
 * Initializes the underlying bitmap allocator.
 *
 * @note Called once at kernel startup
 * @note Must be called after basic drivers (serial, vga) are initialized
 */
int heap_init(void);

/**
 * @brief Check if heap is initialized
 * @return 1 if initialized, 0 otherwise
 */
int heap_is_initialized(void);

/**
 * @brief Allocate memory from kernel heap
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 *
 * Allocates at least 'size' bytes from the kernel heap.
 * Memory is page-aligned (4KB aligned).
 *
 * Allocation behavior:
 *   - size <= 4KB: Allocates 1 page (4KB)
 *   - size <= 8KB: Allocates 2 pages (8KB)
 *   - etc.
 *
 * @note Memory is NOT zeroed - use kcalloc for zeroed allocation
 * @note Returns NULL if allocation fails (out of memory)
 * @note Minimum allocation is 4KB regardless of requested size
 *
 * @example
 *   void* buffer = kmalloc(1024);  // Actually gets 4KB
 *   if (buffer == NULL) {
 *       // Handle out of memory
 *   }
 */
void* kmalloc(size_t size);

/**
 * @brief Allocate zeroed memory from kernel heap
 * @param nmemb Number of elements
 * @param size Size of each element in bytes
 * @return Pointer to allocated zeroed memory, or NULL on failure
 *
 * Allocates nmemb * size bytes and zeros the memory.
 * Equivalent to kmalloc() followed by memset(..., 0, ...).
 *
 * @note Memory is guaranteed to be zeroed
 * @note Same alignment as kmalloc (page-aligned)
 *
 * @example
 *   int* array = kcalloc(100, sizeof(int));  // 400 bytes -> 4KB, zeroed
 */
void* kcalloc(size_t nmemb, size_t size);

/**
 * @brief Reallocate memory with new size
 * @param ptr Pointer to previously allocated memory (or NULL)
 * @param new_size New size in bytes
 * @return Pointer to reallocated memory, or NULL on failure
 *
 * Changes the size of the allocation pointed to by ptr.
 * May move the allocation to a new location.
 *
 * Behavior:
 *   - If ptr is NULL: Equivalent to kmalloc(new_size)
 *   - If new_size is 0: Equivalent to kmem_free_auto(ptr), returns NULL
 *   - If new_size > old_size: Old data is preserved
 *   - If new_size < old_size: Data is truncated (old data preserved)
 *
 * @note Old data is preserved up to min(old_size, new_size)
 * @note Original ptr is freed (don't use it after call)
 *
 * @example
 *   void* ptr = kmalloc(1024);
 *   // ... use ptr ...
 *   ptr = krealloc(ptr, 4096);  // Grow to 4KB
 */
void* krealloc(void* ptr, size_t new_size);

/**
 * @brief Free allocated memory (unified API)
 * @param ptr Pointer to memory to free (or NULL)
 *
 * Unified free function that automatically detects the allocation type:
 *   - Slab allocations (<= 2048 bytes): Freed to slab cache
 *   - Bitmap allocations (> 2048 bytes): Freed to bitmap allocator
 *
 * This is the recommended free() function for all kernel memory.
 *
 * Behavior:
 *   - If ptr is NULL: No operation (safe)
 *   - If ptr is invalid: May crash or be ignored
 *   - If ptr already freed: Undefined behavior (double-free)
 *
 * @note Safe to call with NULL (no operation)
 * @note Do NOT free the same pointer twice
 * @note Do NOT free stack or static memory
 *
 * @example
 *   // Small allocation (slab)
 *   void* small = kmem_alloc(64);
 *   kmem_free_auto(small);  // Automatically uses slab free
 *
 *   // Large allocation (bitmap)
 *   void* large = kmalloc(4096);
 *   kmem_free_auto(large);  // Automatically uses bitmap free
 */
void kmem_free_auto(void* ptr);

/**
 * @brief Check if a pointer belongs to slab memory pool
 * @param ptr Pointer to check
 * @return 1 if slab allocation, 0 if bitmap or invalid
 * @note Used internally by kmem_free_auto()
 * @note Also available for testing/debugging
 */
int is_slab_address(void* ptr);

/* =============================================================================
 * Query Functions
 * =============================================================================
 */

/**
 * @brief Get total free memory in bytes
 * @return Number of free bytes
 */
size_t heap_get_free_memory(void);

/**
 * @brief Get total allocated memory in bytes
 * @return Number of allocated bytes
 */
size_t heap_get_allocated_memory(void);

/**
 * @brief Get largest contiguous free block in bytes
 * @return Size of largest free block in bytes
 */
size_t heap_get_largest_free_block(void);

/**
 * @brief Print heap statistics to serial console
 *
 * Outputs:
 *   - Total memory
 *   - Free memory
 *   - Allocated memory
 *   - Fragmentation info
 *
 * @note For debugging only
 */
void heap_print_stats(void);

/* =============================================================================
 * Advanced Functions
 * =============================================================================
 */

/**
 * @brief Allocate memory with specific alignment
 * @param size Number of bytes to allocate
 * @param alignment Required alignment (must be power of 2)
 * @return Pointer to allocated memory, or NULL on failure
 *
 * Allocates memory with at least 'alignment' byte alignment.
 *
 * @note alignment must be power of 2
 * @note alignment <= PAGE_SIZE is guaranteed (default behavior)
 * @note alignment > PAGE_SIZE may fail if no suitable region
 *
 * @example
 *   void* dma_buffer = kmalloc_align(4096, 4096);  // 4KB aligned for DMA
 */
void* kmalloc_align(size_t size, size_t alignment);

/**
 * @brief Get the actual allocated size for a pointer
 * @param ptr Pointer to allocated memory
 * @return Actual allocated size in bytes, or 0 if invalid
 *
 * Returns the actual size allocated (may be larger than requested).
 *
 * @example
 *   void* ptr = kmalloc(1000);  // Requests 1000 bytes
 *   size_t actual = kmalloc_size(ptr);  // Returns 4096 (1 page)
 */
size_t kmalloc_size(void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* HEAP_H */
