#include "heap.h"
#include "bitmap.h"
#include "slab.h"
#include "serial.h"
#include "print.h"
#include "string.h"
#include "spinlock.h"
#include "barriers.h"

/* =============================================================================
 * Heap State
 * =============================================================================
 */
static int g_heap_initialized = 0;

/* Spinlock for thread-safe allocation */
static spinlock_t g_heap_lock = SPINLOCK_INIT;

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * @brief Calculate number of pages needed for a size
 */
static inline size_t size_to_pages(size_t size) {
    return (size + PAGE_SIZE - 1) / PAGE_SIZE;
}

/**
 * @brief Check if value is power of 2
 */
static inline int is_power_of_2(size_t val) {
    return val && !(val & (val - 1));
}

/* =============================================================================
 * Initialization
 * =============================================================================
 */

int heap_init(void) {
    if (g_heap_initialized) {
        return 1;  /* Already initialized */
    }

    /* Initialize bitmap allocator */
    bitmap_init();

    if (!bitmap_is_initialized()) {
        return 0;  /* Failed to initialize bitmap */
    }

    /* Initialize slab allocator (for small objects) */
    serial_write_str("[HEAP] Initializing slab allocator...\r\n");
    if (!slab_init()) {
        serial_write_str("[HEAP] Warning: slab_init() failed, continuing without slab\r\n");
    } else {
        serial_write_str("[HEAP] Slab allocator initialized\r\n");
    }

    g_heap_initialized = 1;
    mb();

    return 1;
}

int heap_is_initialized(void) {
    rmb();
    return g_heap_initialized;
}

/* =============================================================================
 * Core Allocation Functions
 * =============================================================================
 */

void* kmalloc(size_t size) {
    if (!g_heap_initialized) {
        return NULL;
    }

    if (size == 0) {
        return NULL;
    }

    /* Use slab allocator for small objects (<= 2KB) */
    if (size <= SLAB_MAX_SIZE && slab_is_initialized()) {
        void* ptr = kmem_alloc(size);
        if (ptr != NULL) {
            return ptr;
        }
        /* If slab fails, fall through to bitmap allocation */
    }

    if (size > HEAP_MAX_ALLOC) {
        return NULL;  /* Too large */
    }

    /* Acquire lock for thread safety */
    spinlock_acquire(&g_heap_lock);

    /* Calculate pages needed */
    size_t pages = size_to_pages(size);

    /* Allocate contiguous pages */
    size_t start_page = bitmap_alloc_contiguous(pages);

    /* Release lock */
    spinlock_release(&g_heap_lock);

    if (start_page == (size_t)-1) {
        return NULL;  /* Out of memory */
    }

    /* Convert page number to address */
    void* ptr = page_to_addr(start_page);

    return ptr;
}

void* kcalloc(size_t nmemb, size_t size) {
    /* Calculate total size with overflow check */
    size_t total_size = nmemb * size;
    
    /* Check for overflow */
    if (nmemb != 0 && total_size / nmemb != size) {
        return NULL;  /* Overflow */
    }
    
    /* Allocate memory */
    void* ptr = kmalloc(total_size);
    
    if (ptr != NULL) {
        /* Zero the memory */
        memset(ptr, 0, total_size);
    }
    
    return ptr;
}

void* krealloc(void* ptr, size_t new_size) {
    if (!g_heap_initialized) {
        return NULL;
    }
    
    /* Case 1: ptr is NULL - allocate new */
    if (ptr == NULL) {
        return kmalloc(new_size);
    }
    
    /* Case 2: new_size is 0 - free and return NULL */
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    if (new_size > HEAP_MAX_ALLOC) {
        return NULL;
    }
    
    /* Get old size */
    size_t old_size = kmalloc_size(ptr);
    
    /* Case 3: new size fits in old allocation */
    if (new_size <= old_size) {
        return ptr;  /* No change needed */
    }
    
    /* Case 4: Need to grow allocation */
    /* Allocate new memory */
    void* new_ptr = kmalloc(new_size);
    
    if (new_ptr == NULL) {
        return NULL;  /* Out of memory */
    }
    
    /* Copy old data to new allocation */
    memcpy(new_ptr, ptr, old_size);
    
    /* Free old allocation */
    kfree(ptr);
    
    return new_ptr;
}

void kfree(void* ptr) {
    if (!g_heap_initialized || ptr == NULL) {
        return;
    }

    /* Note: For slab allocations (size <= 2048), users should use kmem_free()
       kfree() is for large allocations only (bitmap-based) */

    /* Validate pointer is in managed range */
    uintptr_t phys = (uintptr_t)ptr;
    if (phys < PHYSICAL_MEMORY_START || phys >= PHYSICAL_MEMORY_END) {
        return;  /* Invalid pointer */
    }

    /* Convert to page number */
    size_t start_page = addr_to_page(ptr);

    if (start_page == (size_t)-1) {
        return;  /* Invalid address */
    }

    /* Acquire lock */
    spinlock_acquire(&g_heap_lock);

    /* Calculate number of pages */
    /* We need to find how many contiguous pages are allocated starting here */
    size_t pages = 0;
    while (bitmap_is_page_allocated(start_page + pages) == 1) {
        pages++;
    }

    /* Free all pages */
    if (pages > 0) {
        bitmap_free_contiguous(start_page, pages);
    }

    /* Release lock */
    spinlock_release(&g_heap_lock);
}

/* =============================================================================
 * Query Functions
 * =============================================================================
 */

size_t heap_get_free_memory(void) {
    if (!g_heap_initialized) {
        return 0;
    }
    return bitmap_count_free_pages() * PAGE_SIZE;
}

size_t heap_get_allocated_memory(void) {
    if (!g_heap_initialized) {
        return 0;
    }
    return bitmap_count_allocated_pages() * PAGE_SIZE;
}

size_t heap_get_largest_free_block(void) {
    if (!g_heap_initialized) {
        return 0;
    }
    return bitmap_largest_free_region() * PAGE_SIZE;
}

void heap_print_stats(void) {
    if (!g_heap_initialized) {
        serial_write_str("[HEAP] Not initialized\r\n");
        return;
    }
    
    size_t free_mem = heap_get_free_memory();
    size_t alloc_mem = heap_get_allocated_memory();
    size_t total_mem = PHYSICAL_MEMORY_SIZE;
    size_t largest = heap_get_largest_free_block();
    
    serial_write_str("\r\n=== Heap Statistics ===\r\n");
    
    serial_write_str("Total memory:     ");
    serial_write_dec(total_mem / 1024 / 1024);
    serial_write_str(" MB\r\n");
    
    serial_write_str("Free memory:      ");
    serial_write_dec(free_mem / 1024 / 1024);
    serial_write_str(" MB (");
    serial_write_dec(free_mem / 1024);
    serial_write_str(" KB)\r\n");
    
    serial_write_str("Allocated memory: ");
    serial_write_dec(alloc_mem / 1024 / 1024);
    serial_write_str(" MB (");
    serial_write_dec(alloc_mem / 1024);
    serial_write_str(" KB)\r\n");
    
    serial_write_str("Largest block:    ");
    serial_write_dec(largest / 1024 / 1024);
    serial_write_str(" MB (");
    serial_write_dec(largest / 1024);
    serial_write_str(" KB)\r\n");
    
    serial_write_str("Usage:            ");
    if (total_mem > 0) {
        size_t usage = alloc_mem * 100 / total_mem;
        serial_write_dec(usage);
        serial_write_str("%\r\n");
    } else {
        serial_write_str("N/A\r\n");
    }
    
    serial_write_str("=======================\r\n");
    
    /* Also print bitmap stats */
    bitmap_print_stats();
}

/* =============================================================================
 * Advanced Functions
 * =============================================================================
 */

void* kmalloc_align(size_t size, size_t alignment) {
    if (!g_heap_initialized) {
        return NULL;
    }
    
    if (!is_power_of_2(alignment)) {
        return NULL;  /* Invalid alignment */
    }
    
    /* For page-aligned allocations (alignment <= PAGE_SIZE), use regular kmalloc */
    if (alignment <= PAGE_SIZE) {
        return kmalloc(size);
    }
    
    /* For larger alignments, we need special handling */
    /* This is a simplified implementation */

    size_t pages = size_to_pages(size);

    /* Use bitmap_alloc_contiguous which handles allocation internally */
    /* For aligned allocation, we may need to try multiple times */
    /* This is a simplified approach - just use regular allocation for now */
    spinlock_acquire(&g_heap_lock);
    size_t start_page = bitmap_alloc_contiguous(pages);
    spinlock_release(&g_heap_lock);

    if (start_page == (size_t)-1) {
        return NULL;
    }

    return page_to_addr(start_page);
}

size_t kmalloc_size(void* ptr) {
    if (!g_heap_initialized || ptr == NULL) {
        return 0;
    }
    
    /* Validate pointer */
    uintptr_t phys = (uintptr_t)ptr;
    if (phys < PHYSICAL_MEMORY_START || phys >= PHYSICAL_MEMORY_END) {
        return 0;
    }
    
    size_t start_page = addr_to_page(ptr);
    if (start_page == (size_t)-1) {
        return 0;
    }
    
    /* Count contiguous allocated pages */
    size_t pages = 0;
    while (bitmap_is_page_allocated(start_page + pages) == 1) {
        pages++;
    }
    
    return pages * PAGE_SIZE;
}
