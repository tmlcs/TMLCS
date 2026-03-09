#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * SPINLOCK API [CRIT-004 Phase 2: SMP Preparation]
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
 * SMP SAFETY:
 *   - Safe for multi-processor systems
 *   - Disables interrupts on local CPU during acquire (prevents deadlock)
 *   - Memory barriers ensure proper ordering
 * 
 * Current status: Implementation ready, UP-tested
 * Tracking issue: #SMP-001
 * =============================================================================
 */

/* Spinlock structure - must be 64-bit aligned for atomic access */
typedef struct {
    volatile uint64_t locked;  /* 0 = unlocked, 1 = locked */
} spinlock_t;

/* Static initializer for spinlocks - C++17 compatible */
#define SPINLOCK_INIT { 0 }

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
#define SPINLOCK_GUARD(lock) \
    spinlock_acquire(lock); \
    __attribute__((cleanup(spinlock_release_guard))) spinlock_t* _guard_lock = lock

/* Helper function for cleanup attribute */
static inline void spinlock_release_guard(spinlock_t** lock) {
    if (*lock) {
        spinlock_release(*lock);
    }
}

/* =============================================================================
 * VGA Driver Lock [CRIT-004]
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
