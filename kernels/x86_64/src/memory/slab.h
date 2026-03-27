#ifndef SLAB_H
#define SLAB_H

#include "spinlock.h"
#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * GLOBEX_OS Slab Allocator
 * =============================================================================
 *
 * Slab allocator for small object allocation (< 4KB).
 * Reduces memory waste and improves allocation speed for small objects.
 *
 * Problem Solved:
 *   - kmalloc(64) wastes 4032 bytes (98% waste!)
 *   - Slab allocator uses exact size needed
 *
 * Design:
 *   - Pre-defined caches for common sizes: 32, 64, 128, 256, 512, 1024, 2048 bytes
 *   - Each cache manages slabs of 4KB pages
 *   - Objects are allocated from free list within slab
 *   - Free objects are linked in-place (no extra memory overhead)
 *
 * Memory Layout (per slab):
 *   +------------------+
 *   | Slab Metadata    |  72 bytes
 *   +------------------+
 *   | Object 0         |  N bytes
 *   +------------------+
 *   | Object 1         |  N bytes
 *   +------------------+
 *   | ...              |
 *   +------------------+
 *   | Object N-1       |  N bytes
 *   +------------------+
 *
 * Usage:
 *   @code
 *   slab_init();  // Initialize at kernel startup
 *
 *   // Allocate small objects
 *   void* obj = kmem_alloc(64);   // Uses 64-byte cache
 *   kmem_free(obj, 64);           // Return to cache
 *
 *   // Direct cache API
 *   void* obj = slab_alloc_64();  // Fast path for 64 bytes
 *   slab_free_64(obj);            // Fast free
 *   @endcode
 *
 * =============================================================================
 */

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/** Maximum size for slab allocation (objects > this use kmalloc) */
#define SLAB_MAX_SIZE 2048

/** Number of predefined caches */
#define NUM_CACHES 7

/** Slab size in bytes (1 page) */
#define SLAB_SIZE 0x1000

/** Cache sizes (power of 2 for alignment) */
#define CACHE_SIZE_32 32
#define CACHE_SIZE_64 64
#define CACHE_SIZE_128 128
#define CACHE_SIZE_256 256
#define CACHE_SIZE_512 512
#define CACHE_SIZE_1024 1024
#define CACHE_SIZE_2048 2048

/** Reserved space for slab_t header at start of each slab page.
 * static_assert in slab.cpp verifies sizeof(slab_t) <= SLAB_HEADER_SIZE. */
#define SLAB_HEADER_SIZE 72

/** Objects per slab (excluding metadata) */
#define OBJECTS_PER_SLAB_32 ((SLAB_SIZE - SLAB_HEADER_SIZE) / 32)
#define OBJECTS_PER_SLAB_64 ((SLAB_SIZE - SLAB_HEADER_SIZE) / 64)
#define OBJECTS_PER_SLAB_128 ((SLAB_SIZE - SLAB_HEADER_SIZE) / 128)
#define OBJECTS_PER_SLAB_256 ((SLAB_SIZE - SLAB_HEADER_SIZE) / 256)
#define OBJECTS_PER_SLAB_512 ((SLAB_SIZE - SLAB_HEADER_SIZE) / 512)
#define OBJECTS_PER_SLAB_1024 ((SLAB_SIZE - SLAB_HEADER_SIZE) / 1024)
#define OBJECTS_PER_SLAB_2048 ((SLAB_SIZE - SLAB_HEADER_SIZE) / 2048)

/* =============================================================================
 * Data Structures
 * =============================================================================
 */

/**
 * @brief Free object node (embedded in free objects)
 *
 * When an object is free, it contains a pointer to the next free object.
 * This uses the object's own memory (no extra overhead).
 */
typedef struct slab_free_node {
    struct slab_free_node* next; /**< Next free object in list */
} slab_free_node_t;

/**
 * @brief Which linked list a slab currently resides in
 */
typedef enum : uint8_t {
    SLAB_LIST_FREE = 0, /* transient init state; overwritten by add_slab_to_list */
    SLAB_LIST_PARTIAL = 1,
    SLAB_LIST_FULL = 2,
} slab_list_state_t;

/**
 * @brief Slab metadata (stored at beginning of each slab)
 */
typedef struct slab slab_t; /* Forward declaration */

struct slab {
    slab_free_node_t* free_list;  /**< List of free objects */
    size_t num_free;              /**< Number of free objects */
    size_t object_size;           /**< Size of each object */
    size_t num_objects;           /**< Total objects in slab */
    struct slab_cache* cache;     /**< Back pointer to cache */
    slab_t* next;                 /**< Next slab in list */
    slab_t* prev;                 /**< Previous slab in list */
    slab_list_state_t list_state; /**< Which list this slab is currently in */
    uint64_t magic;               /**< Magic number for validation */
};

/** Magic number for slab validation */
#define SLAB_MAGIC 0x51AB51AB51AB51ABULL

/**
 * @brief Cache for a specific object size
 * Note: Using fixed-size types for stability
 */
typedef struct slab_cache {
    uint32_t object_size;      /**< Size of objects in this cache */
    uint32_t objects_per_slab; /**< Objects per slab */
    slab_t* partial;           /**< Slabs with some free objects */
    slab_t* full;              /**< Slabs with no free objects */
    slab_t* empty;             /**< Completely empty slabs (cached) */
    uint32_t num_slabs;        /**< Total slabs in cache */
    uint32_t num_allocations;  /**< Total allocations from this cache */
    uint32_t num_frees;        /**< Total frees to this cache */
} slab_cache_t;

/**
 * @brief Global slab allocator state (extern for testing)
 * Note: Made smaller for stability
 */
typedef struct {
    int initialized;          /**< 1 if initialized */
    size_t total_allocations; /**< Total allocations */
    size_t total_frees;       /**< Total frees */
    size_t total_slabs;       /**< Total slabs allocated */
} slab_state_t;

/* Caches are now separate static arrays in slab.cpp */

/* =============================================================================
 * Initialization
 * =============================================================================
 */

int slab_init(void);
int slab_is_initialized(void);
void slab_shutdown(void);

/* =============================================================================
 * Core Allocation API
 * =============================================================================
 */

void* kmem_alloc(size_t size);

/**
 * @brief Free memory allocated by kmem_alloc() or slab_alloc_*()
 * @param ptr Pointer to memory to free
 * @param size Size parameter kept for backward compatibility (ignored)
 *
 * FIX (FEAT-MEM-003): Now properly returns memory to slab free lists.
 * Previously a no-op causing memory leak.
 *
 * Features:
 *   - Returns object to slab free list for reuse
 *   - Moves slabs between lists (full->partial->empty)
 *   - Frees empty slabs back to heap
 *   - Validates object alignment and slab integrity
 *   - Detects double-free (DEBUG mode)
 *   - Detects buffer overflow via guard bytes (DEBUG mode)
 *
 * @note Safe to call with NULL (no operation)
 * @note Do NOT free the same pointer twice (detected in DEBUG mode)
 * @note Do NOT free stack or static memory
 */
void kmem_free(void* ptr, size_t size);

/* =============================================================================
 * Direct Cache API
 * =============================================================================
 */

void* slab_alloc_32(void);
void slab_free_32(void* ptr);
void* slab_alloc_64(void);
void slab_free_64(void* ptr);
void* slab_alloc_128(void);
void slab_free_128(void* ptr);
void* slab_alloc_256(void);
void slab_free_256(void* ptr);
void* slab_alloc_512(void);
void slab_free_512(void* ptr);
void* slab_alloc_1024(void);
void slab_free_1024(void* ptr);
void* slab_alloc_2048(void);
void slab_free_2048(void* ptr);

/* =============================================================================
 * Query Functions
 * =============================================================================
 */

void slab_print_stats(void);
size_t slab_get_total_memory(void);

/* For testing */
slab_state_t* slab_get_state(void);
extern slab_state_t g_slab_state;

/* Slab pool (allocated from early_alloc) */
extern uint8_t* g_slab_pool;
extern size_t g_slab_pool_size;

/** Global slab lock — exposed for krealloc in heap.cpp only. */
extern spinlock_t g_slab_lock;

#ifdef __cplusplus
}
#endif

#endif /* SLAB_H */
