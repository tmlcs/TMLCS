#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h> // For size_t

namespace Heap {
    void init(uint64_t heap_start, uint64_t heap_end);
    void* malloc(size_t size);
    void free(void* ptr);
}

#endif // HEAP_H
