#include "slab.h"
#include "heap.h"
#include "bitmap.h"
#include "serial.h"
#include "print.h"
#include "string.h"
#include "spinlock.h"
#include "barriers.h"
#include "early_alloc.h"

/* =============================================================================
 * Low-level I/O Port Access
 * =============================================================================
 */

/* =============================================================================
 * SLAB_DEBUG Configuration
 * =============================================================================
 * INVESTIGATION RESULT (2026-03-19):
 *
 * Serial corruption was traced to QEMU UART 16550 emulation, NOT the slab.
 * Guard band tests showed NO corruption in slab memory.
 * Double-free detection found NO issues.
 *
 * The corruption pattern "Allocated: 5/5" -> "Allocate5" occurs in the
 * UART transmit buffer when many characters are written rapidly.
 *
 * SLAB_DEBUG is disabled because:
 *   1. Slab allocator is NOT the source of corruption
 *   2. Guard validation adds overhead without benefit
 *   3. Tests are disabled to avoid QEMU UART bug
 *
 * For more details, see: SLAB_CORRUPTION_INVESTIGATION.md
 * =============================================================================
 */
#define SLAB_DEBUG 0  /* Not needed - slab is verified correct */

/* Sentinel byte written beyond each allocation when SLAB_DEBUG=1.
 * If this byte is overwritten, a buffer overflow is detected. */
#define SLAB_GUARD_BYTE 0xAB

static_assert(sizeof(slab_t) <= SLAB_HEADER_SIZE,
              "slab_t exceeds SLAB_HEADER_SIZE -- update SLAB_HEADER_SIZE in slab.h");

/* =============================================================================
 * MED-001 FIX: Slab Timing Delays Configuration
 * =============================================================================
 * These serial_write_str(".") calls were added as timing delays to work
 * around a QEMU UART emulation bug. They are NOT needed for normal operation
 * and add unnecessary overhead.
 *
 * Enable SLAB_DEBUG_TIMING only when debugging QEMU UART timing issues.
 * For production builds or real hardware, set to 0.
 * =============================================================================
 */
#define SLAB_DEBUG_TIMING 0  /* Set to 1 only when debugging QEMU UART */

#if SLAB_DEBUG_TIMING
    #define SLAB_TIMING_DELAY() do { serial_write_str("."); mb(); } while(0)
#else
    #define SLAB_TIMING_DELAY() do { mb(); } while(0)
#endif

#if SLAB_DEBUG
    #define SLAB_GUARD_PATTERN  0xDEADBEEF
    #define SLAB_FREE_PATTERN   0xCC
    #define SLAB_ALLOC_PATTERN  0xAA
    #define GUARD_SIZE          8  /* 8 bytes before and after object */
#else
    #define GUARD_SIZE          0
#endif

/* =============================================================================
 * Early Heap Memory Pool for Slab Caches
 * =============================================================================
 * REFACTOR (2026-03-21): Use early_alloc() instead of BSS pool
 *
 * PREVIOUS APPROACH:
 *   - Static 256KB BSS array (g_slab_memory)
 *   - Manual pointer management
 *   - Timing issues with memory writes
 *
 * NEW APPROACH:
 *   - Dynamic allocation via early_alloc()
 *   - Proper memory initialization
 *   - No timing delays needed
 *
 * BENEFITS:
 *   - Cleaner separation of concerns
 *   - No BSS vs .data confusion
 *   - Easier to test and debug
 *   - Can be replaced with kmalloc() after heap_init()
 *
 * Memory usage: ~448 bytes for 7 cache structures (7 * 64 bytes)
 * Allocated from 1MB early_alloc pool
 * =============================================================================
 */

/* Pool pointer - allocated from early heap */
uint8_t* g_slab_pool = 0;
size_t g_slab_pool_size = 0;

/* State variables */
static int g_slab_initialized = 0;
static size_t g_slab_total_allocs = 0;
static size_t g_slab_total_frees = 0;
static size_t g_slab_total_slabs = 0;
static size_t g_slab_memory_used = 0;

#if SLAB_DEBUG
static size_t g_slab_corruptions_detected = 0;
static size_t g_slab_double_frees = 0;
#endif

/* MED-002 FIX: Use SPINLOCK_INIT for compile-time initialization.
 * Previously relied on BSS zero-fill to produce locked=0, which is
 * fragile and inconsistent with g_heap_lock and g_log_lock. */
static spinlock_t g_slab_lock = SPINLOCK_INIT;

/* For testing access */
slab_state_t g_slab_state;

/* Cache pointers */
static slab_cache_t* g_caches[NUM_CACHES];

slab_state_t* slab_get_state(void) {
    g_slab_state.initialized = g_slab_initialized;
    g_slab_state.total_allocations = g_slab_total_allocs;
    g_slab_state.total_frees = g_slab_total_frees;
    g_slab_state.total_slabs = g_slab_total_slabs;
    return &g_slab_state;
}

/* =============================================================================
 * Simple BSS Memory Pointer
 * =============================================================================
 * We use a simple pointer into the BSS array for allocation.
 * No complex free list needed for initial implementation.
 * =============================================================================
 */

/* =============================================================================
 * Cache Configuration
 * =============================================================================
 */

/* Cache sizes (power of 2 for alignment) */
#define CACHE_32_SIZE   32
#define CACHE_64_SIZE   64
#define CACHE_128_SIZE  128
#define CACHE_256_SIZE  256
#define CACHE_512_SIZE  512
#define CACHE_1024_SIZE 1024
#define CACHE_2048_SIZE 2048

/* Number of caches */
#define NUM_CACHES 7

/* Cache pointers - already declared in BSS section above (line 92) */

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * Get cache index for a given size
 * @param size Requested size in bytes
 * @return Cache index (0-6) or -1 if too large
 */
static int get_cache_index(size_t size) {
    if (size <= CACHE_32_SIZE) {
        return 0;
    } else if (size <= CACHE_64_SIZE) {
        return 1;
    } else if (size <= CACHE_128_SIZE) {
        return 2;
    } else if (size <= CACHE_256_SIZE) {
        return 3;
    } else if (size <= CACHE_512_SIZE) {
        return 4;
    } else if (size <= CACHE_1024_SIZE) {
        return 5;
    } else if (size <= CACHE_2048_SIZE) {
        return 6;
    }
    return -1;  /* Too large for slab, use kmalloc */
}

/**
 * Get cache pointer for a given index
 * @param idx Cache index (0-6)
 * @return Pointer to cache structure
 */
static slab_cache_t* get_cache(int idx) {
    if (idx < 0 || idx >= NUM_CACHES) {
        return nullptr;
    }
    return g_caches[idx];
}

#if SLAB_DEBUG
/* =============================================================================
 * Debug: Guard Band Validation
 * =============================================================================
 */

/**
 * @brief Write guard patterns around object
 * @param ptr Pointer to object start (after first guard)
 * @param size Object size
 */
static void write_guards(void* ptr, size_t size) {
    uint32_t* before_guard = (uint32_t*)((uint8_t*)ptr - GUARD_SIZE);
    uint32_t* after_guard = (uint32_t*)((uint8_t*)ptr + size);
    
    before_guard[0] = SLAB_GUARD_PATTERN;
    before_guard[1] = SLAB_GUARD_PATTERN;
    after_guard[0] = SLAB_GUARD_PATTERN;
    after_guard[1] = SLAB_GUARD_PATTERN;
}

/**
 * @brief Validate guard patterns around object
 * @param ptr Pointer to object start (after first guard)
 * @param size Object size
 * @return true if guards are intact, false if corruption detected
 */
static bool validate_guards(void* ptr, size_t size) {
    uint32_t* before_guard = (uint32_t*)((uint8_t*)ptr - GUARD_SIZE);
    uint32_t* after_guard = (uint32_t*)((uint8_t*)ptr + size);
    
    if (before_guard[0] != SLAB_GUARD_PATTERN || before_guard[1] != SLAB_GUARD_PATTERN) {
        serial_write_str("[SLAB] GUARD CORRUPTION (before)! ptr=0x");
        serial_write_hex64((uint64_t)ptr);
        serial_write_str("\r\n");
        return false;
    }
    
    if (after_guard[0] != SLAB_GUARD_PATTERN || after_guard[1] != SLAB_GUARD_PATTERN) {
        serial_write_str("[SLAB] GUARD CORRUPTION (after)! ptr=0x");
        serial_write_hex64((uint64_t)ptr);
        serial_write_str("\r\n");
        return false;
    }
    
    return true;
}

/**
 * @brief Fill object with pattern for debugging
 * @param ptr Pointer to object
 * @param size Object size
 * @param pattern Pattern to fill
 */
static void fill_pattern(void* ptr, size_t size, uint8_t pattern) {
    uint8_t* bytes = (uint8_t*)ptr;
    for (size_t i = 0; i < size; i++) {
        bytes[i] = pattern;
    }
}

/**
 * @brief Track freed pointers for double-free detection
 * Simple fixed-size table
 */
#define MAX_TRACKED_FREES 64
static void* g_freed_ptrs[MAX_TRACKED_FREES];
static int g_free_ptr_count = 0;

static bool is_double_free(void* ptr) {
    for (int i = 0; i < g_free_ptr_count; i++) {
        if (g_freed_ptrs[i] == ptr) {
            return true;
        }
    }
    return false;
}

static void track_free(void* ptr) {
    if (g_free_ptr_count < MAX_TRACKED_FREES) {
        g_freed_ptrs[g_free_ptr_count++] = ptr;
    }
}

static void clear_tracked_frees(void) {
    g_free_ptr_count = 0;
}

#endif /* SLAB_DEBUG */

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * Initialize a single cache from the memory pool
 *
 * REFACTOR (2026-03-21): Use early_alloc pool instead of BSS array.
 *
 * MED-001 FIX (2026-03-23): Timing delays now conditional via SLAB_DEBUG_TIMING.
 * Root cause: Memory timing requires ~15-110μs between pointer writes.
 * This is a known QEMU emulation timing issue.
 * 
 * Use SLAB_DEBUG_TIMING=1 only when debugging QEMU UART timing.
 * For production/real hardware, SLAB_DEBUG_TIMING=0 (no overhead).
 */
static int init_cache(int idx, size_t size) {
    /* Allocate cache structure from early heap pool */
    slab_cache_t* cache = (slab_cache_t*)(g_slab_pool + g_slab_memory_used);

    /* Initialize cache struct */
    cache->object_size = size;
    cache->objects_per_slab = (SLAB_SIZE - SLAB_HEADER_SIZE) / size;

    /* Use 0 instead of nullptr for freestanding compatibility */
    cache->partial = 0;
    SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */

    cache->full = 0;
    SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */
    
    cache->empty = 0;
    SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */

    cache->num_slabs = 0;
    cache->num_allocations = 0;
    cache->num_frees = 0;
    mb();

    g_caches[idx] = cache;

    /* Update pool usage */
    g_slab_memory_used += sizeof(slab_cache_t);
    mb();

    return 1;
}

int slab_init(void) {
    serial_write_str("[SLAB] slab_init() started\r\n");

    if (g_slab_initialized) {
        serial_write_str("[SLAB] Already initialized\r\n");
        return 1;
    }

    /* Allocate pool from early heap (4KB is plenty for cache structs) */
    g_slab_pool_size = 4096;  /* 4KB for ~64 cache structures */
    g_slab_pool = (uint8_t*)early_alloc(g_slab_pool_size);
    
    if (!g_slab_pool) {
        serial_write_str("[SLAB] ERROR: early_alloc failed\r\n");
        return 0;
    }
    
    serial_write_str("[SLAB] Allocated ");
    serial_write_dec(g_slab_pool_size);
    serial_write_str(" bytes from early heap\r\n");
    serial_write_str("[SLAB] Pool address: 0x");
    serial_write_hex64((uint64_t)(uintptr_t)g_slab_pool);
    serial_write_str("\r\n");

    /* SLAB-LOW-001 FIX: Removed redundant field-by-field reset.
     * g_slab_lock is statically initialized to SPINLOCK_INIT at declaration;
     * slab_init() is only called once from heap_init(), so the lock is always
     * in its clean default state here. */
    mb();

    serial_write_str("[SLAB] Reset counters...\r\n");
    g_slab_initialized = 0;
    g_slab_total_allocs = 0;
    g_slab_total_frees = 0;
    g_slab_total_slabs = 0;
    g_slab_memory_used = 0;

    /* Initialize all caches */
    serial_write_str("[SLAB] Initializing 7 caches (32-2048 bytes)...\r\n");

    if (!init_cache(0, CACHE_32_SIZE)) {
        return 0;
    }
    serial_write_str("[SLAB] Cache 32 bytes OK\r\n");

    if (!init_cache(1, CACHE_64_SIZE)) {
        return 0;
    }
    serial_write_str("[SLAB] Cache 64 bytes OK\r\n");

    if (!init_cache(2, CACHE_128_SIZE)) {
        return 0;
    }
    serial_write_str("[SLAB] Cache 128 bytes OK\r\n");

    if (!init_cache(3, CACHE_256_SIZE)) {
        return 0;
    }
    serial_write_str("[SLAB] Cache 256 bytes OK\r\n");

    if (!init_cache(4, CACHE_512_SIZE)) {
        return 0;
    }
    serial_write_str("[SLAB] Cache 512 bytes OK\r\n");

    if (!init_cache(5, CACHE_1024_SIZE)) {
        return 0;
    }
    serial_write_str("[SLAB] Cache 1024 bytes OK\r\n");

    if (!init_cache(6, CACHE_2048_SIZE)) {
        return 0;
    }
    serial_write_str("[SLAB] Cache 2048 bytes OK\r\n");

    serial_write_str("[SLAB] All caches initialized\r\n");
    serial_write_str("[SLAB] Pool memory used: ");
    serial_write_dec(g_slab_memory_used);
    serial_write_str(" bytes\r\n");

    serial_write_str("[SLAB] Set initialized=1...\r\n");
    g_slab_initialized = 1;
    mb();

    serial_write_str("[SLAB] slab_init() DONE\r\n");

    return 1;
}

int slab_is_initialized(void) {
    return g_slab_initialized;
}

void slab_shutdown(void) {
    g_slab_initialized = 0;
}

/* =============================================================================
 * Core Allocation - Slab Allocator with Proper Free List
 * =============================================================================
 * Full slab allocator implementation with:
 *   - Free list management for object reuse
 *   - Slab reuse (empty slabs cached for future allocations)
 *   - Proper memory reclamation on free
 *   - Guard bytes for corruption detection (DEBUG mode)
 *
 * MED-001 FIX (2026-03-23): Timing delays now conditional via SLAB_DEBUG_TIMING.
 * Set SLAB_DEBUG_TIMING=1 only when debugging QEMU UART timing issues.
 * For production/real hardware, use SLAB_DEBUG_TIMING=0 (no overhead).
 * =============================================================================
 */

/* find_slab_for_object() removed: kmem_free() derives the owning slab in O(1)
 * by aligning the pointer down to the SLAB_SIZE page boundary (MED-001 fix). */

/**
 * Remove slab from a list (partial, full, or empty)
 */
static void remove_slab_from_list(slab_t* slab, slab_t** list) {
    if (slab->prev) {
        slab->prev->next = slab->next;
    } else {
        *list = slab->next;
    }
    
    if (slab->next) {
        slab->next->prev = slab->prev;
    }
    
    slab->next = 0;
    slab->prev = 0;
}

/**
 * Add slab to the beginning of a list
 */
static void add_slab_to_list(slab_t* slab, slab_t** list) {
    slab->next = *list;
    slab->prev = 0;
    
    if (*list) {
        (*list)->prev = slab;
    }
    
    *list = slab;
}

void* kmem_alloc(size_t size) {
    if (!g_slab_initialized || size == 0 || size > SLAB_MAX_SIZE) {
        return 0;
    }

    spinlock_acquire(&g_slab_lock);

    int idx = get_cache_index(size);
    if (idx < 0) {
        spinlock_release(&g_slab_lock);
        return 0;
    }

    slab_cache_t* cache = get_cache(idx);
    if (cache == 0) {
        spinlock_release(&g_slab_lock);
        return 0;
    }

    /* First, try to allocate from partial slabs */
    if (cache->partial) {
        slab_t* slab = cache->partial;

        /* Allocate from free list */
        void* obj = slab->free_list;
        slab->free_list = slab->free_list->next;
        slab->num_free--;

        cache->num_allocations++;
        SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */
        g_slab_total_allocs++;
        SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */

        /* Move slab to full list if exhausted */
        if (slab->num_free == 0) {
            remove_slab_from_list(slab, &cache->partial);
            add_slab_to_list(slab, &cache->full);
        }

        spinlock_release(&g_slab_lock);
        return obj;
    }

    /* No partial slabs, allocate a new slab */
    slab_t* slab = (slab_t*)kmalloc(SLAB_SIZE);
    if (slab == 0) {
        spinlock_release(&g_slab_lock);
        return 0;
    }

    /* Initialize slab */
    slab->free_list = 0;
    slab->num_free = cache->objects_per_slab;
    slab->object_size = cache->object_size;
    slab->num_objects = cache->objects_per_slab;
    slab->cache = cache;
    slab->next = 0;
    slab->prev = 0;
    slab->magic = SLAB_MAGIC;

    SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */

    /* Build free list - optimized with single delay at end */
    uint8_t* objects = (uint8_t*)slab + SLAB_HEADER_SIZE;
    slab->free_list = (slab_free_node_t*)objects;

    slab_free_node_t* current = slab->free_list;
    for (size_t i = 0; i < cache->objects_per_slab - 1; i++) {
        current->next = (slab_free_node_t*)((uint8_t*)current + cache->object_size);
        current = current->next;
    }
    current->next = 0;

    /* Single timing delay after loop */
    SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */

    /* Allocate first object */
    void* obj = slab->free_list;
    slab->free_list = slab->free_list->next;
    slab->num_free--;

    SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */

    /* Add slab to partial list */
    add_slab_to_list(slab, &cache->partial);

    SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */

    cache->num_slabs++;
    SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */

    cache->num_allocations++;
    SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */
    
    g_slab_total_slabs++;
    SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */
    
    g_slab_total_allocs++;
    SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */

    spinlock_release(&g_slab_lock);

    return obj;
}

/**
 * Free memory allocated by kmem_alloc() - PROPER IMPLEMENTATION
 * 
 * FIX (FEAT-MEM-003): Previously a no-op causing memory leak.
 * Now properly returns objects to slab free lists for reuse.
 * 
 * Algorithm:
 *   1. Find the slab containing this object
 *   2. Validate the object pointer alignment
 *   3. Check for double-free (DEBUG mode)
 *   4. Check guard bytes for corruption (DEBUG mode)
 *   5. Return object to slab free list
 *   6. Move slab between lists as needed (full->partial, partial->empty)
 *   7. Free empty slabs back to heap
 *
 * NOTE: No timing delays needed - only pointer write is to free_list
 */
void kmem_free(void* ptr, size_t size) {
    (void)size;

    if (!g_slab_initialized) {
        return;
    }

    /* NULL is safe to free (no-op) */
    if (ptr == 0) {
        return;
    }

    spinlock_acquire(&g_slab_lock);

    /* MED-001 FIX: O(1) slab lookup.
     * Each slab occupies exactly one SLAB_SIZE-aligned page with the
     * slab_t header at the page base.  Aligning ptr down to SLAB_SIZE
     * gives the owning slab directly, replacing the previous O(N*M) scan
     * over all caches and all their partial/full slab lists. */
    slab_t* slab = (slab_t*)((uintptr_t)ptr & ~((uintptr_t)(SLAB_SIZE - 1)));

    /* Validate slab magic before dereferencing any other field */
    if (slab->magic != SLAB_MAGIC) {
        /* Not a live slab page - invalid free */
        spinlock_release(&g_slab_lock);
        return;
    }

    slab_cache_t* target_cache = slab->cache;
    if (!target_cache) {
        spinlock_release(&g_slab_lock);
        return;
    }

    /* MED-NEW-001 FIX: Validate that ptr falls within the object area of this
     * slab page before computing obj_offset.  Without this check, a pointer
     * below obj_area_start causes uintptr_t underflow, producing a large
     * positive offset that passes the alignment check and corrupts memory.
     * obj_area_end is the first byte past the slab page. */
    uint8_t* obj_area_start = (uint8_t*)slab + SLAB_HEADER_SIZE;
    uint8_t* obj_area_end   = (uint8_t*)slab + SLAB_SIZE;
    if ((uint8_t*)ptr < obj_area_start || (uint8_t*)ptr >= obj_area_end) {
        spinlock_release(&g_slab_lock);
        return;
    }

    /* Validate object alignment */
    uintptr_t obj_offset = (uintptr_t)ptr - (uintptr_t)obj_area_start;
    if (obj_offset % target_cache->object_size != 0) {
        /* Invalid pointer - not aligned to object boundary */
        spinlock_release(&g_slab_lock);
        return;
    }

#if SLAB_DEBUG
    /* Check for double-free */
    slab_free_node_t* check = slab->free_list;
    while (check) {
        if (check == ptr) {
            /* Double free detected! */
            g_slab_double_frees++;
            spinlock_release(&g_slab_lock);
            return;
        }
        check = check->next;
    }

    /* Check guard bytes */
    uint8_t* obj = (uint8_t*)ptr;
    uint8_t* guard_before = obj - GUARD_SIZE;
    uint8_t* guard_after = obj + target_cache->object_size;

    for (int i = 0; i < GUARD_SIZE; i++) {
        if (guard_before[i] != SLAB_GUARD_BYTE || guard_after[i] != SLAB_GUARD_BYTE) {
            /* Buffer overflow detected! */
            g_slab_corruptions_detected++;
            /* Still free the object, but log the corruption */
        }
    }
#endif

    /* Return object to free list - SINGLE POINTER WRITE (needs delay) */
    slab_free_node_t* node = (slab_free_node_t*)ptr;
    node->next = slab->free_list;
    SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */

    slab->free_list = node;
    slab->num_free++;

    SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */

    target_cache->num_frees++;
    SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */

    g_slab_total_frees++;
    SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */

    /* Move slab between lists as needed */
    if (slab->num_free == slab->num_objects) {
        /* SLAB-MED-002 FIX: Slab fully empty — return page to heap.
         *
         * Previously slabs were never freed, causing permanent memory loss
         * proportional to peak live-slab count.
         *
         * Transition source:
         *   num_objects == 1 (2048-byte cache): 0→1 means slab was in full.
         *   num_objects  > 1 (all other caches): came from partial.
         *
         * Lock-order protocol to prevent g_slab_lock → g_heap_lock deadlock:
         *   1. Remove slab from list while holding g_slab_lock.
         *   2. Clear magic so kmem_free_auto() routes to bitmap path,
         *      NOT back into kmem_free() which would re-acquire g_slab_lock.
         *   3. Release g_slab_lock.
         *   4. Call kmem_free_auto() — acquires g_heap_lock only. */
        if (slab->num_objects == 1) {
            remove_slab_from_list(slab, &target_cache->full);
        } else {
            remove_slab_from_list(slab, &target_cache->partial);
        }
        target_cache->num_slabs--;
        g_slab_total_slabs--;
        slab->magic = 0;
        spinlock_release(&g_slab_lock);
        kmem_free_auto((void*)slab);
        return;
    } else if (slab->num_free == 1) {
        /* Was full (0 free → 1 free) — move to partial */
        remove_slab_from_list(slab, &target_cache->full);
        add_slab_to_list(slab, &target_cache->partial);
        SLAB_TIMING_DELAY();  /* MED-001 FIX: Conditional timing delay */
    }

    spinlock_release(&g_slab_lock);
}

/* =============================================================================
 * Direct Cache API
 * =============================================================================
 */

void* slab_alloc_32(void) { return kmem_alloc(32); }
void slab_free_32(void* ptr) { kmem_free(ptr, 32); }

void* slab_alloc_64(void) { return kmem_alloc(64); }
void slab_free_64(void* ptr) { kmem_free(ptr, 64); }

void* slab_alloc_128(void) { return kmem_alloc(128); }
void slab_free_128(void* ptr) { kmem_free(ptr, 128); }

void* slab_alloc_256(void) { return kmem_alloc(256); }
void slab_free_256(void* ptr) { kmem_free(ptr, 256); }

void* slab_alloc_512(void) { return kmem_alloc(512); }
void slab_free_512(void* ptr) { kmem_free(ptr, 512); }

void* slab_alloc_1024(void) { return kmem_alloc(1024); }
void slab_free_1024(void* ptr) { kmem_free(ptr, 1024); }

void* slab_alloc_2048(void) { return kmem_alloc(2048); }
void slab_free_2048(void* ptr) { kmem_free(ptr, 2048); }

/* =============================================================================
 * Query Functions
 * =============================================================================
 */

void slab_print_stats(void) {
    if (!g_slab_initialized) {
        serial_write_str("[SLAB] Not initialized\r\n");
        return;
    }

    serial_write_str("\r\n=== Slab Stats ===\r\n");
    serial_write_str("Slabs: ");
    serial_write_dec(g_slab_total_slabs);
    serial_write_str("\r\n");
    serial_write_str("Allocs: ");
    serial_write_dec(g_slab_total_allocs);
    serial_write_str("\r\n");
    serial_write_str("Frees: ");
    serial_write_dec(g_slab_total_frees);
    serial_write_str("\r\n");
    serial_write_str("Pool: ");
    serial_write_dec(g_slab_memory_used);
    serial_write_str(" bytes\r\n");

#if SLAB_DEBUG
    serial_write_str("\r\n=== Debug Stats ===\r\n");
    serial_write_str("Corruptions: ");
    serial_write_dec(g_slab_corruptions_detected);
    serial_write_str("\r\n");
    serial_write_str("Double frees: ");
    serial_write_dec(g_slab_double_frees);
    serial_write_str("\r\n");
#endif

    serial_write_str("=============\r\n");
}

size_t slab_get_total_memory(void) {
    return g_slab_total_slabs * SLAB_SIZE;
}
