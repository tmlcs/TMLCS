#include "memory/pmm.h"
#include "utils/log.h"
#include "print.h" // For debugging output to VGA if needed

// Global variables for PMM
static uint64_t* bitmap;
static uint64_t total_pages;
static uint64_t used_pages;

// Helper function to set a bit in the bitmap
static void set_bit(uint64_t bit) {
    bitmap[bit / 64] |= (1ULL << (bit % 64));
}

// Helper function to clear a bit in the bitmap
static void clear_bit(uint64_t bit) {
    bitmap[bit / 64] &= ~(1ULL << (bit % 64));
}

// Helper function to check a bit in the bitmap
static bool test_bit(uint64_t bit) {
    return (bitmap[bit / 64] & (1ULL << (bit % 64))) != 0;
}

namespace PMM {

    void init(uint64_t memory_size, uint64_t bitmap_address) {
        total_pages = memory_size / PAGE_SIZE;
        bitmap = (uint64_t*)bitmap_address;
        used_pages = 0;

        // Mark all pages as used initially
        for (uint64_t i = 0; i < total_pages / 64; ++i) {
            bitmap[i] = 0xFFFFFFFFFFFFFFFFULL;
        }
        // Handle remaining bits if total_pages is not a multiple of 64
        for (uint64_t i = (total_pages / 64) * 64; i < total_pages; ++i) {
            set_bit(i);
        }
        used_pages = total_pages; // Initialize used_pages to total_pages after marking all as used

        Log::info("PMM initialized.");
        Log::info("Total pages: %d", total_pages);
    }

    void mark_region_used(uint64_t base, uint64_t length) {
        uint64_t start_page = base / PAGE_SIZE;
        uint64_t num_pages = length / PAGE_SIZE;

        for (uint64_t i = 0; i < num_pages; ++i) {
            if (!test_bit(start_page + i)) { // Only increment if it was free
                set_bit(start_page + i);
                used_pages++;
            }
        }
    }

    void mark_region_free(uint64_t base, uint64_t length) {
        uint64_t start_page = base / PAGE_SIZE;
        uint64_t num_pages = length / PAGE_SIZE;

        for (uint64_t i = 0; i < num_pages; ++i) {
            if (test_bit(start_page + i)) { // Only decrement if it was used
                clear_bit(start_page + i);
                used_pages--;
            }
        }
    }

    uint64_t allocate_page() {
        for (uint64_t i = 0; i < total_pages; ++i) {
            if (!test_bit(i)) { // Found a free page
                set_bit(i);
                used_pages++;
                return i * PAGE_SIZE;
            }
        }
        Log::error("PMM: Out of memory! Could not allocate a page of size %d bytes.", PAGE_SIZE);
        return 0; // Out of memory
    }

    void free_page(uint64_t page_address) {
        uint64_t page_num = page_address / PAGE_SIZE;
        if (test_bit(page_num)) { // Only free if it was used
            clear_bit(page_num);
            used_pages--;
        } else {
            Log::warning("PMM: Attempted to free an already free page at %x.", page_address);
        }
    }

    uint64_t get_free_memory() {
        return (total_pages - used_pages) * PAGE_SIZE;
    }

    uint64_t get_used_memory() {
        return used_pages * PAGE_SIZE;
    }
}
