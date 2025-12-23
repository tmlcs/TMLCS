#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "log.h"

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
        Log::info("Heap start: ");
        // TODO: Convert heap_start_address to string for logging
        // Log::info(uint64_to_string(heap_start_address));
        Log::info("Heap end: ");
        // TODO: Convert heap_end_address to string for logging
        // Log::info(uint64_to_string(heap_end_address));
    }

    void* malloc(size_t size) {
        // Align size to 8 bytes for simplicity
        size = (size + 7) & ~7;

        FreeBlock* current = free_list_head;
        FreeBlock* previous = nullptr;

        while (current) {
            if (current->size >= size + sizeof(FreeBlock)) { // Found a block large enough
                // Split the block
                FreeBlock* new_block = (FreeBlock*)((uint64_t)current + size + sizeof(FreeBlock));
                new_block->size = current->size - (size + sizeof(FreeBlock));
                new_block->next = current->next;

                current->size = size;
                current->next = nullptr;

                if (previous) {
                    previous->next = new_block;
                } else {
                    free_list_head = new_block;
                }
                return (void*)((uint64_t)current + sizeof(FreeBlock));
            }
            previous = current;
            current = current->next;
        }
        Log::error("Heap: Out of memory!");
        return nullptr; // Out of memory
    }

    void free(void* ptr) {
        if (!ptr) return;

        FreeBlock* block_to_free = (FreeBlock*)((uint64_t)ptr - sizeof(FreeBlock));
        
        // Simple insertion into the free list (no merging for now)
        block_to_free->next = free_list_head;
        free_list_head = block_to_free;

        // TODO: Implement merging of adjacent free blocks for better memory utilization
    }
}
