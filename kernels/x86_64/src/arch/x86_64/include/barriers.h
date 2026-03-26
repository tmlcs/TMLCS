#ifndef ARCH_BARRIERS_H
#define ARCH_BARRIERS_H

/**
 * @file barriers.h
 * @brief Memory barrier primitives for x86_64
 *
 * Memory barriers ensure proper ordering of memory operations
 * in SMP and device driver contexts. They prevent both compiler
 * and CPU reordering that could cause race conditions.
 *
 * @note Centralized barrier definitions
 *       Previously, each driver defined its own barriers inconsistently.
 *       This header provides a single source of truth for all barriers.
 *
 * @see https://www.kernel.org/doc/Documentation/memory-barriers.txt
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* =============================================================================
 * Compiler Memory Barriers
 * =============================================================================
 * These barriers prevent compiler reordering but do not emit CPU instructions.
 * x86_64 has strong hardware memory ordering, so compiler barriers are
 * typically sufficient for most use cases.
 *
 * MED-004: Usage Guidelines
 * -------------------------
 * Use mb()/rmb()/wmb() for:
 *   - SMP synchronization (shared variables between CPUs)
 *   - Driver state that may be accessed concurrently
 *   - Any memory that crosses CPU boundaries
 *
 * Use barrier() for:
 *   - Critical sections where lock already provides SMP safety
 *   - Local state that doesn't cross CPU boundaries
 *   - Optimization barriers in tight loops
 *   - When you explicitly want ONLY compiler reordering prevention
 *
 * Use hw_mb()/hw_rmb()/hw_wmb() for:
 *   - MMIO (Memory-Mapped I/O) operations
 *   - DMA buffer synchronization
 *   - Write-combining (WC) memory regions
 * =============================================================================
 */

/**
 * @brief Full compiler memory barrier
 * @note Prevents compiler from reordering loads and stores across the barrier
 * @note Does NOT emit any CPU instruction (0 cycles)
 * @note Use when hardware ordering is sufficient but compiler may reorder
 *
 * @assembly
 *   Instruction: "" (empty inline assembly)
 *   Clobbers: "memory" - tells compiler memory may be modified
 *   Effects: Compiler must assume any memory location may have changed
 *   Cycles: 0 (compiler directive only)
 *
 * @example
 *   // Ensure write to flag happens after write to data
 *   data = 42;
 *   mb();  // Compiler barrier
 *   flag = 1;  // Guaranteed to happen after data write (compiler order)
 */
#define mb() __asm__ volatile("" ::: "memory")

/**
 * @brief Read compiler memory barrier
 * @note Prevents compiler from reordering loads across the barrier
 * @note Does NOT emit any CPU instruction
 * @note Use when reading shared data that may be modified by another CPU
 *
 * @assembly
 *   Instruction: "" (empty inline assembly)
 *   Clobbers: "memory"
 *   Effects: All loads before barrier complete before loads after barrier
 *   Cycles: 0 (compiler directive only)
 *
 * @example
 *   // Ensure we see latest data before checking flag
 *   rmb();  // Compiler barrier
 *   if (flag) {
 *       // data is guaranteed to be read after flag check (compiler order)
 *       process(data);
 *   }
 */
#define rmb() __asm__ volatile("" ::: "memory")

/**
 * @brief Write compiler memory barrier
 * @note Prevents compiler from reordering stores across the barrier
 * @note Does NOT emit any CPU instruction
 * @note Use when writing shared data that may be read by another CPU
 *
 * @assembly
 *   Instruction: "" (empty inline assembly)
 *   Clobbers: "memory"
 *   Effects: All stores before barrier complete before stores after barrier
 *   Cycles: 0 (compiler directive only)
 *
 * @example
 *   // Ensure data is written before flag signals completion
 *   data = 42;
 *   wmb();  // Compiler barrier
 *   flag = 1;  // Guaranteed to happen after data write (compiler order)
 */
#define wmb() __asm__ volatile("" ::: "memory")

/**
 * @brief Compiler barrier only (alias for mb())
 * @note Same as mb() - provided for code clarity in specific contexts
 *
 * MED-004 FIX: Usage clarification
 *   - Use mb()/rmb()/wmb() for SMP memory ordering (shared variables)
 *   - Use barrier() ONLY for compiler reordering prevention in:
 *     * Critical sections where lock already provides SMP safety
 *     * Local state that doesn't cross CPU boundaries
 *     * Optimization barriers in tight loops
 *
 * @example
 *   // ✅ CORRECT: barrier() for local optimization
 *   spinlock_token_t tok = spinlock_acquire(&lock);  // Lock provides SMP safety
 *   local_var = compute();    // No mb() needed - lock is barrier
 *   barrier();                // Prevent compiler reordering only
 *   use(local_var);
 *   spinlock_release(&lock, tok);
 *
 * @example
 *   // ❌ WRONG: barrier() for SMP shared variable
 *   shared_flag = 1;
 *   barrier();  // WRONG - should be wmb() for SMP
 *
 * @see mb() for full SMP memory barrier
 */
#define barrier() __asm__ volatile("" ::: "memory")

/* =============================================================================
 * Hardware Memory Barriers (LFENCE/SFENCE/MFENCE)
 * =============================================================================
 * These barriers emit actual CPU instructions to enforce ordering at the
 * hardware level. Use when compiler barriers are insufficient (e.g., for
 * write-combining memory, DMA operations, or weakly-ordered devices).
 *
 * @note x86_64 has strong memory ordering:
 *   - Loads are not reordered with other loads
 *   - Stores are not reordered with other stores
 *   - Loads are not reordered with older stores
 *   - Stores MAY be reordered with older loads (rare)
 *
 * Therefore, mfence is typically only needed for:
 *   - Write-combining (WC) memory regions
 *   - DMA buffer synchronization
 *   - Inter-processor synchronization with weakly-ordered devices
 * =============================================================================
 */

/**
 * @brief Full hardware memory barrier (LFENCE + SFENCE)
 * @note Emits LFENCE and SFENCE instructions
 * @note Orders both loads and stores at hardware level
 * @note Use for DMA, write-combining memory, or device MMIO
 *
 * @assembly
 *   Instructions: lfence; sfence
 *   Effects:
 *     - LFENCE: All prior loads complete before later loads
 *     - SFENCE: All prior stores complete before later stores
 *   Cycles: ~50-100 (depends on CPU and pending operations)
 *
 * @note For most SMP synchronization, compiler barriers (mb/rmb/wmb) are sufficient
 * @note Use hw_mb() only when hardware ordering is explicitly required
 *
 * @see https://www.felixcloutier.com/x86/lfence
 * @see https://www.felixcloutier.com/x86/sfence
 */
static inline void hw_mb(void) {
    __asm__ volatile("lfence; sfence" ::: "memory");
}

/**
 * @brief Read hardware memory barrier (LFENCE)
 * @note Emits LFENCE instruction
 * @note Orders loads at hardware level
 * @note Use for DMA read synchronization
 *
 * @assembly
 *   Instruction: lfence
 *   Effects: All prior loads complete before later loads
 *   Cycles: ~20-50 (depends on CPU and pending loads)
 *
 * @see https://www.felixcloutier.com/x86/lfence
 */
static inline void hw_rmb(void) {
    __asm__ volatile("lfence" ::: "memory");
}

/**
 * @brief Write hardware memory barrier (SFENCE)
 * @note Emits SFENCE instruction
 * @note Orders stores at hardware level
 * @note Use for write-combining memory or DMA write synchronization
 *
 * @assembly
 *   Instruction: sfence
 *   Effects: All prior stores complete before later stores
 *   Cycles: ~20-50 (depends on CPU and pending stores)
 *
 * @see https://www.felixcloutier.com/x86/sfence
 */
static inline void hw_wmb(void) {
    __asm__ volatile("sfence" ::: "memory");
}

/* =============================================================================
 * Acquire/Release Semantics
 * =============================================================================
 * These macros provide acquire (load) and release (store) semantics for
 * lock-free synchronization. They are lighter-weight than full barriers.
 *
 * Acquire: Ensures subsequent operations happen after the acquire
 * Release: Ensures prior operations happen before the release
 * =============================================================================
 */

/**
 * @brief Load-acquire barrier (for lock acquisition)
 * @note Ensures subsequent loads/stores happen after this load
 * @note Use when acquiring a lock or reading a synchronization variable
 *
 * @note On x86_64, loads already have acquire semantics, so this is
 *       primarily a compiler barrier. The CPU guarantees:
 *       - Loads are not reordered with older stores
 *       - Loads are not reordered with other loads
 *
 * @example
 *   // Spinlock acquire pattern
 *   while (atomic_load_acquire(&lock->locked)) {
 *       pause();  // Spin with PAUSE instruction
 *   }
 *   // Critical section - all operations happen after acquire
 */
#define smp_load_acquire(p)                                                                        \
    ({                                                                                             \
        typeof(*(p)) val;                                                                          \
        val = *(p);                                                                                \
        rmb(); /* Ensure subsequent ops happen after load */                                       \
        val;                                                                                       \
    })

/**
 * @brief Store-release barrier (for lock release)
 * @note Ensures prior loads/stores happen before this store
 * @note Use when releasing a lock or writing a synchronization variable
 *
 * @note On x86_64, stores already have release semantics, so this is
 *       primarily a compiler barrier. The CPU guarantees:
 *       - Stores are not reordered with other stores
 *       - Stores are not reordered with older loads
 *
 * @example
 *   // Spinlock release pattern
 *   // Critical section - all operations happen before release
 *   smp_store_release(&lock->locked, 0);  // Release lock
 */
#define smp_store_release(p, v)                                                                    \
    do {                                                                                           \
        wmb(); /* Ensure prior ops happen before store */                                          \
        *(p) = (v);                                                                                \
    } while (0)

/* =============================================================================
 * PAUSE Instruction for Spin Loops
 * =============================================================================
 * The PAUSE instruction improves spinlock performance by:
 *   - Preventing pipeline stalls from false memory dependencies
 *   - Reducing power consumption during tight spin loops
 *   - Improving hyperthreading performance (yields execution to sibling thread)
 * =============================================================================
 */

/**
 * @brief PAUSE instruction for spin loops
 * @note Emits PAUSE instruction (SSE2)
 * @note Use in spinlock wait loops to improve performance
 *
 * @assembly
 *   Instruction: pause
 *   Effects:
 *     - Delays execution of next instruction
 *     - Prevents false dependency detection
 *     - Reduces power consumption
 *   Cycles: ~10-20 (varies by CPU)
 *
 * @note Without PAUSE, spinlocks can cause:
 *   - Pipeline stalls
 *   - Higher power consumption
 *   - Hyperthreading starvation
 *
 * @see https://www.felixcloutier.com/x86/pause
 *
 * @example
 *   while (lock->locked) {
 *       cpu_pause();  // Spin efficiently
 *   }
 */
static inline void cpu_pause(void) {
    __asm__ volatile("pause" ::: "memory");
}

/* =============================================================================
 * Cache Line Alignment for SMP
 * =============================================================================
 * Cache line size for x86_64 is typically 64 bytes.
 * Use this to prevent false sharing in SMP systems.
 * =============================================================================
 */

/** Cache line size in bytes (x86_64 typical) */
#define CACHE_LINE_SIZE 64

/**
 * @brief Align variable to cache line boundary
 * @note Prevents false sharing in SMP systems
 *
 * @example
 *   // Per-CPU data - prevent false sharing
 *   struct alignas(CACHE_LINE_SIZE) cpu_data {
 *       uint32_t counter;
 *       // ... other per-CPU data
 *   };
 */
#define CACHE_ALIGNED __attribute__((aligned(CACHE_LINE_SIZE)))

/* =============================================================================
 * Prefetch Hints
 * =============================================================================
 * Prefetch instructions can improve performance by loading data into
 * cache before it's needed. Use with care - incorrect prefetching
 * can hurt performance.
 * =============================================================================
 */

/**
 * @brief Prefetch data into cache (read hint)
 * @param ptr Pointer to data to prefetch
 *
 * @assembly
 *   Instruction: prefetcht0 [ptr]
 *   Effects: Loads data into L1 cache (temporal hint T0)
 *   Cycles: Asynchronous (doesn't block execution)
 *
 * @note Use when you know data will be accessed soon
 * @note Prefetch is a hint - CPU may ignore it
 *
 * @see https://www.felixcloutier.com/x86/prefetch
 */
static inline void prefetch_read(const void* ptr) {
    __asm__ volatile("prefetcht0 %0" ::"m"(*(const char*) ptr));
}

/**
 * @brief Prefetch data into cache (write hint)
 * @param ptr Pointer to data to prefetch for writing
 *
 * @assembly
 *   Instruction: prefetchw [ptr]
 *   Effects: Loads data into cache with write intent
 *   Cycles: Asynchronous (doesn't block execution)
 *
 * @note Use when you know data will be modified soon
 * @note 3DNow! extension, widely supported on AMD and modern Intel
 *
 * @see https://www.felixcloutier.com/x86/prefetch
 */
static inline void prefetch_write(void* ptr) {
    __asm__ volatile("prefetchw %0" ::"m"(*(char*) ptr));
}

#ifdef __cplusplus
}
#endif

#endif /* ARCH_BARRIERS_H */
