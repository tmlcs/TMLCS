#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * SPINLOCK TOKEN API
 * =============================================================================
 *
 * Non-reentrant interrupt-disabling CAS spinlock for x86_64.
 *
 * Acquire captures the full RFLAGS register (including IF) into an opaque
 * spinlock_token_t on the caller's stack frame BEFORE cli. Release checks
 * bit 9 (IF) of the saved RFLAGS and conditionally restores sti. This keeps
 * the interrupt-state decision per-CPU and per-call, eliminating:
 *   - STI inside exception/IRQ handlers (CRIT-1)
 *   - SMP race on the saved flag field (CRIT-2/3)
 *
 * Usage:
 *   spinlock_t my_lock = SPINLOCK_INIT;
 *
 *   spinlock_token_t tok = spinlock_acquire(&my_lock);
 *   // ... critical section ...
 *   spinlock_release(&my_lock, tok);
 *
 * Always initialise global/static locks with = SPINLOCK_INIT.
 * Never acquire the same lock recursively — spins forever with IF=0.
 *
 * =============================================================================
 * MED-002 FIX: DEADLOCK PREVENTION GUIDE
 * =============================================================================
 *
 * COMMON DEADLOCK SCENARIOS AND HOW TO AVOID THEM:
 *
 * ❌ WRONG: Recursive acquisition (same CPU)
 * ─────────────────────────────────────────────
 *   spinlock_lock(&lock);
 *   // ... do something ...
 *   spinlock_lock(&lock);  // DEADLOCK! Spins forever with interrupts disabled
 *
 *   WHY: The lock is already held by this CPU. The second acquisition tries
 *        to CAS from 0→1, but the lock is already 1. Since interrupts are
 *        disabled, nothing can release the lock. CPU spins forever.
 *
 * ✅ CORRECT: Use a single acquisition for the entire critical section
 * ─────────────────────────────────────────────────────────────────────
 *   spinlock_lock(&lock);
 *   // ... do all work while holding lock ...
 *   spinlock_unlock(&lock);
 *
 *
 * ❌ WRONG: Calling code that acquires the same lock
 * ───────────────────────────────────────────────────
 *   spinlock_lock(&g_serial_lock);
 *   LOG_INFO("Serial status: %d", status);  // DEADLOCK RISK!
 *   spinlock_unlock(&g_serial_lock);
 *
 *   WHY: LOG_INFO() internally acquires g_log_lock, which may call serial
 *        output functions that try to acquire g_serial_lock. Even if not,
 *        any function call while holding a lock is risky.
 *
 * ✅ CORRECT: Minimize critical section, call external functions outside
 * ───────────────────────────────────────────────────────────────────────
 *   // Gather data while holding lock (fast, no external calls)
 *   spinlock_lock(&g_serial_lock);
 *   int local_status = g_serial_state.status;
 *   spinlock_unlock(&g_serial_lock);
 *
 *   // Now safe to call external functions
 *   LOG_INFO("Serial status: %d", local_status);
 *
 *
 * ❌ WRONG: Holding lock across blocking operations
 * ─────────────────────────────────────────────────
 *   spinlock_lock(&lock);
 *   pit_wait_ms(100);  // Blocks for 100ms while holding lock!
 *   spinlock_unlock(&lock);
 *
 *   WHY: Other CPUs spin waiting for the lock, wasting CPU cycles and
 *        potentially causing timeout failures in time-sensitive code.
 *
 * ✅ CORRECT: Never block while holding a spinlock
 * ────────────────────────────────────────────────
 *   // Copy data, release lock, then wait
 *   spinlock_lock(&lock);
 *   void* local_copy = data;
 *   spinlock_unlock(&lock);
 *   pit_wait_ms(100);  // Safe - lock is released
 *
 *
 * =============================================================================
 * LOCK ORDERING (to prevent ABBA deadlocks in SMP)
 * =============================================================================
 * When acquiring multiple locks, always acquire in this order:
 *
 *   1. g_slab_lock    (slab allocator)
 *   2. g_heap_lock    (bitmap/heap allocator)
 *   3. g_log_lock     (logging system)
 *   4. g_vga_lock     (VGA driver)
 *   5. g_serial_lock  (serial driver)
 *
 * Example of correct ordering:
 *   spinlock_lock(&g_slab_lock);
 *   // ... slab work ...
 *   spinlock_lock(&g_heap_lock);
 *   // ... work needing both locks ...
 *   spinlock_unlock(&g_heap_lock);
 *   spinlock_unlock(&g_slab_lock);
 *
 * NEVER acquire in reverse order (heap then slab) - ABBA deadlock!
 * =============================================================================
 */

/** Opaque token returned by spinlock_acquire. Holds the RFLAGS snapshot
 *  captured before cli. Pass it unmodified to spinlock_release. */
typedef struct {
    uint64_t rflags;
} spinlock_token_t;

/** Spinlock structure. Only field is the CAS word. */
typedef struct {
    volatile uint64_t locked; /* 0 = unlocked, 1 = locked */
} spinlock_t;

/** Static initializer — compatible with C and C++ aggregate init. */
#define SPINLOCK_INIT                                                                              \
    { 0 }

/* =============================================================================
 * Core API
 * =============================================================================
 */

void spinlock_init(spinlock_t* lock);
spinlock_token_t spinlock_acquire(spinlock_t* lock);
bool spinlock_try_acquire(spinlock_t* lock, spinlock_token_t* out_tok);
void spinlock_release(spinlock_t* lock, spinlock_token_t tok);

/* =============================================================================
 * VGA Driver Lock (global instance lives in spinlock.cpp)
 * =============================================================================
 */

spinlock_token_t vga_lock(void);
void vga_unlock(spinlock_token_t tok);
bool vga_try_lock(spinlock_token_t* out_tok);

/* =============================================================================
 * Serial Driver Lock (global instance lives in spinlock.cpp)
 * =============================================================================
 */

spinlock_token_t serial_lock(void);
void serial_unlock(spinlock_token_t tok);
bool serial_try_lock(spinlock_token_t* out_tok);
void serial_force_unlock(void);

/** Reset g_vga_lock to unlocked state without restoring IF.
 *  For use ONLY at the top of default_exception_handler(). */
void vga_force_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* SPINLOCK_H */
