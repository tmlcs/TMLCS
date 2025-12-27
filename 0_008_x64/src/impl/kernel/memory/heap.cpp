#include "memory/heap.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "utils/log.h"

// Simple linked list node for free blocks
struct FreeBlock {
    size_t size;
    FreeBlock* next;
};

static FreeBlock* free_list_head = nullptr;
static uint64_t heap_start_address = 0;
static uint64_t heap_end_address = 0;

namespace Heap {

    void init(uint64_t heap_start, uint64_t heap_end) {
        heap_start_address = heap_start;
        heap_end_address = heap_end;

        // Initialize the entire heap region as one large free block
        free_list_head = (FreeBlock*)heap_start_address;
        free_list_head->size = heap_end_address - heap_start_address;
        free_list_head->next = nullptr;

        Log::info("Heap initialized.");
        Log::info("Heap start: %x", heap_start_address);
        Log::info("Heap end: %x", heap_end_address);
    }

    void* malloc(size_t size) {
        // Align size to 8 bytes for simplicity
        size = (size + 7) & ~7;

        FreeBlock* current = free_list_head;
        FreeBlock* previous = nullptr;
        FreeBlock* best_fit_block = nullptr;
        FreeBlock* best_fit_previous = nullptr;
        size_t min_size_diff = (size_t)-1; // Initialize with a very large value

        while (current) {
            if (current->size >= size + sizeof(FreeBlock)) { // Found a block large enough
                size_t current_size_diff = current->size - (size + sizeof(FreeBlock));
                if (current_size_diff < min_size_diff) {
                    min_size_diff = current_size_diff;
                    best_fit_block = current;
                    best_fit_previous = previous;
                }
            }
            previous = current;
            current = current->next;
        }

        if (best_fit_block) {
            // Split the block
            FreeBlock* new_block = (FreeBlock*)((uint64_t)best_fit_block + size + sizeof(FreeBlock));
            new_block->size = best_fit_block->size - (size + sizeof(FreeBlock));
            new_block->next = best_fit_block->next;

            best_fit_block->size = size;
            best_fit_block->next = nullptr;

            if (best_fit_previous) {
                best_fit_previous->next = new_block;
            } else {
                free_list_head = new_block;
            }
            return (void*)((uint64_t)best_fit_block + sizeof(FreeBlock));
        } else {
            // Out of memory, try to expand the heap
            Log::warning("Heap: Out of memory, attempting to expand heap. Requested size: %d", size);

            uint64_t physical_page = PMM::allocate_page();
            if (physical_page == 0) {
                Log::error("Heap: Failed to allocate physical page for heap expansion.");
                return nullptr; // Truly out of memory
            }

            uint64_t new_virtual_page_address = heap_end_address;
            VMM::map_page(new_virtual_page_address, physical_page, VMM::PAGE_PRESENT | VMM::PAGE_WRITE);
            heap_end_address += PMM::PAGE_SIZE; // Corrected: Use PMM::PAGE_SIZE

            // Add the new page as a free block to the heap's free list
            FreeBlock* new_free_block = (FreeBlock*)new_virtual_page_address;
            new_free_block->size = PMM::PAGE_SIZE - sizeof(FreeBlock); // Corrected: Use PMM::PAGE_SIZE
            new_free_block->next = nullptr;

            // Insert the new_free_block into the sorted free list
            FreeBlock* current_insert = free_list_head;
            FreeBlock* previous_insert = nullptr;

            while (current_insert && current_insert < new_free_block) {
                previous_insert = current_insert;
                current_insert = current_insert->next;
            }

            if (previous_insert) {
                previous_insert->next = new_free_block;
            } else {
                free_list_head = new_free_block;
            }
            new_free_block->next = current_insert;

            // Attempt to merge with adjacent blocks (similar to free function)
            // Try to merge with the previous block
            if (previous_insert && (uint64_t)previous_insert + previous_insert->size + sizeof(FreeBlock) == (uint64_t)new_free_block) {
                previous_insert->size += new_free_block->size + sizeof(FreeBlock);
                previous_insert->next = new_free_block->next;
                new_free_block = previous_insert; // The merged block is now 'previous_insert'
            }

            // Try to merge with the next block
            if (current_insert && (uint64_t)new_free_block + new_free_block->size + sizeof(FreeBlock) == (uint64_t)current_insert) {
                new_free_block->size += current_insert->size + sizeof(FreeBlock);
                new_free_block->next = current_insert->next;
            }

            // Now retry the allocation with the expanded heap
            return malloc(size);
        }
    }

    void free(void* ptr) {
        if (!ptr) return;

        FreeBlock* block_to_free = (FreeBlock*)((uint64_t)ptr - sizeof(FreeBlock));
        
        // Find the correct place to insert the block (maintaining sorted order by address)
        FreeBlock* current = free_list_head;
        FreeBlock* previous = nullptr;

        while (current && current < block_to_free) {
            previous = current;
            current = current->next;
        }

        // Insert the block
        if (previous) {
            previous->next = block_to_free;
        } else {
            free_list_head = block_to_free;
        }
        block_to_free->next = current;

        // Try to merge with the previous block
        if (previous && (uint64_t)previous + previous->size + sizeof(FreeBlock) == (uint64_t)block_to_free) {
            previous->size += block_to_free->size + sizeof(FreeBlock);
            previous->next = block_to_free->next;
            block_to_free = previous; // The merged block is now 'previous'
        }

        // Try to merge with the next block
        if (current && (uint64_t)block_to_free + block_to_free->size + sizeof(FreeBlock) == (uint64_t)current) {
            block_to_free->size += current->size + sizeof(FreeBlock);
            block_to_free->next = current->next;
        }
    }
} // Close Heap namespace
