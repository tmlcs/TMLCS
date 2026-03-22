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

/* =============================================================================
 * Helper Functions (Internal)
 * =============================================================================
 */

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
     * All pages are FREE by default.
     * 
     * We could mark some pages as used here if needed:
     * - Pages containing the kernel itself
     * - Pages containing page tables
     * - Reserved regions
     */
    
    /* For now, mark first 16 pages (64KB) as used for kernel/boot data */
    for (size_t i = 0; i < 16; i++) {
        set_bit(i);
    }
    
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

size_t bitmap_alloc(void) {
    if (!g_bitmap_initialized) {
        return (size_t)-1;
    }

    /* Scan bitmap for first free page */
    for (size_t word_idx = 0; word_idx < BITMAP_WORDS; word_idx++) {
        uint64_t word = g_page_bitmap.words[word_idx];

        /* If word is all 1s, no free pages here */
        if (word == 0xFFFFFFFFFFFFFFFFULL) {
            continue;
        }

        /* PERF-MEM-001: Use __builtin_ctzll for O(1) bit finding
         * Find first zero bit: invert word and count trailing zeros
         * __builtin_ctzll returns number of trailing zeros
         * For inverted word, this gives us the first zero bit position
         */
        unsigned int bit_idx = __builtin_ctzll(~word);
        size_t page = word_idx * 64 + bit_idx;

        /* Mark as used */
        set_bit(page);

        return page;
    }

    /* No free pages found */
    return (size_t)-1;
}

size_t bitmap_alloc_contiguous(size_t count) {
    if (!g_bitmap_initialized || count == 0) {
        return (size_t)-1;
    }
    
    size_t found_start = (size_t)-1;
    size_t found_count = 0;
    
    /* Scan bitmap for contiguous region */
    for (size_t page = 0; page < TOTAL_PAGES; page++) {
        if (test_bit(page) == 0) {
            /* Free page found */
            if (found_start == (size_t)-1) {
                found_start = page;
            }
            found_count++;
            
            /* Check if we found enough */
            if (found_count >= count) {
                /* Mark all pages as used */
                for (size_t i = 0; i < count; i++) {
                    set_bit(found_start + i);
                }
                return found_start;
            }
        } else {
            /* Allocated page - reset search */
            found_start = (size_t)-1;
            found_count = 0;
        }
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
    
    if (test_bit(page) == 0) {
        return -1;  /* Page already free */
    }
    
    clear_bit(page);
    return 0;
}

int bitmap_free_contiguous(size_t page, size_t count) {
    /* FIX-MEM-004: Validate inputs */
    if (!g_bitmap_initialized || page >= TOTAL_PAGES || count == 0) {
        return -1;
    }

    /* Validate all pages are allocated */
    for (size_t i = 0; i < count; i++) {
        if (page + i >= TOTAL_PAGES || test_bit(page + i) == 0) {
            return -1;  /* Invalid or already free */
        }
    }

    /* Free all pages */
    for (size_t i = 0; i < count; i++) {
        clear_bit(page + i);
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
        
        /* Count zero bits */
        for (size_t bit_idx = 0; bit_idx < 64; bit_idx++) {
            if (!(word & (1ULL << bit_idx))) {
                count++;
            }
        }
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
