#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h> // For size_t

namespace PMM {
    const uint64_t PAGE_SIZE = 4096; // 4KB pages

    void init(uint64_t memory_size, uint64_t bitmap_address);
    void mark_region_used(uint64_t base, uint64_t length);
    void mark_region_free(uint64_t base, uint64_t length);

    uint64_t allocate_page();
    void free_page(uint64_t page_address);

    uint64_t get_free_memory();
    uint64_t get_used_memory();
}

#endif // PMM_H
