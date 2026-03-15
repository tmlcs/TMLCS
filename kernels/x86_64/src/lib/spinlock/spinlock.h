#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * SPINLOCK API
 * =============================================================================
 *
 * Basic spinlock implementation for x86_64 using atomic instructions.
 * This provides mutual exclusion for SMP environments.
 *
 * Usage:
 *   spinlock_t my_lock = SPINLOCK_INIT;
 *
 *   spinlock_acquire(&my_lock);
 *   // ... critical section ...
 *   spinlock_release(&my_lock);
 *
 * Implementation:
 *   Uses LOCK CMPXCHG instruction for atomic compare-and-swap.
 *   Spinlock is a simple ticket lock for fairness.
 *
 *   Previous implementation had a fast path that acquired the lock without
 *   disabling interrupts. This could cause deadlock in nested interrupt context.
 *   The fast path has been removed. Now ALWAYS disables interrupts during acquire.
 *
 * SMP SAFETY:
 *   - Safe for multi-processor systems
 *   - ALWAYS disables interrupts on local CPU during acquire (prevents deadlock)
 *   - Re-enables interrupts on release (restores previous state)
 *   - Memory barriers ensure proper ordering
 *   - No fast path - interrupt safety guaranteed in all cases
 *
 * Interrupt handling:
 *   - spinlock_acquire() ALWAYS saves and disables interrupts
 *   - spinlock_release() RESTORES interrupt state to what it was before acquire
 *   - This prevents permanent interrupt disablement and nested deadlock
 *
 * DEADLOCK PREVENTION:
 *   The previous fast path could cause this deadlock scenario:
 *     1. CPU acquires lock via fast path (interrupts enabled)
 *     2. Interrupt occurs on same CPU
 *     3. Interrupt handler tries to acquire same lock
 *     4. Handler spins forever (lock held, interrupts disabled in slow path)
 *     5. Original code never resumes -> DEADLOCK
 *
 *   Fix: Always disable interrupts eliminates this scenario completely.
 *
 * Current status: Implementation ready, SMP-safe
 * =============================================================================
 */

/* Spinlock structure - must be 64-bit aligned for atomic access
 * Added interrupts_enabled field to track interrupt state
 * interrupts_enabled must be volatile - it's written during
 * acquire and read during release, potentially across different CPUs in SMP.
 * Without volatile, compiler may cache the value and not see updates.
 */
typedef struct {
    volatile uint64_t locked;         /* 0 = unlocked, 1 = locked */
    volatile bool interrupts_enabled; /* Track interrupt state for restore */
} spinlock_t;

/* Static initializer for spinlocks - C++17 compatible
 * Initialize both locked and interrupts_enabled fields
 */
#define SPINLOCK_INIT                                                                              \
    { 0, false }

/* =============================================================================
 * Core API
 * =============================================================================
 */

/**
 * Initialize a spinlock
 * @param lock Pointer to spinlock
 */
void spinlock_init(spinlock_t* lock);

/**
 * Acquire spinlock (blocking)
 * Disables interrupts on local CPU to prevent deadlock
 * @param lock Pointer to spinlock
 */
void spinlock_acquire(spinlock_t* lock);

/**
 * Try to acquire spinlock (non-blocking)
 * @param lock Pointer to spinlock
 * @return true if acquired, false if already locked
 */
bool spinlock_try_acquire(spinlock_t* lock);

/**
 * Release spinlock
 * Re-enables interrupts on local CPU
 * @param lock Pointer to spinlock
 */
void spinlock_release(spinlock_t* lock);

/* =============================================================================
 * Convenience Macros
 * =============================================================================
 */

/* Guard macro for automatic release (scope-based) */
#define SPINLOCK_GUARD(lock)                                                                       \
    spinlock_acquire(lock);                                                                        \
    __attribute__((cleanup(spinlock_release_guard))) spinlock_t* _guard_lock = lock

/* Helper function for cleanup attribute */
static inline void spinlock_release_guard(spinlock_t** lock) {
    if (*lock) {
        spinlock_release(*lock);
    }
}

/* =============================================================================
 * VGA Driver Lock
 * =============================================================================
 * Global spinlock for protecting VGA text mode operations.
 * Declared in spinlock.cpp
 * =============================================================================
 */

/**
 * Acquire VGA lock (for print driver)
 */
void vga_lock(void);

/**
 * Release VGA lock
 */
void vga_unlock(void);

/**
 * Try to acquire VGA lock (non-blocking)
 */
bool vga_try_lock(void);

#ifdef __cplusplus
}
#endif

#endif /* SPINLOCK_H */
