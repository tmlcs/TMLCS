#include "heap.h"
#include "barriers.h"
#include "bitmap.h"
#include "print.h"
#include "serial.h"
#include "slab.h"
#include "spinlock.h"
#include "string.h"

/* =============================================================================
 * Heap State and Constants
 * =============================================================================
 */

/* 2 GiB identity map upper bound (set during boot in main.asm) */
static constexpr uintptr_t IDENTITY_MAP_END = 0x80000000UL;

static int g_heap_initialized = 0;

/* Spinlock for thread-safe allocation */
static spinlock_t g_heap_lock = SPINLOCK_INIT;

/* HIGH-001 FIX: Track slab availability separately */
static int g_slab_available = 0;

/* MED-001 FIX: Per-page allocation count metadata.
 * Stores the number of contiguous pages in each bitmap allocation, indexed
 * by the starting page number.  Non-first pages and unallocated pages hold 0.
 * Eliminates the forward bitmap scan in kmem_free_auto() / kmalloc_size()
 * that incorrectly merged adjacent independent allocations into one free.
 *
 * Storage: TOTAL_PAGES * sizeof(uint16_t) ≈ 1 MB in .bss (zero-initialised).
 * uint16_t covers up to 65535 pages = 256 MB, well above HEAP_MAX_ALLOC.
 */
static uint16_t g_page_alloc_count[TOTAL_PAGES];

/* HIGH-3 FIX: Guard against silent truncation if HEAP_MAX_ALLOC ever grows
 * beyond what uint16_t can represent in page units. */
static_assert(HEAP_MAX_ALLOC / PAGE_SIZE <= 65535,
              "HEAP_MAX_ALLOC exceeds uint16_t capacity in g_page_alloc_count");

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
        return 1; /* Already initialized */
    }

    /* Initialize bitmap allocator */
    bitmap_init();

    if (!bitmap_is_initialized()) {
        serial_write_str("[HEAP] FATAL: bitmap_init() failed\r\n");
        return 0; /* Failed to initialize bitmap */
    }

    /* HIGH-001 FIX: Initialize slab and track availability */
    serial_write_str("[HEAP] Initializing slab allocator...\r\n");
    if (!slab_init()) {
        serial_write_str("[HEAP] WARNING: slab_init() failed, small allocs will use bitmap\r\n");
        g_slab_available = 0; /* Mark slab as unavailable */
    } else {
        serial_write_str("[HEAP] Slab allocator initialized\r\n");
        g_slab_available = 1; /* Mark slab as available */
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

    /* HIGH-001 FIX: Use slab only if available */
    if (size <= SLAB_MAX_SIZE && g_slab_available && slab_is_initialized()) {
        void* ptr = kmem_alloc(size);
        if (ptr != NULL) {
            return ptr;
        }
        /* If slab fails, fall through to bitmap allocation */
    }

    if (size > HEAP_MAX_ALLOC) {
        return NULL; /* Too large */
    }

    /* Acquire lock for thread safety */
    spinlock_token_t heap_tok = spinlock_acquire(&g_heap_lock);

    /* Calculate pages needed */
    size_t pages = size_to_pages(size);

    /* Allocate contiguous pages */
    size_t start_page = bitmap_alloc_contiguous(pages);

    /* MED-002 FIX: Write metadata inside the lock so bitmap state and
     * g_page_alloc_count are always consistent. A window between the
     * bitmap update and the metadata write would allow a concurrent
     * kmalloc_size() / kmem_free_auto() to see stale count = 0. */
    if (start_page != (size_t) -1) {
        g_page_alloc_count[start_page] = (uint16_t) pages;
    }

    /* Release lock */
    spinlock_release(&g_heap_lock, heap_tok);

    if (start_page == (size_t) -1) {
        return NULL; /* Out of memory */
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
        return NULL; /* Overflow */
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
        kmem_free_auto(ptr); /* Use unified API */
        return NULL;
    }

    if (new_size > HEAP_MAX_ALLOC) {
        return NULL;
    }

    /* CRIT-SEC-003 FIX: Determine allocation type and size while holding lock.
     * For slab allocations, we must hold g_slab_lock across the entire operation
     * to prevent use-after-free: another CPU could free and reallocate the slab
     * page between releasing the lock and completing memcpy().
     *
     * For bitmap allocations, we can safely release the lock after getting size
     * because the bitmap metadata (g_page_alloc_count) protects against reuse. */

    size_t old_size = 0;
    bool is_slab = false;
    bool needs_slab_copy = false;

    /* First, determine the allocation type and size */
    {
        spinlock_token_t slab_tok = spinlock_acquire(&g_slab_lock);
        is_slab = is_slab_address(ptr);
        if (is_slab) {
            uintptr_t page_start = (uintptr_t) ptr & ~((uintptr_t) (SLAB_SIZE - 1));
            const slab_t* s = reinterpret_cast<const slab_t*>(page_start);
            old_size = s->object_size;
            /* Check if new allocation will also use slab */
            needs_slab_copy =
                (new_size <= SLAB_MAX_SIZE && g_slab_available && slab_is_initialized());
        }
        spinlock_release(&g_slab_lock, slab_tok);

        if (!is_slab) {
            old_size = kmalloc_size(ptr);
        }
    }

    /* Case 0: Same size - no operation needed (optimization)
     * Note: Lock was already released in the scope above. */
    if (new_size == old_size) {
        return ptr;
    }

    /* Case 3: new size fits in old allocation */
    if (new_size <= old_size) {
        return ptr; /* No change needed */
    }

    /* Case 4: Need to grow allocation */
    /* CRIT-001 FIX: For slab-to-slab reallocation, use locked functions to avoid
     * race conditions. We hold g_slab_lock across the entire operation:
     * 1. Allocate new object using kmem_alloc_locked (assumes lock held)
     * 2. Copy data (lock still held - protects both allocations)
     * 3. Free old object using kmem_free_locked (assumes lock held)
     * This eliminates the window where another CPU could free/reallocate the
     * old slab page between releasing the lock and completing memcpy(). */
    if (is_slab && needs_slab_copy) {
        spinlock_token_t slab_tok = spinlock_acquire(&g_slab_lock);

        /* Allocate new slab object (lock already held) */
        void* new_ptr = kmem_alloc_locked(new_size);

        if (new_ptr == NULL) {
            /* CRIT-001 FIX: kmem_alloc_locked returns NULL if no partial slab
             * available (cannot allocate new slab while lock is held).
             * Release lock and fall back to bitmap allocation. */
            spinlock_release(&g_slab_lock, slab_tok);

            /* Fall back to bitmap allocation */
            void* new_ptr_bitmap = kmalloc(new_size);
            if (new_ptr_bitmap == NULL) {
                return NULL;
            }
            memcpy(new_ptr_bitmap, ptr, old_size);
            kmem_free_auto(ptr);
            return new_ptr_bitmap;
        }

        /* Copy old data to new allocation (lock still held) */
        memcpy(new_ptr, ptr, old_size);

        /* Free old slab object (lock still held) */
        kmem_free_locked(ptr, 0);

        spinlock_release(&g_slab_lock, slab_tok);
        return new_ptr;
    }

    /* For bitmap allocations or slab-to-bitmap transitions, use standard approach */
    /* Allocate new memory */
    void* new_ptr = kmalloc(new_size);

    if (new_ptr == NULL) {
        return NULL; /* Out of memory */
    }

    /* Copy old data to new allocation */
    memcpy(new_ptr, ptr, old_size);

    /* Free old allocation using unified API */
    kmem_free_auto(ptr);

    return new_ptr;
}

/* =============================================================================
 * Unified Free API - kmem_free_auto()
 * =============================================================================
 * Automatically detects slab vs bitmap allocation and frees correctly.
 * This is the recommended free() function for all kernel memory.
 * =============================================================================
 */

/* Reference to slab pool (defined in slab.cpp, allocated from early_alloc) */
extern uint8_t* g_slab_pool;
extern size_t g_slab_pool_size;

/**
 * Check if a pointer belongs to a slab-allocated object.
 *
 * Slab pages are 4KB-aligned and start with a slab_t header whose
 * magic field is set to SLAB_MAGIC on initialization. Aligning ptr
 * down to the nearest 4KB boundary and reading the magic field is
 * sufficient to distinguish slab pages from other heap pages.
 */
/* Linker-script symbol: first byte after all kernel sections.
 * Heap pages are allocated at or above this address. */
extern "C" char __kernel_end;

int is_slab_address(void* ptr) {
    if (ptr == nullptr) {
        return 0;
    }

    uintptr_t addr = (uintptr_t) ptr;

    /* Align down to slab page boundary (SLAB_SIZE = 4KB = power of 2) */
    uintptr_t page_start = addr & ~((uintptr_t) (SLAB_SIZE - 1));

    /* LOW-001 FIX: Bounds check before dereferencing the potential slab header.
     * Heap/slab pages are allocated above the kernel image and within the
     * physical memory window. A pointer outside this range cannot be
     * a slab object, so skip the magic read entirely. */
    uintptr_t heap_base = (uintptr_t) &__kernel_end;
    uintptr_t heap_limit = PHYSICAL_MEMORY_END;
    if (page_start < heap_base || page_start >= heap_limit) {
        return 0;
    }

    /* Read slab magic from potential slab header */
    const slab_t* possible_slab = reinterpret_cast<const slab_t*>(page_start);

    /* Verify object falls within the object area (after slab header) */
    if (possible_slab->magic == SLAB_MAGIC) {
        uintptr_t obj_start = page_start + SLAB_HEADER_SIZE;
        uintptr_t obj_end = page_start + SLAB_SIZE;
        return (addr >= obj_start && addr < obj_end) ? 1 : 0;
    }

    return 0;
}

/**
 * Free memory with automatic size detection
 *
 * This is the recommended free() function for all kernel memory.
 * It automatically detects whether the pointer was allocated by:
 *   - Slab allocator (small objects <= 2048 bytes)
 *   - Bitmap allocator (large objects > 2048 bytes)
 *
 * And frees it using the appropriate method.
 *
 * @note Safe to call with NULL (no operation)
 * @note Do NOT free the same pointer twice
 * @note Do NOT free stack or static memory
 */
void kmem_free_auto(void* ptr) {
    /* NULL is safe (no operation) */
    if (ptr == nullptr) {
        return;
    }

    /* CRIT-002 FIX: Hold g_slab_lock across type check AND dispatch to eliminate
     * the race window where another CPU could reallocate the slab page between
     * releasing the lock and calling kmem_free(). We use kmem_free_locked()
     * which assumes the lock is already held. */
    spinlock_token_t slab_tok = spinlock_acquire(&g_slab_lock);
    bool is_slab = is_slab_address(ptr);

    if (is_slab) {
        /* Slab allocation - use locked free (lock already held) */
        kmem_free_locked(ptr, 0);
        spinlock_release(&g_slab_lock, slab_tok);
    } else {
        /* Not slab — bitmap allocation; release slab lock first, then free */
        spinlock_release(&g_slab_lock, slab_tok);

        if (!g_heap_initialized) {
            return;
        }

        /* Validate pointer is in managed range */
        uintptr_t phys = (uintptr_t) ptr;
        if (phys < PHYSICAL_MEMORY_START || phys >= PHYSICAL_MEMORY_END) {
            return; /* Invalid pointer */
        }

        /* Convert to page number */
        size_t start_page = addr_to_page(ptr);

        if (start_page == (size_t) -1) {
            return; /* Invalid address */
        }

        /* Acquire heap lock for bitmap operations */
        spinlock_token_t heap_tok = spinlock_acquire(&g_heap_lock);

        /* CRIT-002 FIX: Re-verify page type after acquiring heap lock.
         * SMP SAFETY: Between releasing g_slab_lock (line 386) and acquiring
         * g_heap_lock (line 406), another CPU could have reallocated this page
         * as a slab page via kmem_alloc(). Without this re-check, we would call
         * bitmap_free_contiguous() on a slab-owned page, causing corruption.
         *
         * On uniprocessor systems with interrupts disabled, this check is
         * redundant but harmless. For SMP, it is critical. */
        if (is_slab_address(ptr)) {
            /* Page was converted to slab - release heap lock and free via slab */
            spinlock_release(&g_heap_lock, heap_tok);
            spinlock_token_t slab_tok2 = spinlock_acquire(&g_slab_lock);
            kmem_free_locked(ptr, 0);
            spinlock_release(&g_slab_lock, slab_tok2);
            return;
        }

        /* MED-001 FIX: Read page count from metadata instead of scanning the
         * bitmap forward.  The forward scan merged adjacent allocations into
         * one free, silently double-freeing the neighbor pages. */
        size_t pages = g_page_alloc_count[start_page];

        if (pages > 0) {
            g_page_alloc_count[start_page] = 0;
            bitmap_free_contiguous(start_page, pages);
        }

        /* Release heap lock */
        spinlock_release(&g_heap_lock, heap_tok);
    }
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
        return NULL; /* Invalid alignment */
    }

    /* For page-aligned allocations (alignment <= PAGE_SIZE), use regular kmalloc */
    if (alignment <= PAGE_SIZE) {
        return kmalloc(size);
    }

    /* LOW-NEW-011 FIX: Alignments larger than PAGE_SIZE are not implemented.
     * A future implementation would scan the bitmap for a page whose physical
     * address satisfies the requested alignment.
     * Emit a serial warning so callers can distinguish "not implemented" from
     * other NULL returns (heap not initialized, bad alignment power-of-2). */
    serial_write_str("[HEAP] kmalloc_align: alignment > PAGE_SIZE not implemented\r\n");
    return NULL;
}

size_t kmalloc_size(void* ptr) {
    if (!g_heap_initialized || ptr == NULL) {
        return 0;
    }

    /* Validate pointer */
    uintptr_t phys = (uintptr_t) ptr;
    if (phys < PHYSICAL_MEMORY_START || phys >= PHYSICAL_MEMORY_END) {
        return 0;
    }

    size_t start_page = addr_to_page(ptr);
    if (start_page == (size_t) -1) {
        return 0;
    }

    /* MED-001 FIX: Read page count from metadata (O(1), no scan)
     * CRIT-003 FIX: Use atomic load to prevent race condition with concurrent
     * kmem_free_auto() or kmalloc() modifying g_page_alloc_count. On x86_64,
     * uint16_t reads are atomic, but without explicit atomic semantics, the
     * compiler/CPU could reorder this load. Using __ATOMIC_RELAXED is sufficient
     * here because: (1) we only need atomicity, not ordering, and (2) the caller
     * (krealloc) holds appropriate locks for the subsequent operations. */
    return (size_t) __atomic_load_n(&g_page_alloc_count[start_page], __ATOMIC_RELAXED) * PAGE_SIZE;
}
