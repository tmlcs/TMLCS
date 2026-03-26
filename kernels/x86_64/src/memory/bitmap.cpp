#include "bitmap.h"
#include "serial.h"
#include "print.h"
#include "spinlock.h"
#include "barriers.h"

/* =============================================================================
 * Global Bitmap Instance
 * =============================================================================
 * Allocated in .bss section - zero-initialized by boot code.
 * This means all pages start as FREE (all bits = 0).
 * =============================================================================
 */
bitmap_t g_page_bitmap;

/* =============================================================================
 * Initialization State
 * =============================================================================
 */
static int g_bitmap_initialized = 0;

/* Linker script symbol: end of all kernel sections
 * (.text + .rodata + .data + .bss incl. early_alloc pool + .boot.data
 *  incl. page tables and boot stack).
 * Declared as char so taking its address gives the raw byte address.
 */
extern "C" char __kernel_end;

/* =============================================================================
 * Helper Functions (Internal)
 * =============================================================================
 */

/**
 * @brief Count set bits in a 64-bit word (freestanding popcount)
 *
 * __builtin_popcountll emits a call to __popcountdi2 (libgcc) when the
 * hardware popcnt instruction is not explicitly enabled, which is unavailable
 * in -nostdlib freestanding builds.  This parallel bit-count is O(1) with
 * no library dependencies.
 */
static inline size_t popcount64(uint64_t x) {
    x -= (x >> 1) & 0x5555555555555555ULL;
    x  = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
    x  = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return (size_t)((x * 0x0101010101010101ULL) >> 56);
}

/**
 * @brief Get the word index and bit index for a page
 */
static inline void get_bit_indices(size_t page, size_t* word_idx, size_t* bit_idx) {
    *word_idx = page / 64;
    *bit_idx = page % 64;
}

/**
 * @brief Test if a bit is set
 */
static inline int test_bit(size_t page) {
    size_t word_idx, bit_idx;
    get_bit_indices(page, &word_idx, &bit_idx);
    
    if (word_idx >= BITMAP_WORDS) {
        return -1;  /* Invalid page */
    }
    
    return (g_page_bitmap.words[word_idx] & (1ULL << bit_idx)) ? 1 : 0;
}

/**
 * @brief Set a bit (mark page as used)
 */
static inline void set_bit(size_t page) {
    size_t word_idx, bit_idx;
    get_bit_indices(page, &word_idx, &bit_idx);
    
    if (word_idx < BITMAP_WORDS) {
        g_page_bitmap.words[word_idx] |= (1ULL << bit_idx);
        wmb();  /* Ensure write is visible */
    }
}

/**
 * @brief Clear a bit (mark page as free)
 */
static inline void clear_bit(size_t page) {
    size_t word_idx, bit_idx;
    get_bit_indices(page, &word_idx, &bit_idx);
    
    if (word_idx < BITMAP_WORDS) {
        g_page_bitmap.words[word_idx] &= ~(1ULL << bit_idx);
        wmb();  /* Ensure write is visible */
    }
}

/* =============================================================================
 * Initialization
 * =============================================================================
 */

void bitmap_init(void) {
    /*
     * Bitmap is in .bss, so it's already zero-initialized.
     * All pages start as FREE.
     *
     * Reserve all pages occupied by the kernel image so the heap
     * cannot hand them out and overwrite running kernel code/data.
     *
     * __kernel_end (linker.ld) is placed after the last section:
     *   .text + .rodata + .data + .bss (incl. 1 MB early_alloc pool)
     *   + .boot.data (page tables + 64 KB boot stack)
     *
     * Round up to the next page boundary so the final partial page
     * (if any) is fully protected.
     */
    uintptr_t end_addr      = (uintptr_t)&__kernel_end;
    uintptr_t protected_end = (end_addr + PAGE_SIZE - 1) & ~((uintptr_t)(PAGE_SIZE - 1));
    size_t    pages_to_reserve = (protected_end - PHYSICAL_MEMORY_START) / PAGE_SIZE;

    if (pages_to_reserve > TOTAL_PAGES) {
        pages_to_reserve = TOTAL_PAGES;
    }

    for (size_t i = 0; i < pages_to_reserve; i++) {
        set_bit(i);
    }

    serial_write_str("[BITMAP] Reserved ");
    serial_write_dec(pages_to_reserve);
    serial_write_str(" pages for kernel (");
    serial_write_dec((uint32_t)(pages_to_reserve * PAGE_SIZE / 1024));
    serial_write_str(" KB, 0x100000-0x");
    serial_write_hex(protected_end);
    serial_write_str(")\r\n");

    wmb();
    g_bitmap_initialized = 1;
    mb();  /* Ensure initialization is visible */
}

int bitmap_is_initialized(void) {
    rmb();
    return g_bitmap_initialized;
}

/* =============================================================================
 * Core Allocation Functions
 * =============================================================================
 */

/* MED-NEW-003: INTERNAL API — bitmap_alloc() is an internal function.
 * Direct callers (outside heap.cpp) must:
 *   1. Hold g_heap_lock for the entire alloc + g_page_alloc_count update.
 *   2. Update g_page_alloc_count[page] immediately after a successful return.
 * Failing to do so leaves the per-page metadata out of sync with the
 * bitmap, breaking kmalloc_size() and krealloc() for that page. */
size_t bitmap_alloc(void) {
    if (!g_bitmap_initialized) {
        return (size_t)-1;
    }

    /* HIGH-004 FIX: Atomic CAS loop prevents TOCTOU race between scan and mark.
     *
     * Original: read word → find free bit → set_bit() (non-atomic |=)
     * Race: another CPU could claim the same bit between the read and |=.
     *
     * Fix: use __atomic_compare_exchange_n. If another CPU modifies the word
     * between our read and CAS, the CAS fails and 'word' is refreshed with
     * the current value — we retry within the same word_idx without
     * re-scanning from the beginning.
     */
    for (size_t word_idx = 0; word_idx < BITMAP_WORDS; word_idx++) {
        uint64_t word = __atomic_load_n(&g_page_bitmap.words[word_idx],
                                        __ATOMIC_RELAXED);

        while (word != 0xFFFFFFFFFFFFFFFFULL) {
            /* Find first free bit in this word */
            unsigned int bit_idx = __builtin_ctzll(~word);
            uint64_t desired = word | (1ULL << bit_idx);

            /* Attempt to claim the bit atomically.
             * On failure, 'word' is updated with the current memory value
             * so the next iteration retries with fresh data. */
            if (__atomic_compare_exchange_n(
                    &g_page_bitmap.words[word_idx],
                    &word,
                    desired,
                    0 /* strong */,
                    __ATOMIC_SEQ_CST,
                    __ATOMIC_SEQ_CST)) {
                return word_idx * 64 + (size_t)bit_idx;
            }
            /* CAS failed: 'word' refreshed — retry same word */
        }
    }

    /* No free pages found */
    return (size_t)-1;
}

size_t bitmap_alloc_contiguous(size_t count) {
    if (!g_bitmap_initialized || count == 0) {
        return (size_t)-1;
    }

    /* HIGH-004 FIX: Atomic fetch_or + rollback prevents TOCTOU race.
     *
     * Original: scan with test_bit() → set_bit() for each page (non-atomic).
     * Race: another CPU could claim a page in the candidate run between the
     * scan and the mark, causing two allocations to overlap.
     *
     * Fix:
     *   1. Scan for a candidate free run (non-atomic — just a hint).
     *   2. Claim each page with __atomic_fetch_or; if any bit was already
     *      set by another CPU, roll back all claimed pages with
     *      __atomic_fetch_and and restart the scan past the conflict.
     */
    size_t scan_start = 0;

    while (scan_start < TOTAL_PAGES) {
        /* Find next free run of 'count' pages starting at scan_start */
        size_t found_start = scan_start;
        size_t found_count = 0;

        for (size_t page = scan_start; page < TOTAL_PAGES; page++) {
            uint64_t word = __atomic_load_n(
                &g_page_bitmap.words[page / 64], __ATOMIC_RELAXED);
            if (word & (1ULL << (page % 64))) {
                /* Allocated — reset run, advance start past this page */
                found_start = page + 1;
                found_count = 0;
            } else {
                if (++found_count >= count) break;
            }
        }

        if (found_count < count) {
            return (size_t)-1;  /* No free run exists */
        }

        /* Attempt to claim [found_start, found_start+count) atomically */
        size_t claimed = 0;
        for (; claimed < count; claimed++) {
            size_t p = found_start + claimed;
            uint64_t mask = 1ULL << (p % 64);
            uint64_t old_val = __atomic_fetch_or(
                &g_page_bitmap.words[p / 64], mask, __ATOMIC_SEQ_CST);
            if (old_val & mask) {
                break;  /* Conflict: page already in use */
            }
        }

        if (claimed == count) {
            return found_start;  /* All pages claimed successfully */
        }

        /* Conflict at found_start+claimed — roll back pages we claimed */
        for (size_t j = 0; j < claimed; j++) {
            size_t rp = found_start + j;
            __atomic_fetch_and(
                &g_page_bitmap.words[rp / 64],
                ~(1ULL << (rp % 64)),
                __ATOMIC_SEQ_CST);
        }

        /* Restart scan past the conflicting page */
        scan_start = found_start + claimed + 1;
    }

    /* No contiguous region found */
    return (size_t)-1;
}

int bitmap_free(size_t page) {
    if (!g_bitmap_initialized) {
        return -1;
    }

    if (page >= TOTAL_PAGES) {
        return -1;  /* Invalid page */
    }

    /* BITMAP-MED-001 FIX: Atomic test-and-clear prevents TOCTOU race.
     *
     * Original: test_bit() then clear_bit() — two separate non-atomic ops.
     * Race: another CPU could free the same page between the check and
     * the clear, causing a silent double-free that corrupts the bitmap.
     *
     * Fix: __atomic_fetch_and clears the bit and returns the old word in
     * a single atomic RMW. If the bit was already clear in the old word,
     * report double-free without corrupting state (the fetch_and was a
     * no-op since the bit was already 0). */
    size_t   word_idx = page / 64;
    uint64_t mask     = 1ULL << (page % 64);

    uint64_t old_val = __atomic_fetch_and(
        &g_page_bitmap.words[word_idx],
        ~mask,
        __ATOMIC_SEQ_CST);

    if (!(old_val & mask)) {
        return -1;  /* Page was already free — double-free detected */
    }

    return 0;
}

/**
 * CRIT-003 FIX: Atomically free contiguous pages to prevent TOCTOU race
 * 
 * PROBLEM: Original implementation had a time-of-check-time-of-use race.
 * Between checking if pages are allocated and clearing bits, another CPU
 * could free the same pages, causing double-free corruption.
 * 
 * SOLUTION: Use atomic test-and-clear for each page. If any page is
 * already free, rollback all previously-freed pages and return error.
 */
int bitmap_free_contiguous(size_t page, size_t count) {
    /* FIX-MEM-004: Validate inputs */
    if (!g_bitmap_initialized || page >= TOTAL_PAGES || count == 0) {
        return -1;
    }

    /* CRIT-003 FIX: Atomically free each page with rollback on failure */
    size_t freed = 0;
    
    for (size_t i = 0; i < count; i++) {
        size_t current_page = page + i;
        
        /* Check bounds */
        if (current_page >= TOTAL_PAGES) {
            /* Rollback already-freed pages — atomic to prevent SMP race */
            for (size_t j = 0; j < freed; j++) {
                size_t rp = page + j;
                __atomic_fetch_or(
                    &g_page_bitmap.words[rp / 64],
                    1ULL << (rp % 64),
                    __ATOMIC_SEQ_CST);
            }
            return -1;
        }
        
        /* Atomically test-and-clear the bit */
        size_t word_idx, bit_idx;
        get_bit_indices(current_page, &word_idx, &bit_idx);
        
        uint64_t mask = 1ULL << bit_idx;
        uint64_t old_val = __atomic_fetch_and(
            &g_page_bitmap.words[word_idx],
            ~mask,
            __ATOMIC_SEQ_CST
        );
        
        /* Check if bit was already clear (double-free) */
        if (!(old_val & mask)) {
            /* Page was already free - rollback pages [0, freed) that we cleared.
             * NOTE: page+freed was NOT cleared by us (its bit was already 0),
             * so the rollback must stop at j < freed, not j <= freed.
             * Use atomic fetch_or to prevent SMP race during rollback. */
            for (size_t j = 0; j < freed; j++) {
                size_t rp = page + j;
                __atomic_fetch_or(
                    &g_page_bitmap.words[rp / 64],
                    1ULL << (rp % 64),
                    __ATOMIC_SEQ_CST);
            }
            return -1;  /* Double-free detected */
        }
        
        freed++;
    }

    return 0;
}

/* =============================================================================
 * Query Functions
 * =============================================================================
 */

int bitmap_is_page_allocated(size_t page) {
    if (!g_bitmap_initialized || page >= TOTAL_PAGES) {
        return -1;
    }
    return test_bit(page);
}

size_t bitmap_count_free_pages(void) {
    if (!g_bitmap_initialized) {
        return 0;
    }
    
    size_t count = 0;

    for (size_t word_idx = 0; word_idx < BITMAP_WORDS; word_idx++) {
        uint64_t word = g_page_bitmap.words[word_idx];
        /* MED-001 FIX: popcount64(~word) counts free (zero) bits in O(1)
         * using the parallel bit-count method (no libgcc dependency). */
        /* M-01 FIX: Mask out phantom bits in the last word if TOTAL_PAGES is
         * not a multiple of 64.  Unused high bits are zero (free), which
         * popcount64(~word) would incorrectly count as free pages. */
        if (word_idx == BITMAP_WORDS - 1) {
            size_t used_bits = TOTAL_PAGES % 64;
            if (used_bits != 0) {
                uint64_t mask = (((uint64_t)1ULL) << used_bits) - 1;
                word |= ~mask;  /* set unused high bits to 1 (allocated) */
            }
        }
        count += popcount64(~word);
    }

    return count;
}

size_t bitmap_count_allocated_pages(void) {
    if (!g_bitmap_initialized) {
        return 0;
    }
    
    return TOTAL_PAGES - bitmap_count_free_pages();
}

size_t bitmap_largest_free_region(void) {
    if (!g_bitmap_initialized) {
        return 0;
    }
    
    size_t max_count = 0;
    size_t current_count = 0;
    
    for (size_t page = 0; page < TOTAL_PAGES; page++) {
        if (test_bit(page) == 0) {
            /* Free page */
            current_count++;
            if (current_count > max_count) {
                max_count = current_count;
            }
        } else {
            /* Allocated - reset counter */
            current_count = 0;
        }
    }
    
    return max_count;
}

/* =============================================================================
 * Debug Functions
 * =============================================================================
 */

void bitmap_print_stats(void) {
    if (!g_bitmap_initialized) {
        serial_write_str("[BITMAP] Not initialized\r\n");
        return;
    }
    
    size_t free_pages = bitmap_count_free_pages();
    size_t allocated_pages = bitmap_count_allocated_pages();
    size_t largest = bitmap_largest_free_region();
    
    serial_write_str("\r\n=== Bitmap Statistics ===\r\n");
    serial_write_str("Total pages:      ");
    serial_write_dec(TOTAL_PAGES);
    serial_write_str("\r\n");
    
    serial_write_str("Free pages:       ");
    serial_write_dec(free_pages);
    serial_write_str("\r\n");
    
    serial_write_str("Allocated pages:  ");
    serial_write_dec(allocated_pages);
    serial_write_str("\r\n");
    
    serial_write_str("Largest free:     ");
    serial_write_dec(largest);
    serial_write_str(" pages\r\n");
    
    serial_write_str("Free memory:      ");
    serial_write_dec(free_pages * PAGE_SIZE / 1024 / 1024);
    serial_write_str(" MB\r\n");
    
    serial_write_str("Fragmentation:    ");
    if (allocated_pages > 0) {
        /* Simple fragmentation metric */
        size_t frag = (allocated_pages > largest) ? 
                      ((allocated_pages - largest) * 100 / allocated_pages) : 0;
        serial_write_dec(frag);
        serial_write_str("%\r\n");
    } else {
        serial_write_str("0%\r\n");
    }
    
    serial_write_str("=========================\r\n");
}

void bitmap_dump_region(size_t start_page, size_t count) {
    if (!g_bitmap_initialized) {
        return;
    }
    
    serial_write_str("[BITMAP DUMP] Pages ");
    serial_write_dec(start_page);
    serial_write_str("-");
    serial_write_dec(start_page + count - 1);
    serial_write_str(":\r\n");
    
    for (size_t i = 0; i < count; i++) {
        if (i % 64 == 0) {
            serial_write_str("\r\n");
            serial_write_dec(start_page + i);
            serial_write_str(": ");
        }
        
        int allocated = test_bit(start_page + i);
        serial_write_char(allocated ? '1' : '0');
        
        if ((i + 1) % 8 == 0) {
            serial_write_char(' ');
        }
    }
    
    serial_write_str("\r\n");
}
