#include "memory/vmm.h"
#include "memory/pmm.h"
#include "utils/log.h"

// Assembly function to invalidate TLB entry
extern "C" void invlpg(uint64_t virtual_address);

namespace VMM {

    // Helper function to get the physical address of a page table entry
    static uint64_t* get_pte_address(uint64_t* pml4, uint64_t virtual_address, bool create_if_not_present) {
        uint64_t pml4_index = (virtual_address >> 39) & 0x1FF;
        uint64_t pdpt_index = (virtual_address >> 30) & 0x1FF;
        uint64_t pd_index = (virtual_address >> 21) & 0x1FF;
        uint64_t pt_index = (virtual_address >> 12) & 0x1FF;

        uint64_t* pdpt = (uint64_t*)(pml4[pml4_index] & 0xFFFFFFFFFFFFF000ULL);
        if (!pdpt && create_if_not_present) {
            pdpt = (uint64_t*)PMM::allocate_page();
            if (!pdpt) {
                Log::error("VMM: Failed to allocate PDPT page for virtual address %x.", virtual_address);
                return nullptr;
            }
            for (int i = 0; i < 512; ++i) pdpt[i] = 0; // Clear new page
            pml4[pml4_index] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_WRITE;
        } else if (!pdpt) {
            return nullptr;
        }

        uint64_t* pd = (uint64_t*)(pdpt[pdpt_index] & 0xFFFFFFFFFFFFF000ULL);
        if (!pd && create_if_not_present) {
            pd = (uint64_t*)PMM::allocate_page();
            if (!pd) {
                Log::error("VMM: Failed to allocate PD page for virtual address %x.", virtual_address);
                return nullptr;
            }
            for (int i = 0; i < 512; ++i) pd[i] = 0; // Clear new page
            pdpt[pdpt_index] = (uint64_t)pd | PAGE_PRESENT | PAGE_WRITE;
        } else if (!pd) {
            return nullptr;
        }

        uint64_t* pt = (uint64_t*)(pd[pd_index] & 0xFFFFFFFFFFFFF000ULL);
        if (!pt && create_if_not_present) {
            pt = (uint64_t*)PMM::allocate_page();
            if (!pt) {
                Log::error("VMM: Failed to allocate PT page for virtual address %x.", virtual_address);
                return nullptr;
            }
            for (int i = 0; i < 512; ++i) pt[i] = 0; // Clear new page
            pd[pd_index] = (uint64_t)pt | PAGE_PRESENT | PAGE_WRITE;
        } else if (!pt) {
            return nullptr;
        }

        return &pt[pt_index];
    }

    uint64_t* get_current_page_table() {
        uint64_t pml4_phys;
        asm volatile ("mov %%cr3, %0" : "=r" (pml4_phys));
        return (uint64_t*)pml4_phys; // Assuming identity mapping for kernel
    }

    void switch_page_table(uint64_t* page_table) {
        asm volatile ("mov %0, %%cr3" : : "r" (page_table));
    }

    void map_page(uint64_t virtual_address, uint64_t physical_address, uint64_t flags) {
        uint64_t* pml4 = get_current_page_table();
        uint64_t* pte = get_pte_address(pml4, virtual_address, true);
        if (pte) {
            *pte = physical_address | flags | PAGE_PRESENT;
            invlpg(virtual_address); // Invalidate TLB entry
        } else {
            Log::error("VMM: Failed to map page %x to %x.", virtual_address, physical_address);
        }
    }

    void unmap_page(uint64_t virtual_address) {
        uint64_t* pml4 = get_current_page_table();
        uint64_t* pte = get_pte_address(pml4, virtual_address, false);
        if (pte) {
            *pte = 0; // Clear the entry
            invlpg(virtual_address); // Invalidate TLB entry
        } else {
            Log::warning("VMM: Attempted to unmap a non-existent page at %x.", virtual_address);
        }
    }

    uint64_t get_physical_address(uint64_t virtual_address) {
        uint64_t* pml4 = get_current_page_table();
        uint64_t* pte = get_pte_address(pml4, virtual_address, false);
        if (pte && (*pte & PAGE_PRESENT)) {
            return (*pte & 0xFFFFFFFFFFFFF000ULL) + (virtual_address & 0xFFF);
        }
        return 0; // Not mapped or not present
    }
}
