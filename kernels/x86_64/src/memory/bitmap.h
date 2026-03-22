#ifndef BITMAP_H
#define BITMAP_H

#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * GLOBEX_OS Bitmap Page Allocator
 * =============================================================================
 *
 * Bitmap-based physical page allocator for kernel memory management.
 * Tracks free/used pages using a bitmap where:
 *   - 0 = page is FREE
 *   - 1 = page is USED
 *
 * Features:
 *   - O(1) check if page is free/used
 *   - O(n) allocation (scans bitmap for free page)
 *   - O(1) deallocation (just clears bit)
 *   - Thread-safe via spinlock (when used with heap)
 *
 * Memory Layout:
 *   - Physical memory divided into 4KB pages
 *   - Bitmap tracks pages from KERNEL_LOAD_ADDRESS to end of mapped region
 *   - 2GiB mapped = 524,288 pages = 64KB bitmap
 *
 * Usage:
 *   @code
 *   bitmap_init();  // Initialize at kernel startup
 *
 *   size_t page = bitmap_alloc();  // Allocate one page
 *   bitmap_free(page);            // Free page
 *
 *   size_t pages = bitmap_alloc_contiguous(4);  // Allocate 4 contiguous pages
 *   bitmap_free_contiguous(pages, 4);           // Free 4 pages
 *   @endcode
 *
 * =============================================================================
 */

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/** Page size in bytes (4KB standard) */
#define PAGE_SIZE 0x1000

/** Physical memory start (1MB - kernel load address) */
#define PHYSICAL_MEMORY_START 0x100000

/** Physical memory end (2GiB mapped region) */
#define PHYSICAL_MEMORY_END 0x80000000

/** Total physical memory size in bytes (2GiB - 1MB) */
#define PHYSICAL_MEMORY_SIZE (PHYSICAL_MEMORY_END - PHYSICAL_MEMORY_START)

/** Total number of pages in managed region */
#define TOTAL_PAGES (PHYSICAL_MEMORY_SIZE / PAGE_SIZE)

/** Bitmap size in bytes (1 bit per page) */
#define BITMAP_SIZE_BYTES ((TOTAL_PAGES + 7) / 8)

/** Number of 64-bit words in bitmap */
#define BITMAP_WORDS ((BITMAP_SIZE_BYTES + 7) / 8)

/* =============================================================================
 * Bitmap Data Structure
 * =============================================================================
 */

/**
 * @brief Bitmap structure for page tracking
 *
 * The bitmap uses 1 bit per page:
 *   - 0 = FREE (available for allocation)
 *   - 1 = USED (currently allocated)
 *
 * Memory for bitmap itself is allocated in .bss section.
 * Size: ~64KB for 2GiB of physical memory.
 */
typedef struct {
    uint64_t words[BITMAP_WORDS];  /**< Bitmap words (524288 bits = 64KB) */
} bitmap_t;

/**
 * @brief Global bitmap instance
 * Allocated in .bss section (zero-initialized by boot code)
 */
extern bitmap_t g_page_bitmap;

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * @brief Initialize the bitmap allocator
 *
 * Must be called before any allocation.
 * Marks all pages as FREE initially.
 *
 * @note Called once at kernel startup
 * @note Must be called before heap_init()
 */
void bitmap_init(void);

/**
 * @brief Check if bitmap is initialized
 * @return 1 if initialized, 0 otherwise
 */
int bitmap_is_initialized(void);

/* =============================================================================
 * Core Allocation Functions
 * =============================================================================
 */

/**
 * @brief Allocate a single physical page
 * @return Page number (0 to TOTAL_PAGES-1), or -1 if no free pages
 *
 * Finds the first free page in the bitmap and marks it as used.
 * Uses first-fit strategy for simplicity.
 *
 * PERF-MEM-001: Uses __builtin_ctzll() for O(1) bit finding.
 * Performance: O(n) where n = number of 64-bit words (not bits)
 *
 * @note Returns page NUMBER, not address
 * @note Use page_to_addr() to convert to physical address
 *
 * @example
 *   size_t page = bitmap_alloc();
 *   if (page != (size_t)-1) {
 *       void* addr = page_to_addr(page);
 *       // Use memory...
 *   }
 */
size_t bitmap_alloc(void);

/**
 * @brief Allocate multiple contiguous physical pages
 * @param count Number of contiguous pages to allocate
 * @return Starting page number, or -1 if no contiguous region found
 *
 * Finds the first region of 'count' contiguous free pages.
 * Uses first-fit strategy.
 *
 * @note May be slow for large allocations (scans bitmap)
 * @note All pages in region are marked as used
 *
 * @example
 *   size_t page = bitmap_alloc_contiguous(4);  // 16KB contiguous
 *   if (page != (size_t)-1) {
 *       void* addr = page_to_addr(page);
 *       // Use 16KB contiguous memory...
 *   }
 */
size_t bitmap_alloc_contiguous(size_t count);

/**
 * @brief Free a single physical page
 * @param page Page number to free
 * @return 0 on success, -1 on error (invalid page or already free)
 *
 * Marks the specified page as free in the bitmap.
 *
 * @note Does NOT zero the page content
 * @note Caller must ensure page is not in use
 *
 * @example
 *   bitmap_free(page);  // Page can now be reallocated
 */
int bitmap_free(size_t page);

/**
 * @brief Free multiple contiguous physical pages
 * @param page Starting page number
 * @param count Number of pages to free (must be > 0)
 * @return 0 on success, -1 on error
 *
 * Marks all pages in the range [page, page+count) as free.
 *
 * FIX-MEM-004: Now validates count > 0 to prevent invalid frees.
 *
 * @note All pages must have been allocated together
 * @note Does NOT zero the page content
 * @note Returns -1 if count == 0 (invalid parameter)
 *
 * @example
 *   bitmap_free_contiguous(start_page, 4);  // Free 4 pages
 */
int bitmap_free_contiguous(size_t page, size_t count);

/* =============================================================================
 * Query Functions
 * =============================================================================
 */

/**
 * @brief Check if a page is currently allocated
 * @param page Page number to check
 * @return 1 if allocated, 0 if free, -1 if invalid page
 */
int bitmap_is_page_allocated(size_t page);

/**
 * @brief Count total free pages in the system
 * @return Number of free pages
 *
 * @note Scans entire bitmap - O(n) operation
 * @note Use sparingly (e.g., for debugging)
 */
size_t bitmap_count_free_pages(void);

/**
 * @brief Count total allocated pages in the system
 * @return Number of allocated pages
 *
 * @note Scans entire bitmap - O(n) operation
 */
size_t bitmap_count_allocated_pages(void);

/**
 * @brief Find the largest contiguous free region
 * @return Size of largest free region in pages
 *
 * @note Useful for fragmentation analysis
 * @note Scans entire bitmap - O(n) operation
 */
size_t bitmap_largest_free_region(void);

/* =============================================================================
 * Address Conversion Helpers
 * =============================================================================
 */

/**
 * @brief Convert page number to physical address
 * @param page Page number (0 to TOTAL_PAGES-1)
 * @return Physical address (page * PAGE_SIZE + PHYSICAL_MEMORY_START)
 *
 * @example
 *   size_t page = 100;
 *   void* addr = page_to_addr(page);  // 0x100000 + 100*0x1000
 */
static inline void* page_to_addr(size_t page) {
    return (void*)(PHYSICAL_MEMORY_START + page * PAGE_SIZE);
}

/**
 * @brief Convert physical address to page number
 * @param addr Physical address
 * @return Page number, or -1 if address is out of range
 *
 * @example
 *   void* addr = (void*)0x500000;  // 5MB
 *   size_t page = addr_to_page(addr);  // 4096 (5MB / 4KB)
 */
static inline size_t addr_to_page(void* addr) {
    uintptr_t phys = (uintptr_t)addr;
    if (phys < PHYSICAL_MEMORY_START || phys >= PHYSICAL_MEMORY_END) {
        return (size_t)-1;
    }
    return (phys - PHYSICAL_MEMORY_START) / PAGE_SIZE;
}

/* =============================================================================
 * Debug Functions
 * =============================================================================
 */

/**
 * @brief Print bitmap statistics to serial console
 *
 * Outputs:
 *   - Total pages
 *   - Free pages
 *   - Allocated pages
 *   - Fragmentation info
 *
 * @note For debugging only
 */
void bitmap_print_stats(void);

/**
 * @brief Dump a region of the bitmap to serial console
 * @param start_page Starting page number
 * @param count Number of pages to dump
 *
 * Shows allocation status as binary string:
 *   0 = free, 1 = allocated
 *
 * @note For debugging only
 */
void bitmap_dump_region(size_t start_page, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* BITMAP_H */
