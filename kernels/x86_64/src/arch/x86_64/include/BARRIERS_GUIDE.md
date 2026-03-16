# Memory Barriers Guide - GLOBEX_OS

## Overview

This guide documents the proper use of memory barriers in GLOBEX_OS kernel development.

## Barrier Types

### Compiler Barriers (Recommended for Most Cases)

x86_64 has **strong hardware memory ordering**, so compiler barriers are typically sufficient:

| Barrier | Instruction | Use Case | Cycles |
|---------|-------------|----------|--------|
| `mb()` | None (compiler only) | Full barrier - prevents all reordering | 0 |
| `rmb()` | None (compiler only) | Read barrier - for reading shared data | 0 |
| `wmb()` | None (compiler only) | Write barrier - for writing shared data | 0 |
| `barrier()` | None (compiler only) | Alias for `mb()` - see barrier() vs mb() for usage | 0 |

**barrier() vs mb() Clarification**

Use `mb()`/`rmb()`/`wmb()` for:
- ✅ SMP synchronization (shared variables between CPUs)
- ✅ Driver state that may be accessed concurrently
- ✅ Any memory that crosses CPU boundaries

Use `barrier()` for:
- ✅ Critical sections where lock already provides SMP safety
- ✅ Local state that doesn't cross CPU boundaries
- ✅ Optimization barriers in tight loops
- ✅ When you explicitly want ONLY compiler reordering prevention

**When to use:**
- SMP synchronization with spinlocks → Use `mb()`/`rmb()`/`wmb()`
- Reading/writing shared variables protected by locks → Use `mb()`/`rmb()`/`wmb()`
- Most driver state updates → Use `mb()`/`rmb()`/`wmb()`
- Local optimization in locked sections → Use `barrier()`

### Hardware Barriers (Special Cases Only)

Use hardware barriers when compiler barriers are insufficient:

| Barrier | Instruction | Use Case | Cycles |
|---------|-------------|----------|--------|
| `hw_mb()` | `lfence; sfence` | DMA, write-combining memory, MMIO | ~50-100 |
| `hw_rmb()` | `lfence` | DMA read synchronization | ~20-50 |
| `hw_wmb()` | `sfence` | Write-combining memory, DMA write | ~20-50 |

**When to use:**
- Memory-Mapped I/O (MMIO) operations
- Write-combining (WC) memory regions
- DMA buffer synchronization
- Inter-processor synchronization with weakly-ordered devices

## Usage Patterns

### Pattern 1: Shared Variable Access (SMP)

```cpp
// Writer CPU
data = 42;
wmb();  // Ensure data write happens before flag
flag = 1;

// Reader CPU
if (flag) {
    rmb();  // Ensure flag check happens before data read
    process(data);  // Guaranteed to see latest value
}
```

### Pattern 2: MMIO Operations (Hardware)

```cpp
// Writing to hardware registers
outb(port + SERIAL_DLL, divisor & 0xFF);
outb(port + SERIAL_DLM, (divisor >> 8) & 0xFF);
hw_wmb();  // Ensure writes reach hardware before continuing

// Reading from hardware registers
uint8_t status = inb(port + SERIAL_LSR);
hw_rmb();  // Ensure read completes before using value
```

### Pattern 3: Lock-Protected Access

```cpp
// Spinlock already provides memory barriers
spinlock_acquire(&lock);
// ... critical section ...
spinlock_release(&lock);
// No additional barriers needed for normal RAM access
```

### Pattern 4: Atomic Operations with Barriers

```cpp
// Atomic increment with release semantics
atomic_inc32(&counter);
wmb();  // Ensure counter update visible before signaling
flag = 1;
```

## x86_64 Memory Ordering Guarantees

x86_64 provides **strong memory ordering**:

✅ **Guaranteed:**
- Loads are NOT reordered with other loads
- Stores are NOT reordered with other stores
- Loads are NOT reordered with older stores
- Stores are NOT reordered with older loads (mostly)

❌ **May be reordered:**
- Stores MAY be reordered with older loads (rare, but possible)

## Common Mistakes

### Mistake 1: Using Hardware Barriers Unnecessarily

```cpp
// ❌ WRONG - hw_mb() is overkill for normal RAM
shared_data = 42;
hw_mb();  // ~50-100 cycles wasted
flag = 1;

// ✅ CORRECT - compiler barrier is sufficient
shared_data = 42;
wmb();  // 0 cycles, compiler only
flag = 1;
```

### Mistake 2: Missing Barriers for MMIO

```cpp
// ❌ WRONG - MMIO writes may be reordered
outb(port + SERIAL_DLL, value);
outb(port + SERIAL_LCR, config);  // May execute BEFORE DLL write!

// ✅ CORRECT - hardware barrier ensures ordering
outb(port + SERIAL_DLL, value);
hw_wmb();  // Ensure DLL write completes
outb(port + SERIAL_LCR, config);
```

### Mistake 3: Assuming volatile is Sufficient

```cpp
// ❌ WRONG - volatile only prevents compiler optimization
volatile int flag = 0;
flag = 1;  // No ordering guarantee!

// ✅ CORRECT - use barriers for ordering
int flag = 0;
flag = 1;
wmb();  // Ensures ordering
```

## Decision Tree

```
Do you need memory ordering?
│
├─ No (single-threaded code)
│   └─> No barrier needed
│
├─ Yes
    │
    ├─ Accessing MMIO or DMA buffers?
    │   ├─ Yes ──> Use hw_mb(), hw_rmb(), or hw_wmb()
    │   └─ No
    │       │
    │       ├─ Protected by spinlock?
    │       │   ├─ Yes ──> No additional barrier needed
    │       │   └─ No
    │       │       │
    │       │       ├─ Writing shared data?
    │       │       │   └─> Use wmb()
    │       │       │
    │       │       ├─ Reading shared data?
    │       │       │   └─> Use rmb()
    │       │       │
    │       │       └─ Both read and write?
    │       │           └─> Use mb()
```

## Performance Impact

| Barrier | Cycles | When to Pay the Cost |
|---------|--------|---------------------|
| `mb()`, `rmb()`, `wmb()` | 0 | Always - no runtime cost |
| `hw_mb()` | ~50-100 | Only for MMIO/DMA |
| `hw_rmb()` | ~20-50 | Only for DMA reads |
| `hw_wmb()` | ~20-50 | Only for WC memory/DMA writes |

## Migration Guide

If you find code using inconsistent barriers:

1. **Identify the access type:**
   - Normal RAM → Use compiler barriers
   - MMIO/Device registers → Use hardware barriers

2. **Replace local barrier definitions:**
   ```cpp
   // Old (remove)
   #define mb() __asm__ volatile("" ::: "memory")
   
   // New (use centralized)
   #include "barriers.h"
   ```

3. **Update comments:**
   ```cpp
   // Old
   mb();  // barrera de memoria
   
   // New
   mb();  // Full memory barrier - ensures ordering
   ```

## References

- [Intel® 64 and IA-32 Architectures Optimization Reference Manual](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [AMD64 Architecture Programmer's Manual](https://www.amd.com/en/support/tech-docs)
- [Linux Kernel Memory Barriers Documentation](https://www.kernel.org/doc/Documentation/memory-barriers.txt)
