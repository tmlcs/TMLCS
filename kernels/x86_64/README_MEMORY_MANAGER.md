# GLOBEX_OS - Memory Manager Documentation

## Overview

The GLOBEX_OS Memory Manager provides kernel-space dynamic memory allocation using a bitmap-based page allocator. It offers `kmalloc()`/`kfree()` style APIs familiar to kernel developers.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Code                          │
│                    (kernel_main, drivers)                    │
├─────────────────────────────────────────────────────────────┤
│                      heap.h / heap.cpp                       │
│                   kmalloc, kfree, krealloc                   │
│                  (High-level allocation API)                 │
├─────────────────────────────────────────────────────────────┤
│                   bitmap.h / bitmap.cpp                      │
│              bitmap_alloc, bitmap_free                       │
│            (Low-level page tracking bitmap)                  │
├─────────────────────────────────────────────────────────────┤
│                    Physical Memory                           │
│              2GiB identity-mapped region                     │
│              Pages: 0x100000 - 0x80000000                    │
└─────────────────────────────────────────────────────────────┘
```

## Components

### 1. Bitmap Allocator (`bitmap.h`, `bitmap.cpp`)

Low-level physical page allocator using a bitmap data structure.

**Features:**
- 1 bit per page (0 = FREE, 1 = USED)
- First-fit allocation strategy
- O(1) free, O(n) allocation
- Contiguous multi-page allocation support

**Memory Overhead:**
- 2GiB memory = 524,288 pages = 64KB bitmap

**API:**
```cpp
// Initialize bitmap (call once at startup)
void bitmap_init(void);

// Allocate single page
size_t bitmap_alloc(void);

// Allocate contiguous pages
size_t bitmap_alloc_contiguous(size_t count);

// Free pages
int bitmap_free(size_t page);
int bitmap_free_contiguous(size_t page, size_t count);

// Query functions
size_t bitmap_count_free_pages(void);
size_t bitmap_largest_free_region(void);
```

### 2. Heap Allocator (`heap.h`, `heap.cpp`)

High-level memory allocation API built on top of bitmap allocator.

**Features:**
- Page-aligned allocations (4KB minimum)
- Variable size support (automatically rounds up)
- Thread-safe via spinlock
- NULL pointer safety

**API:**
```cpp
// Initialize heap (call once, after bitmap_init)
int heap_init(void);

// Allocate memory
void* kmalloc(size_t size);

// Allocate zeroed memory
void* kcalloc(size_t nmemb, size_t size);

// Reallocate memory
void* krealloc(void* ptr, size_t new_size);

// Free memory
void kfree(void* ptr);

// Query functions
size_t heap_get_free_memory(void);
size_t heap_get_allocated_memory(void);
void heap_print_stats(void);
```

## Usage Guide

### Basic Allocation

```cpp
#include "heap.h"

// Initialize at kernel startup (after serial/vga drivers)
heap_init();

// Allocate 1KB (actually gets 4KB due to page alignment)
void* buffer = kmalloc(1024);
if (buffer == NULL) {
    // Handle out of memory
    return;
}

// Use the memory...
memset(buffer, 0, 1024);

// Free when done
kfree(buffer);
buffer = NULL;  // Good practice
```

### Zeroed Allocation

```cpp
// Allocate array of 100 integers, zeroed
int* array = (int*)kcalloc(100, sizeof(int));
if (array != NULL) {
    // array[0..99] are all zero
    kfree(array);
}
```

### Reallocation

```cpp
// Initial allocation
char* data = (char*)kmalloc(100);
memcpy(data, "Hello", 6);

// Grow to 200 bytes (data preserved)
data = (char*)krealloc(data, 200);

// Shrink to 50 bytes (first 50 bytes preserved)
data = (char*)krealloc(data, 50);

kfree(data);
```

### Large Allocations

```cpp
// 64KB allocation (16 pages)
void* large = kmalloc(64 * 1024);

// 1MB allocation (256 pages)
void* huge = kmalloc(1024 * 1024);

// Check actual allocated size
size_t actual = kmalloc_size(huge);  // Returns 1048576 (1MB)

kfree(large);
kfree(huge);
```

## Memory Model

### Physical Layout

```
0x00000000 ┌─────────────────┐
           │   Unused        │
           ├─────────────────┤
0x00100000 │ Kernel Load     │ ← PHYSICAL_MEMORY_START
           │ (16 pages used) │
           ├─────────────────┤
           │   Free Pages    │
           │   (available)   │
           │                 │
           ├─────────────────┤
0x80000000 │ End of Mapped   │ ← PHYSICAL_MEMORY_END
           └─────────────────┘

Total managed: ~2GiB (524,272 pages)
```

### Allocation Sizes

| Requested Size | Actual Allocation | Pages |
|----------------|-------------------|-------|
| 1 byte         | 4KB               | 1     |
| 1KB            | 4KB               | 1     |
| 4KB            | 4KB               | 1     |
| 4KB + 1 byte   | 8KB               | 2     |
| 8KB            | 8KB               | 2     |
| 64KB           | 64KB              | 16    |
| 1MB            | 1MB               | 256   |

### Alignment

- All allocations are page-aligned (4KB boundary)
- `kmalloc()` returns addresses divisible by 0x1000
- Suitable for DMA operations requiring page alignment

## Thread Safety

### Spinlock Protection

The heap allocator uses a global spinlock for thread safety:

```cpp
// In kmalloc():
spinlock_acquire(&g_heap_lock);
size_t page = bitmap_alloc_contiguous(pages);
spinlock_release(&g_heap_lock);
```

**Implications:**
- Multiple CPUs can safely call `kmalloc()` concurrently
- Allocations are serialized (may cause contention)
- Interrupt-safe (spinlock disables interrupts)

### Best Practices

```cpp
// ✅ SAFE: kmalloc from any context
void* ptr = kmalloc(1024);

// ✅ SAFE: kfree from any context
kfree(ptr);

// ⚠️ AVOID: Holding lock for long periods
// Keep allocations small and frequent
```

## Error Handling

### Return Values

| Function | Success | Failure |
|----------|---------|---------|
| `kmalloc(size)` | Pointer | `NULL` |
| `kcalloc(n, s)` | Pointer | `NULL` |
| `krealloc(p, s)` | Pointer | `NULL` |
| `kfree(ptr)` | void | void (safe with NULL) |

### Failure Modes

```cpp
// Out of memory
void* ptr = kmalloc(HEAP_MAX_ALLOC + 1);  // Returns NULL

// Zero size
void* ptr = kmalloc(0);  // Returns NULL

// Invalid reallocation
void* ptr = kmalloc(100);
void* new_ptr = krealloc(ptr, HEAP_MAX_ALLOC + 1);  // Returns NULL, ptr still valid
```

### Debugging

```cpp
// Print memory statistics
heap_print_stats();

// Example output:
/*
=== Heap Statistics ===
Total memory:     2047 MB
Free memory:      2000 MB (2048000 KB)
Allocated memory: 47 MB (48128 KB)
Largest block:    1500 MB (1536000 KB)
Usage:            2%
=======================
*/
```

## Limitations

### Current Implementation

1. **Minimum Allocation:** 4KB (1 page)
   - Small allocations waste memory
   - Consider slab allocator for small objects (future)

2. **No Defragmentation:**
   - Freed pages return to free pool
   - May cause fragmentation over time
   - Use `heap_get_largest_free_block()` to monitor

3. **No Memory Protection:**
   - No guard pages
   - No canaries for overflow detection
   - Use carefully and test thoroughly

4. **Maximum Allocation:** 128MB
   - Prevents single allocation from consuming all memory
   - Adjust `HEAP_MAX_ALLOC` if needed

### Future Enhancements

- [ ] Slab allocator for small objects (< 4KB)
- [ ] Memory poisoning for debugging
- [ ] Guard pages for overflow detection
- [ ] Per-CPU heaps for better SMP performance
- [ ] Defragmentation support

## Testing

### Run Memory Manager Tests

```bash
# Build and run
make run-serial

# Or with SMP testing
make run-smp-test
```

### Test Coverage

The test suite covers:
- Basic allocation/deallocation
- Multiple simultaneous allocations
- Zeroed allocation (kcalloc)
- Reallocation (krealloc)
- NULL pointer handling
- Boundary conditions
- Memory integrity
- Fragmentation handling
- Large allocations
- Bitmap allocator direct tests

### Expected Output

```
============================================
GLOBEX_OS Memory Manager Test Suite
============================================

Initializing heap...
Heap initialized successfully

--- Basic kmalloc/kfree ---
  [PASS] kmalloc(1024) returned non-NULL
  [PASS] Pointer is page-aligned
  [PASS] Write to allocated memory succeeded
  [PASS] kfree() completed without crash
  [PASS] kfree(NULL) is safe (no crash)

...

============================================
Memory Manager Test Summary
============================================
Passed: 25
Failed: 0

[MEMORY MANAGER] All tests PASSED!
============================================
```

## Troubleshooting

### Common Issues

**Problem:** `kmalloc()` always returns NULL

**Solution:**
1. Check `heap_init()` was called
2. Check memory stats: `heap_print_stats()`
3. May be out of memory

**Problem:** Memory corruption after free

**Solution:**
1. Check for double-free
2. Check for use-after-free
3. Verify pointer is from kmalloc (not stack/static)

**Problem:** Allocation too slow

**Solution:**
1. Bitmap scan is O(n) - consider caching free list
2. Large allocations take longer
3. Consider per-CPU heaps for SMP

## API Reference

### Core Functions

#### `int heap_init(void)`
Initialize the kernel heap. Must be called before any allocation.

**Returns:** 1 on success, 0 on failure

---

#### `void* kmalloc(size_t size)`
Allocate memory from kernel heap.

**Parameters:**
- `size` - Number of bytes to allocate

**Returns:** Pointer to allocated memory, or NULL on failure

---

#### `void* kcalloc(size_t nmemb, size_t size)`
Allocate zeroed memory.

**Parameters:**
- `nmemb` - Number of elements
- `size` - Size of each element

**Returns:** Pointer to zeroed memory, or NULL on failure

---

#### `void* krealloc(void* ptr, size_t new_size)`
Reallocate memory with new size.

**Parameters:**
- `ptr` - Existing allocation (or NULL)
- `new_size` - New size in bytes

**Returns:** Pointer to reallocated memory, or NULL on failure

---

#### `void kfree(void* ptr)`
Free allocated memory.

**Parameters:**
- `ptr` - Pointer to free (NULL is safe)

---

### Query Functions

#### `size_t heap_get_free_memory(void)`
Get total free memory in bytes.

---

#### `size_t heap_get_allocated_memory(void)`
Get total allocated memory in bytes.

---

#### `void heap_print_stats(void)`
Print heap statistics to serial console.

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-03-19 | Initial implementation |
| | | - Bitmap page allocator |
| | | - kmalloc/kfree API |
| | | - Thread-safe via spinlock |
| | | - Comprehensive test suite |

---

**See Also:**
- `QWEN.md` - Project context and architecture
- `CODE_QUALITY_AUDIT.md` - Quality audit findings
- `README_SMP_TESTING.md` - SMP testing guide
