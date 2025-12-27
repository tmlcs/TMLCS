#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h> // For size_t

namespace VMM {
    // Page table entry flags
    const uint64_t PAGE_PRESENT = 1 << 0;
    const uint64_t PAGE_WRITE = 1 << 1;
    const uint64_t PAGE_USER = 1 << 2;
    const uint64_t PAGE_WRITE_THROUGH = 1 << 3;
    const uint64_t PAGE_CACHE_DISABLE = 1 << 4;
    const uint64_t PAGE_ACCESSED = 1 << 5;
    const uint64_t PAGE_DIRTY = 1 << 6;
    const uint64_t PAGE_HUGE_PAGE = 1 << 7; // For PDE and PDPTE
    const uint64_t PAGE_GLOBAL = 1 << 8;
    const uint64_t PAGE_NO_EXECUTE = 1ULL << 63;

    // Functions for page table management
    uint64_t* get_current_page_table();
    void switch_page_table(uint64_t* page_table);

    void map_page(uint64_t virtual_address, uint64_t physical_address, uint64_t flags);
    void unmap_page(uint64_t virtual_address);

    uint64_t get_physical_address(uint64_t virtual_address);
}

#endif // VMM_H
